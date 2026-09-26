#!/usr/bin/env python3
"""Pixel test of the 3DS texture path: draws frames from gfx.pak in Azahar
(Old 3DS mode, via lr-run) and compares the screenshot with the same frames
composed from the upstream PNG/JSON on the PC.

    python3 emu_gfx_test.py ASSETS SD_ROOT APP.3dsx CORE.so LR_RUN WORKDIR

SD_ROOT is the emulated SD card (DATA/Azahar/sdmc) holding 3ds/pokerogue/gfx.pak.
A pixel matches when every channel is within 8 of the reference (one RGBA5551
step, which covers the palette de-collision nudge)."""
import json, os, subprocess, sys
import numpy as np
from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import gfx  # noqa

assets, sd, app, core, lr, work = sys.argv[1:7]
IMG = os.path.join(assets, "images")
CLEAR = (32, 32, 32)

# (key, frame spec, x, y, variant map key or None, variant)
CASES = [
    ("pokemon/back/642-therian", "PAGE1", 0, 0, None, 0),     # multi-page sheet, frame on page 1
    ("pokemon/445", "0001.png", 250, 0, None, 0),             # Garchomp front
    ("battle_anims/PRAS- Fire", "#10", 0, 140, None, 0),     # 96px spritesheet cell, trimmed
    ("pokemon/1", "0001.png", 150, 150, "pokemon/variant/1.json", 1),  # shiny variant recolour
    ("pokemon/back/257", "0001.png", 300, 130, None, 0),      # Blaziken back
]


def frames_of(rel):
    png = os.path.join(IMG, rel + ".png")
    rgba = gfx.load_rgba(png)
    js = os.path.join(IMG, rel + ".json")
    if os.path.exists(js) and gfx.is_atlas_json(json.load(open(js))):
        fr, names, _ = gfx.atlas_frames(json.load(open(js)))
    elif rel.split("/")[0] in gfx.SPRITESHEETS:
        fr, names = gfx.sheet_frames(rgba, gfx.SPRITESHEETS[rel.split("/")[0]])
    else:
        fr, names = gfx.image_frames(rgba)
    return rgba, fr, names


def page1_frame(rel):
    """name of a frame the packer put on page 1."""
    import struct
    rgba, fr, names = frames_of(rel)
    data, st = gfx.encode(rgba, fr, names)
    kind, gpu, pages, nfr, npal, w, h, nsz = struct.unpack_from("<BBHHHHHI", data, 4)
    off = 20 + npal * 4 + pages * 4
    for i in range(nfr):
        p = struct.unpack_from("<H", data, off + i * 20)[0]
        if p == 1:
            return names[i]
    raise SystemExit("no frame on page 1")


def compose(rel, spec, vmap, variant):
    rgba, fr, names = frames_of(rel)
    if spec.startswith("#"):
        i = int(spec[1:])
    else:
        i = names.index(spec)
    sx, sy, w, h, ox, oy, sw, sh = fr[i]
    out = np.zeros((sh, sw, 4), np.uint8)
    H, W = rgba.shape[:2]
    sub = np.zeros((h, w, 4), np.uint8)
    avail = rgba[sy:min(sy + h, H), sx:min(sx + w, W)]
    sub[: avail.shape[0], : avail.shape[1]] = avail
    out[oy:oy + h, ox:ox + w] = sub
    if vmap:
        m = json.load(open(os.path.join(IMG, vmap)))[str(variant)]
        pairs = list(m.items())[:32]
        flat = out.reshape(-1, 4)
        vis = flat[:, 3] > 0
        rgb = (flat[:, 0].astype(np.uint32) << 16) | (flat[:, 1].astype(np.uint32) << 8) | flat[:, 2]
        done = np.zeros(len(flat), bool)
        for a, b in pairs:
            sel = vis & ~done & (rgb == int(a[:6], 16))
            v = int(b[:6], 16)
            flat[sel, 0], flat[sel, 1], flat[sel, 2] = v >> 16, (v >> 8) & 255, v & 255
            done |= sel
    return out


def expand5(v):
    v = v >> 3
    return (v << 3) | (v >> 2)


cases, lines = [], []
for rel, spec, x, y, vmap, var in CASES:
    if spec == "PAGE1":
        spec = page1_frame(rel)
    cases.append((rel, spec, x, y, vmap, var))
    line = f"show images/{rel.replace(' ', '|')} {spec.replace(' ', '|')} {x} {y}"
    if vmap:
        line += f" images/{vmap} {var}"
    lines.append(line)
open(os.path.join(sd, "3ds/pokerogue/paktest.txt"), "w").write("\n".join(lines + ["frames 100000"]) + "\n")
done = os.path.join(sd, "3ds/pokerogue/paktest-shown")
if os.path.exists(done):
    os.remove(done)
shot = os.path.join(work, "gfx-test.png")
data_dir = os.path.dirname(os.path.dirname(sd.rstrip("/")))
subprocess.run([lr, core, app, data_dir, "frames=3000", "until=" + done, "shotlast=" + shot], check=True,
               capture_output=True)
print(open(os.path.join(sd, "3ds/pokerogue/paktest-log.txt")).read())
scr = np.asarray(Image.open(shot).convert("RGB"))[:240].astype(int)
ref = np.zeros((240, 400, 3), int)
ref[:] = CLEAR
for rel, spec, x, y, vmap, var in cases:
    img = compose(rel, spec, vmap, var)
    h, w = img.shape[:2]
    h, w = min(h, 240 - y), min(w, 400 - x)
    img = img[:h, :w]
    vis = img[..., 3] > 0
    region = ref[y:y + h, x:x + w]
    region[vis] = expand5(img[..., :3][vis].astype(int))
Image.fromarray(ref.astype(np.uint8)).save(os.path.join(work, "gfx-ref.png"))
diff = np.abs(scr - ref).max(axis=2)
bad = diff > 8
print(f"pixels compared: {diff.size}, mismatched: {int(bad.sum())}, max diff {int(diff.max())}")
if bad.any():
    ys, xs = np.nonzero(bad)
    print("first mismatches:", list(zip(xs[:10].tolist(), ys[:10].tolist())))
    Image.fromarray((bad * 255).astype(np.uint8)).save(os.path.join(work, "gfx-diff.png"))
sys.exit(1 if bad.any() else 0)
