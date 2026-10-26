#!/usr/bin/env python3
"""Reads a finished gfx.pak with code of its own (the format is in assets-pipeline/rpak.py) and checks that every PNG in
the extra-images folder is in it, the way the game looks a picture up: images/trainer/custom/kira.png in the folder is
the key images/trainer/custom/kira, found through the pack's FNV-1a hash index, with picture data.

    check_pack_keys.py GFX_PAK EXTRA_IMAGES_DIR [--min N]

--min N  fail unless the folder has at least N pictures (the zip's folder must not have been lost on the way)
Standard library only.
"""
import os
import struct
import sys

TYPE_TEX = 1


def fnv1a64(s):
    h = 0xCBF29CE484222325
    for b in s.encode("utf-8"):
        h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def read_index(path):
    """{hash: (offset, stored, size, type, name)} from the pack's header, index and name table."""
    with open(path, "rb") as f:
        magic, version, count, index_off, names_off, names_size = struct.unpack("<4sIIIII", f.read(24))
        if magic != b"RPK1" or version != 1:
            raise SystemExit(f"FAIL {path} is not a pack (magic {magic!r}, version {version})")
        f.seek(index_off)
        raw = f.read(count * 24)
        f.seek(names_off)
        names = f.read(names_size).split(b"\0")
    out = {}
    for i in range(count):
        h, off, stored, size, _codec, type_, _ = struct.unpack_from("<QIIIBBH", raw, i * 24)
        out[h] = (off, stored, size, type_, names[i].decode("utf-8"))
    return out


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    least = int(sys.argv[sys.argv.index("--min") + 1]) if "--min" in sys.argv else 1
    if "--min" in sys.argv:
        args.remove(sys.argv[sys.argv.index("--min") + 1])
    pak, folder = args
    index = read_index(pak)
    wanted = []
    for dirpath, dirs, files in os.walk(folder):
        dirs.sort()
        for f in sorted(files):
            if f.endswith(".png"):
                rel = os.path.relpath(os.path.join(dirpath, f), folder).replace(os.sep, "/")
                wanted.append("images/" + rel[:-4])
    if len(wanted) < least:
        raise SystemExit(f"FAIL {folder} has {len(wanted)} pictures, expected at least {least}")
    bad = 0
    for key in wanted:
        entry = index.get(fnv1a64(key))
        if entry is None:
            print(f"FAIL {key}: not in {os.path.basename(pak)}")
            bad += 1
        elif entry[4] != key or entry[3] != TYPE_TEX or entry[2] == 0:
            print(f"FAIL {key}: found as {entry[4]!r}, type {entry[3]}, {entry[2]} bytes")
            bad += 1
    if bad:
        raise SystemExit(1)
    print(f"ok: {len(wanted)} extra pictures are in {os.path.basename(pak)} ({len(index)} entries), e.g. {wanted[0]}")


if __name__ == "__main__":
    main()
