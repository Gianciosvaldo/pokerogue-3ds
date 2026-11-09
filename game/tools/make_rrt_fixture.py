#!/usr/bin/env python3
"""Writes game/tests/fixtures/rrt-first-frame.txt: a small RRT sheet made by assets-pipeline/gfx.py (the pipeline's own encoder, so the tile
order and the palette are the real ones) and the RGBA of its first frame, for game/tests/rrt_first_frame_test.c.
usage: python3 game/tools/make_rrt_fixture.py   (needs numpy and Pillow, as the pipeline does)"""
import os
import random
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "assets-pipeline"))
import gfx  # noqa: E402

random.seed(5)
W, H = 70, 41
img = np.zeros((H, W, 4), np.uint8)
cols = [(200, 30, 30, 255), (30, 200, 30, 255), (30, 30, 200, 255), (250, 250, 10, 128), (12, 34, 56, 255)]
for y in range(H):
    for x in range(W):
        if random.random() < 0.7:
            img[y, x] = cols[random.randrange(len(cols))]
# the first frame is a 21x13 rect at (27, 19), trimmed from 24x16; a second frame follows
frames = [(27, 19, 21, 13, 2, 3, 24, 16), (3, 2, 10, 9, 0, 0, 10, 9)]
blob, stats = gfx.encode(img, frames, ["0001.png", "0002.png"])
assert stats["kind"] == gfx.KIND_INDEXED
sx, sy, w, h = frames[0][:4]
exp = img[sy : sy + h, sx : sx + w].copy()
exp[exp[..., 3] == 0] = 0  # a transparent texel is index 0, which reads as colour 0
out = os.path.join(HERE, "..", "tests", "fixtures", "rrt-first-frame.txt")
with open(out, "w") as f:
    f.write("# an RRT sheet made by assets-pipeline/gfx.py encode() and the RGBA of its first frame (game/tools/make_rrt_fixture.py)\n")
    f.write("R %s\nW %d %d\nX %s\n" % (blob.hex(), w, h, exp.tobytes().hex()))
print(out, len(blob), "bytes")
