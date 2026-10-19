#!/usr/bin/env python3
"""Writes five synthetic packs (RPK1, raw entries, no Pokemon content) and a manifest the pack probe checks.
The sizes follow the real packs, so the single-file builds can be tried at their real size.

    synth_packs.py OUT_DIR [SCALE]

OUT_DIR/{gfx,snd,bgm,font,data}.pak and OUT_DIR/probe-manifest.txt. SCALE (default 1.0) multiplies the sizes:
0.01 gives 8.5 MB in all, for quick runs. Each pack holds small entries before and after one large filler, so reads
near the end of the biggest file are checked too. Standard library only.
"""
import os
import random
import struct
import sys

SIZES = {"gfx": 126, "snd": 53, "bgm": 649, "font": 15, "data": 6}  # MB, as built by the pipeline


def fnv1a64(data):
    h = 0xCBF29CE484222325
    for b in data:
        h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def key_hash(key):
    return fnv1a64(key.encode("utf-8"))


def write_pack(path, name, total, rng):
    entries = []  # (hash, offset, size, key)
    small = lambda i: rng.randbytes(rng.randint(200, 20000))
    with open(path, "wb") as f:
        f.write(b"\0" * 32)

        def add(key, data=None, filler=0):
            off = f.tell()
            if filler:
                block = rng.randbytes(1 << 20)
                left = filler
                while left:
                    n = min(left, len(block))
                    f.write(block[:n])
                    left -= n
                size = filler
            else:
                f.write(data)
                size = len(data)
            f.write(b"\0" * (-size & 3))
            entries.append((key_hash(key), off, size, key, data))

        for i in range(3):
            add(f"probe/{name}/head{i}", small(i))
        tail = [small(i) for i in range(3)]
        filler = max(0, total - f.tell() - sum(len(t) + 4 for t in tail) - 24 * 16 - 1000)
        if filler:
            add(f"probe/{name}/filler", filler=filler)
        for i, t in enumerate(tail):
            add(f"probe/{name}/tail{i}", t)
        entries.sort()
        index_off = f.tell()
        for h, off, size, key, _ in entries:
            f.write(struct.pack("<QIIIBBH", h, off, size, size, 0, 4, 0))
        names_off = f.tell()
        names = b"".join(e[3].encode() + b"\0" for e in entries)
        f.write(names)
        f.seek(0)
        f.write(struct.pack("<4sIIIIIII", b"RPK1", 1, len(entries), index_off, names_off, len(names), 0, 0))
    return [(e[3], len(e[4]), fnv1a64(e[4])) for e in entries if e[4] is not None]


def main(out, scale=1.0):
    os.makedirs(out, exist_ok=True)
    rng = random.Random(20261019)
    lines = []
    for name, mb in SIZES.items():
        path = os.path.join(out, f"{name}.pak")
        samples = write_pack(path, name, int(mb * 1_000_000 * scale), rng)
        lines.append(f"pack {name}.pak {os.path.getsize(path)}")
        lines += [f"entry {k} {n} {h:016x}" for k, n, h in samples]
    with open(os.path.join(out, "probe-manifest.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"wrote {len(SIZES)} packs, {sum(os.path.getsize(os.path.join(out, n + '.pak')) for n in SIZES) / 1e6:.0f} MB")


if __name__ == "__main__":
    main(sys.argv[1], float(sys.argv[2]) if len(sys.argv) > 2 else 1.0)
