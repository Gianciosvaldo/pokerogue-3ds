#!/usr/bin/env python3
"""Writes the two small files the HOME menu banner is made from: banner.png (256x128) and silence.wav.
Both are drawn here, from nothing: plain colours and a built-in pixel font, no Pokemon or PokeRogue art.
make-zip.sh feeds them to bannertool. Standard library only.

    make_banner_art.py OUT_DIR
"""
import os
import struct
import sys
import zlib

FONT = {  # 5x7 pixel capitals
    "P": ["1111.", "1...1", "1...1", "1111.", "1....", "1....", "1...."],
    "O": [".111.", "1...1", "1...1", "1...1", "1...1", "1...1", ".111."],
    "K": ["1...1", "1..1.", "1.1..", "11...", "1.1..", "1..1.", "1...1"],
    "E": ["11111", "1....", "1....", "1111.", "1....", "1....", "11111"],
    "R": ["1111.", "1...1", "1...1", "1111.", "1.1..", "1..1.", "1...1"],
    "G": [".111.", "1...1", "1....", "1.111", "1...1", "1...1", ".111."],
    "U": ["1...1", "1...1", "1...1", "1...1", "1...1", "1...1", ".111."],
    "3": ["1111.", "....1", "....1", ".111.", "....1", "....1", "1111."],
    "D": ["1111.", "1...1", "1...1", "1...1", "1...1", "1...1", "1111."],
    "S": [".1111", "1....", "1....", ".111.", "....1", "....1", "1111."],
    " ": ["....."] * 7,
}


def text(canvas, w, x, y, s, scale, rgb):
    for ch in s:
        for r, row in enumerate(FONT[ch]):
            for c, bit in enumerate(row):
                if bit == "1":
                    for dy in range(scale):
                        for dx in range(scale):
                            px = (y + r * scale + dy) * w + x + c * scale + dx
                            canvas[px] = rgb
        x += 6 * scale


def png(path, w, h, pixels):
    raw = b"".join(b"\0" + b"".join(bytes(p) + b"\xff" for p in pixels[y * w:(y + 1) * w]) for y in range(h))

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def main(out):
    os.makedirs(out, exist_ok=True)
    w, h = 256, 128
    px = [(24, 28, 56)] * (w * h)
    for y in range(h):  # a slightly lighter band across the middle
        if 36 <= y < 92:
            for x in range(w):
                px[y * w + x] = (36, 42, 84)
    for x in range(w):
        px[34 * w + x] = px[35 * w + x] = px[92 * w + x] = px[93 * w + x] = (232, 72, 72)
    name, sub = "POKEROGUE", "3DS"
    text(px, w, (w - len(name) * 6 * 4 + 4) // 2, 46, name, 4, (255, 255, 255))
    text(px, w, (w - len(sub) * 6 * 3 + 3) // 2, 108 - 12, sub, 3, (255, 214, 96))
    png(os.path.join(out, "banner.png"), w, h, px)
    rate, n = 22050, 11025  # half a second of silence: the banner needs a tune, this one is quiet
    with open(os.path.join(out, "silence.wav"), "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + n * 2) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16)
                + b"data" + struct.pack("<I", n * 2) + b"\0" * (n * 2))


if __name__ == "__main__":
    main(sys.argv[1])
