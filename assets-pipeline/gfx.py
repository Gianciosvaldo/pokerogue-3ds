"""Images -> RRT textures for the 3DS.

Every upstream PNG becomes one RRT entry. What it holds:
  - TexturePacker atlases (PNG + JSON with frames): frames are de-duplicated and
    repacked into power-of-two pages no larger than 1024x1024 (the PICA200 limit),
    with a 1 px transparent gutter so scaled/rotated draws never bleed.
  - battle_anims/*.png: upstream (loadAnimAssets in battle-anims.ts) loads an
    animation's graphic as a 96x96 spritesheet, so each cell becomes a trimmed
    frame named "0", "1", ... (empty cells keep a 0x0 frame). The images named by
    AnimTimedAddBgEvent are backgrounds, loaded whole (loadImage) and drawn as a
    tile sprite, so they get one whole-image frame like any other PNG. The two
    names used both ways get the cells plus a whole-image frame named "__BASE"
    (Phaser's name for the whole texture), appended after the cells.
  - Any other PNG: one frame covering the whole image. Images wider or taller
    than 1024 px are cut into a grid of pages, one frame per tile, and every tile
    frame carries the full image size as its source size, so drawing all frames
    at the same position with the same options reproduces the image.

Pixels are stored as 8-bit palette indices when the image has <= 255 visible
colors (every Pokemon sheet does; the max seen is 178). The palette keeps the
exact upstream 8-bit colors, which matters because shiny variants and fusions
recolor by exact color match: the app recolors the palette, then converts it to
the GPU format. Images with more colors are stored as ready-to-upload texels.

Page data is stored already swizzled in PICA200 8x8 Morton tile order, so the
app's loader is a straight table lookup (indexed) or memcpy (direct).

RRT layout (little endian), see rrt.h for the C side:
  "RRT1" u8 kind(0 indexed, 1 direct) u8 gpu(0 RGBA5551, 1 RGBA8)
  u16 pages u16 frames u16 palette_count u16 img_w u16 img_h u32 names_size
  palette   palette_count x u32 0xRRGGBBAA (index 0 is always transparent)
  pages     pages x {u16 w, u16 h}                    (power-of-two sizes)
  frames    frames x {u16 page, u16 x, u16 y, u16 w, u16 h,
                      i16 off_x, i16 off_y, u16 src_w, u16 src_h, u16 name_off}
  names     names_size bytes of NUL-terminated frame names
  (pad to 4 bytes)
  texels    per page: w*h bytes (indexed) or w*h*2 (RGBA5551) / w*h*4 (RGBA8)
"""
import hashlib
import json
import os
import struct

import numpy as np
from PIL import Image

MAX_PAGE = 1024
KIND_INDEXED, KIND_DIRECT = 0, 1
GPU_5551, GPU_RGBA8 = 0, 1
SPRITESHEETS = {"battle_anims": 96}  # folder -> cell size (scene-base.ts loadSpritesheet callers)
ANIM_BG_EVENT = "AnimTimedAddBgEvent"  # the only event whose resourceName upstream loads as an image

# ---------------------------------------------------------------- tiling

_tile_cache = {}


def _tiled_order(w, h):
    """perm such that tiled[perm] = raster.ravel() for a w x h PICA texture."""
    key = (w, h)
    if key not in _tile_cache:
        y, x = np.mgrid[0:h, 0:w].astype(np.uint32)
        m = (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3)
        idx = (((y >> 3) * (w >> 3) + (x >> 3)) << 6) + m
        _tile_cache[key] = idx.ravel()
    return _tile_cache[key]


def swizzle(plane):
    """plane: (h, w[, c]) raster -> same data in tile order, flattened."""
    h, w = plane.shape[:2]
    out = np.empty_like(plane.reshape(h * w, *plane.shape[2:]))
    out[_tiled_order(w, h)] = plane.reshape(h * w, *plane.shape[2:])
    return out


def pow2(v):
    p = 8
    while p < v:
        p *= 2
    return p


# ---------------------------------------------------------------- packing


class MaxRects:
    """MaxRects bin packer, best-short-side-fit."""

    def __init__(self, w, h):
        self.free = [(0, 0, w, h)]

    def insert(self, w, h):
        best, bs, bl = None, None, None
        for fx, fy, fw, fh in self.free:
            if w <= fw and h <= fh:
                s, l = min(fw - w, fh - h), max(fw - w, fh - h)
                if best is None or (s, l) < (bs, bl):
                    best, bs, bl = (fx, fy), s, l
        if best is None:
            return None
        x, y = best
        self._split(x, y, w, h)
        return best

    def _split(self, x, y, w, h):
        out = []
        for f in self.free:
            fx, fy, fw, fh = f
            if x >= fx + fw or x + w <= fx or y >= fy + fh or y + h <= fy:
                out.append(f)
                continue
            if x > fx:
                out.append((fx, fy, x - fx, fh))
            if x + w < fx + fw:
                out.append((x + w, fy, fx + fw - x - w, fh))
            if y > fy:
                out.append((fx, fy, fw, y - fy))
            if y + h < fy + fh:
                out.append((fx, y + h, fw, fy + fh - y - h))
        # drop rects contained in others
        out = list(set(out))
        keep = []
        for i, a in enumerate(out):
            ax, ay, aw, ah = a
            contained = False
            for j, b in enumerate(out):
                if i != j:
                    bx, by, bw, bh = b
                    if bx <= ax and by <= ay and ax + aw <= bx + bw and ay + ah <= by + bh and (a != b or i > j):
                        contained = True
                        break
            if not contained:
                keep.append(a)
        self.free = keep


def _try_pack(sizes, W, H):
    """sizes: list of (w, h) incl. gutter. Returns positions or None."""
    order = sorted(range(len(sizes)), key=lambda i: (-sizes[i][1], -sizes[i][0]))
    mr = MaxRects(W + 1, H + 1)  # +1: the gutter of the last row/column may hang off the page
    pos = [None] * len(sizes)
    for i in order:
        p = mr.insert(*sizes[i])
        if p is None:
            return None
        pos[i] = p
    return pos


_CANDIDATES = sorted(
    [(w, h) for w in (8, 16, 32, 64, 128, 256, 512, 1024) for h in (8, 16, 32, 64, 128, 256, 512, 1024)],
    key=lambda s: (s[0] * s[1], abs(s[0] - s[1])),
)


def pack_pages(sizes):
    """Pack (w, h) rects (already including the 1 px gutter) into as few, as small
    power-of-two pages as possible. Returns ([(W, H)], [(page, x, y)])."""
    if not sizes:
        return [(8, 8)], []
    area = sum(w * h for w, h in sizes)
    mw = max(w for w, _ in sizes) - 1
    mh = max(h for _, h in sizes) - 1
    for W, H in _CANDIDATES:
        if W * H < area * 0.9 or W < mw or H < mh:
            continue
        pos = _try_pack(sizes, W, H)
        if pos:
            return [(W, H)], [(0, x, y) for x, y in pos]
    # more than one full page: fill 1024x1024 pages greedily, then shrink each
    remaining = sorted(range(len(sizes)), key=lambda i: (-sizes[i][1], -sizes[i][0]))
    groups = []
    while remaining:
        mr = MaxRects(MAX_PAGE + 1, MAX_PAGE + 1)
        placed, left = [], []
        for i in remaining:
            (placed if mr.insert(*sizes[i]) else left).append(i)
        if not placed:
            raise ValueError("frame larger than 1024 px")
        groups.append(placed)
        remaining = left
    pages, result = [], [None] * len(sizes)
    for g in groups:
        sub_pages, sub_pos = pack_pages([sizes[i] for i in g])
        assert len(sub_pages) == 1
        for i, (_, x, y) in zip(g, sub_pos):
            result[i] = (len(pages), x, y)
        pages.append(sub_pages[0])
    return pages, result


# ---------------------------------------------------------------- encoding


def _palette_of(rgba):
    """Visible colors -> (palette uint32 array with [0] = transparent, index plane) or None if > 255."""
    flat = rgba.reshape(-1, 4)
    vis = flat[:, 3] > 0
    packed = (
        (flat[:, 0].astype(np.uint32) << 24)
        | (flat[:, 1].astype(np.uint32) << 16)
        | (flat[:, 2].astype(np.uint32) << 8)
        | flat[:, 3].astype(np.uint32)
    )
    cols, inv = np.unique(packed[vis], return_inverse=True)
    if len(cols) > 255:
        return None
    idx = np.zeros(len(flat), np.uint8)
    idx[vis] = (inv + 1).astype(np.uint8)
    pal = np.concatenate([np.zeros(1, np.uint32), cols.astype(np.uint32)])
    return pal, idx.reshape(rgba.shape[:2])


def _has_soft_alpha(rgba):
    a = rgba[..., 3]
    return bool(((a > 0) & (a < 255)).any())


def _direct_texels(rgba, gpu):
    r, g, b, a = (rgba[..., i].astype(np.uint32) for i in range(4))
    if gpu == GPU_5551:
        v = ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | (a > 0)
        return v.astype("<u2")
    return ((r << 24) | (g << 16) | (b << 8) | a).astype("<u4")


def encode(rgba_full, frames, names):
    """rgba_full: HxWx4 uint8 of the source image.
    frames: list of (sx, sy, w, h, off_x, off_y, src_w, src_h) rects in rgba_full.
    Returns RRT bytes and stats dict."""
    H, W = rgba_full.shape[:2]
    # a few upstream JSONs have frames that run 1 px past the image edge; pad with transparency
    need_w = max([W] + [sx + w for (sx, sy, w, h, *_r) in frames])
    need_h = max([H] + [sy + h for (sx, sy, w, h, *_r) in frames])
    if need_w > W or need_h > H:
        padded = np.zeros((need_h, need_w, 4), np.uint8)
        padded[:H, :W] = rgba_full
        rgba_full = padded
    pal_idx = _palette_of(rgba_full)
    soft = _has_soft_alpha(rgba_full)
    gpu = GPU_RGBA8 if soft else GPU_5551
    kind = KIND_INDEXED if pal_idx is not None else KIND_DIRECT
    plane = pal_idx[1] if kind == KIND_INDEXED else rgba_full

    # de-duplicate identical frame contents
    uniq, uniq_of = {}, []
    for (sx, sy, w, h, *_rest) in frames:
        if w <= 0 or h <= 0:
            uniq_of.append(-1)
            continue
        k = hashlib.blake2b(plane[sy : sy + h, sx : sx + w].tobytes() + struct.pack("<HH", w, h), digest_size=16).digest()
        if k not in uniq:
            uniq[k] = (sx, sy, w, h)
        uniq_of.append(k)
    keys = list(uniq)
    kpos = {k: i for i, k in enumerate(keys)}
    rects = [uniq[k] for k in keys]
    pages, pos = pack_pages([(w + 1, h + 1) for (_, _, w, h) in rects])

    page_planes = []
    for PW, PH in pages:
        if kind == KIND_INDEXED:
            page_planes.append(np.zeros((PH, PW), np.uint8))
        else:
            page_planes.append(np.zeros((PH, PW, 4), np.uint8))
    for (sx, sy, w, h), (p, x, y) in zip(rects, pos):
        page_planes[p][y : y + h, x : x + w] = plane[sy : sy + h, sx : sx + w]

    name_blob = bytearray()
    frec = []
    for (sx, sy, w, h, ox, oy, sw, sh), k, name in zip(frames, uniq_of, names):
        noff = len(name_blob)
        name_blob += name.encode("utf-8") + b"\0"
        if k == -1:
            frec.append(struct.pack("<HHHHHhhHHH", 0, 0, 0, 0, 0, ox, oy, sw, sh, noff))
        else:
            p, x, y = pos[kpos[k]]
            frec.append(struct.pack("<HHHHHhhHHH", p, x, y, w, h, ox, oy, sw, sh, noff))

    pal = pal_idx[0] if kind == KIND_INDEXED else np.zeros(0, np.uint32)
    out = bytearray(
        struct.pack("<4sBBHHHHHI", b"RRT1", kind, gpu, len(pages), len(frames), len(pal), W, H, len(name_blob))
    )
    out += pal.astype("<u4").tobytes()
    for PW, PH in pages:
        out += struct.pack("<HH", PW, PH)
    out += b"".join(frec)
    out += name_blob
    out += b"\0" * (-len(out) & 3)
    tex_bytes = 0
    for pp, (PW, PH) in zip(page_planes, pages):
        if kind == KIND_INDEXED:
            out += swizzle(pp).tobytes()
        else:
            out += swizzle(_direct_texels(pp, gpu)).tobytes()
        tex_bytes += PW * PH * (4 if gpu == GPU_RGBA8 else 2)
    return bytes(out), {"pages": pages, "kind": kind, "gpu": gpu, "tex_bytes": tex_bytes, "frames": len(frames),
                        "unique": len(rects)}


# ---------------------------------------------------------------- sources


def load_rgba(path):
    im = Image.open(path)
    im.load()
    return np.asarray(im.convert("RGBA"))


def atlas_frames(js):
    tex = js["textures"][0] if "textures" in js else js
    fr = tex["frames"]
    items = [(f.get("filename", ""), f) for f in fr] if isinstance(fr, list) else list(fr.items())
    out, names = [], []
    for name, f in items:
        if f.get("rotated"):
            raise ValueError("rotated frames are not supported")
        r = f["frame"]
        ss = f.get("spriteSourceSize", {"x": 0, "y": 0})
        src = f.get("sourceSize", {"w": r["w"], "h": r["h"]})
        out.append((r["x"], r["y"], r["w"], r["h"], ss["x"], ss["y"], src["w"], src["h"]))
        names.append(name)
    return out, names, tex.get("image")


def is_atlas_json(js):
    if not isinstance(js, dict):
        return False
    if "textures" in js:
        return isinstance(js["textures"], list) and js["textures"] and "frames" in js["textures"][0]
    return "frames" in js


def sheet_frames(rgba, cell):
    H, W = rgba.shape[:2]
    out, names = [], []
    alpha = rgba[..., 3] > 0
    n = 0
    for cy in range(0, H - cell + 1, cell):
        for cx in range(0, W - cell + 1, cell):
            a = alpha[cy : cy + cell, cx : cx + cell]
            ys, xs = np.nonzero(a.any(axis=1))[0], np.nonzero(a.any(axis=0))[0]
            if len(ys) == 0:
                out.append((0, 0, 0, 0, 0, 0, cell, cell))
            else:
                y0, y1, x0, x1 = ys[0], ys[-1] + 1, xs[0], xs[-1] + 1
                out.append((cx + x0, cy + y0, x1 - x0, y1 - y0, x0, y0, cell, cell))
            names.append(str(n))
            n += 1
    return out, names


def image_frames(rgba):
    H, W = rgba.shape[:2]
    if W <= MAX_PAGE and H <= MAX_PAGE:
        return [(0, 0, W, H, 0, 0, W, H)], [""]
    out, names = [], []
    # tiles of at most 1023 px so the gutter still fits in a 1024 page
    T = MAX_PAGE - 1
    for ty in range(0, H, T):
        for tx in range(0, W, T):
            w, h = min(T, W - tx), min(T, H - ty)
            out.append((tx, ty, w, h, tx, ty, W, H))
            names.append("")
    return out, names


def anim_backgrounds(assets):
    """Names under images/battle_anims that upstream loads whole, from assets/battle-anims/*.json:
    every AnimTimedAddBgEvent resourceName (AnimConfig.getBackgroundResourceNames).
    Returns (backgrounds, graphics): graphics are the names loaded as spritesheets."""
    bgs, graphics = set(), set()

    def walk(o):
        if isinstance(o, dict):
            if o.get("eventType") == ANIM_BG_EVENT and o.get("resourceName"):
                bgs.add(o["resourceName"])
            for v in o.values():
                walk(v)
        elif isinstance(o, list):
            for v in o:
                walk(v)

    root = os.path.join(assets, "battle-anims")
    for f in sorted(os.listdir(root)) if os.path.isdir(root) else []:
        if f.endswith(".json"):
            js = json.load(open(os.path.join(root, f), encoding="utf-8"))
            for a in js if isinstance(js, list) else [js]:
                if isinstance(a, dict) and a.get("graphic"):
                    graphics.add(a["graphic"])
            walk(js)
    return bgs, graphics


def anim_bg_mode(rel, anim_bgs):
    """How to cut images/<rel>: None (the default for its folder), "whole" (a background only)
    or "both" (a background that is also an animation's graphic). anim_bgs: anim_backgrounds()."""
    folder = rel.split("/")[0]
    if folder not in SPRITESHEETS or "/" not in rel:
        return None
    name = rel[len(folder) + 1:-4]
    bgs, graphics = anim_bgs
    if name not in bgs:
        return None
    return "both" if name in graphics else "whole"


def convert_png(png_path, rel, bg_mode=None):
    """rel: path relative to images/ (with .png). bg_mode: anim_bg_mode(); without it every
    battle_anims image is cut into cells. Returns (rrt_bytes, stats)."""
    rgba = load_rgba(png_path)
    js_path = png_path[:-4] + ".json"
    folder = rel.split("/")[0]
    if os.path.exists(js_path):
        try:
            js = json.load(open(js_path, encoding="utf-8"))
        except ValueError:
            js = None
        if is_atlas_json(js):
            frames, names, _ = atlas_frames(js)
            return encode(rgba, frames, names)
    if folder in SPRITESHEETS and "/" in rel and bg_mode != "whole":
        frames, names = sheet_frames(rgba, SPRITESHEETS[folder])
        if bg_mode == "both":
            whole, _ = image_frames(rgba)
            frames += whole
            names += ["__BASE"] * len(whole)
        return encode(rgba, frames, names)
    frames, names = image_frames(rgba)
    return encode(rgba, frames, names)


def convert_variant_map(js):
    """images/pokemon/variant/*.json color maps -> RVM1 binary.
    "RVM1" u8 count, then per variant: u8 variant, u8 0, u16 n, n x (u32 from, u32 to) as 0xRRGGBBAA.
    Upstream compares RGB only (alpha > 0 pixels), so alpha is stored as 0xFF."""
    def col(h):
        h = h.lstrip("#")
        v = int(h[:6], 16)
        return (v << 8) | (int(h[6:8], 16) if len(h) >= 8 else 0xFF)

    parts = [struct.pack("<4sB", b"RVM1", len(js))]
    for k, m in js.items():
        parts.append(struct.pack("<BBH", int(k), 0, len(m)))
        for a, b in m.items():
            parts.append(struct.pack("<II", col(a), col(b)))
    return b"".join(parts)


def encode_raw_page(idx_plane, palette):
    """One pow2 page of palette indices, stored as-is (no trimming or repacking),
    with a single frame covering it. For generated textures such as font pages."""
    H, W = idx_plane.shape
    assert W == pow2(W) and H == pow2(H) and W <= MAX_PAGE and H <= MAX_PAGE
    pal = np.asarray(palette, np.uint32)
    soft = any(0 < (c & 0xFF) < 255 for c in pal.tolist())
    names = b"\0"
    out = bytearray(struct.pack("<4sBBHHHHHI", b"RRT1", KIND_INDEXED, GPU_RGBA8 if soft else GPU_5551, 1, 1,
                                len(pal), W, H, len(names)))
    out += pal.astype("<u4").tobytes()
    out += struct.pack("<HH", W, H)
    out += struct.pack("<HHHHHhhHHH", 0, 0, 0, W, H, 0, 0, W, H, 0)
    out += names
    out += b"\0" * (-len(out) & 3)
    out += swizzle(idx_plane.astype(np.uint8)).tobytes()
    return bytes(out)
