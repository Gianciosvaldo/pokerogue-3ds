#!/usr/bin/env python3
"""Checks how gfx.py cuts images/battle_anims against upstream's loadAnimAssets:

  - backgrounds (AnimTimedAddBgEvent names) decode to the whole PNG, one frame
  - the names used both ways keep every 96x96 cell, plus a "__BASE" frame of the whole PNG
  - every other battle_anims image is byte-identical to the cells-only conversion

    python3 tests/anim_bg_test.py PATH/pokerogue-assets
"""
import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import gfx  # noqa: E402


def decode(rrt):
    """RRT bytes -> {frame name or index: RGBA uint8 array (src_h, src_w, 4)} (trim offsets applied)."""
    magic, kind, gpu, npages, nframes, npal, W, H, names_size = struct.unpack_from("<4sBBHHHHHI", rrt, 0)
    assert magic == b"RRT1"
    off = 20
    pal = np.frombuffer(rrt, "<u4", npal, off)
    off += 4 * npal
    pages = [struct.unpack_from("<HH", rrt, off + 4 * i) for i in range(npages)]
    off += 4 * npages
    frames = [struct.unpack_from("<HHHHHhhHHH", rrt, off + 20 * i) for i in range(nframes)]
    off += 20 * nframes
    names = rrt[off:off + names_size]
    off += names_size
    off += -off & 3
    planes = []
    for pw, ph in pages:
        bpp = 1 if kind == gfx.KIND_INDEXED else (4 if gpu == gfx.GPU_RGBA8 else 2)
        n = pw * ph
        tiled = np.frombuffer(rrt, {1: "u1", 2: "<u2", 4: "<u4"}[bpp], n, off)
        off += n * bpp
        raster = tiled[gfx._tiled_order(pw, ph)].reshape(ph, pw)
        if kind == gfx.KIND_INDEXED:
            v = pal[raster]
            rgba = np.stack([(v >> 24) & 255, (v >> 16) & 255, (v >> 8) & 255, v & 255], -1)
        elif gpu == gfx.GPU_RGBA8:
            v = raster.astype(np.uint32)
            rgba = np.stack([(v >> 24) & 255, (v >> 16) & 255, (v >> 8) & 255, v & 255], -1)
        else:
            v = raster.astype(np.uint32)
            rgba = np.stack([((v >> 11) & 31) << 3, ((v >> 6) & 31) << 3, ((v >> 1) & 31) << 3, (v & 1) * 255], -1)
        planes.append(rgba.astype(np.uint8))
    out = {}
    for i, (p, x, y, w, h, ox, oy, sw, sh, noff) in enumerate(frames):
        name = names[noff:names.index(b"\0", noff)].decode()
        img = np.zeros((sh, sw, 4), np.uint8)
        if w and h:
            img[oy:oy + h, ox:ox + w] = planes[p][y:y + h, x:x + w]
        out[name if name else i] = img
        out.setdefault(i, img)
    return out, (kind, gpu)


def same(a, b, fmt):
    """Visible pixels equal (at the stored precision), transparent where the source is."""
    kind, gpu = fmt
    va, vb = a[..., 3] > 0, b[..., 3] > 0
    if not (va == vb).all():
        return False
    if kind == gfx.KIND_DIRECT and gpu == gfx.GPU_5551:
        return (a[va][:, :3] >> 3 == b[vb][:, :3] >> 3).all()
    return (a[va] == b[vb]).all()


def main():
    assets = sys.argv[1]
    bgs, graphics = gfx.anim_backgrounds(assets)
    root = os.path.join(assets, "images", "battle_anims")
    counts = {None: 0, "whole": 0, "both": 0}
    bad = []
    for f in sorted(os.listdir(root)):
        if not f.endswith(".png"):
            continue
        rel = "battle_anims/" + f
        p = os.path.join(root, f)
        mode = gfx.anim_bg_mode(rel, (bgs, graphics))
        counts[mode] += 1
        new, _ = gfx.convert_png(p, rel, mode)
        old, _ = gfx.convert_png(p, rel)
        src = gfx.load_rgba(p)
        if mode is None:
            if new != old:
                bad.append((f, "changed although not a background"))
            continue
        frames, fmt = decode(new)
        if mode == "whole":
            if len([k for k in frames if isinstance(k, int)]) != 1 or not same(frames[0], src, fmt):
                bad.append((f, "background is not one whole-image frame"))
            continue
        cells, ofmt = decode(old)
        ncells = len([k for k in cells if isinstance(k, int)])
        for i in range(ncells):
            if not same(frames[i], cells[i], fmt):
                bad.append((f, f"cell {i} differs"))
                break
        if "__BASE" not in frames or not same(frames["__BASE"], src, fmt):
            bad.append((f, "no whole-image __BASE frame"))
    print(f"backgrounds: {len(bgs)} names ({len(bgs & graphics)} also graphics); images: "
          f"{counts['whole']} whole, {counts['both']} cells + __BASE, {counts[None]} cells unchanged")
    missing = sorted(n for n in bgs if not os.path.exists(os.path.join(root, n + ".png")))
    if missing:
        print(f"backgrounds upstream names but has no image for (upstream's load fails too): {missing}")
    for f, why in bad:
        print("FAIL", f, why)
    print("OK" if not bad else f"{len(bad)} FAILED")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
