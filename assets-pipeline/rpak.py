"""RPAK: the archive format the 3DS app reads assets from.

One file holds thousands of assets so the app opens a single file and seeks,
instead of walking FAT32 directories with thousands of entries.

Layout (little endian):
  header   32 bytes: "RPK1", u32 version=1, u32 count, u32 index_off,
                     u32 names_off, u32 names_size, u32 0, u32 0
  data     entry payloads, each 4-byte aligned
  index    count x 24 bytes, sorted by hash:
             u64 hash (FNV-1a 64 of the key, UTF-8)
             u32 offset, u32 stored_size, u32 size
             u8 codec (0 raw, 1 lz4 block), u8 type, u16 0
  names    NUL-terminated keys in index order (for listing/debugging only;
           the app never needs to load them)
"""
import struct

import lz4.block

CODEC_RAW, CODEC_LZ4 = 0, 1
TYPE_TEX, TYPE_SND, TYPE_JSON, TYPE_BIN, TYPE_FONT, TYPE_VARIANT = 1, 2, 3, 4, 5, 6


def fnv1a64(s):
    h = 0xCBF29CE484222325
    for b in s.encode("utf-8"):
        h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


class Writer:
    def __init__(self, path):
        self.f = open(path, "wb")
        self.f.write(b"\0" * 32)
        self.entries = {}  # hash -> (key, off, stored, size, codec, type)

    def add(self, key, data, type_, compress=True):
        h = fnv1a64(key)
        if h in self.entries:
            other = self.entries[h][0]
            raise ValueError(f"duplicate or colliding key: {key!r} vs {other!r}")
        codec, stored = CODEC_RAW, data
        if compress and len(data) > 64:
            c = lz4.block.compress(data, mode="high_compression", compression=12, store_size=False)
            if len(c) < len(data) * 0.95:
                codec, stored = CODEC_LZ4, c
        off = self.f.tell()
        self.f.write(stored)
        self.f.write(b"\0" * (-len(stored) & 3))
        self.entries[h] = (key, off, len(stored), len(data), codec, type_)
        return len(stored)

    def close(self):
        items = sorted(self.entries.items())
        index_off = self.f.tell()
        for h, (key, off, stored, size, codec, type_) in items:
            self.f.write(struct.pack("<QIIIBBH", h, off, stored, size, codec, type_, 0))
        names_off = self.f.tell()
        names = b"".join(k.encode("utf-8") + b"\0" for _, (k, *_rest) in items)
        self.f.write(names)
        self.f.seek(0)
        self.f.write(struct.pack("<4sIIIIIII", b"RPK1", 1, len(items), index_off, names_off, len(names), 0, 0))
        self.f.close()


class Reader:
    def __init__(self, path):
        self.f = open(path, "rb")
        magic, ver, count, index_off, names_off, names_size, _, _ = struct.unpack("<4sIIIIIII", self.f.read(32))
        assert magic == b"RPK1" and ver == 1
        self.f.seek(index_off)
        raw = self.f.read(count * 24)
        self.f.seek(names_off)
        names = self.f.read(names_size).split(b"\0")
        self.index = {}
        for i in range(count):
            h, off, stored, size, codec, type_, _ = struct.unpack_from("<QIIIBBH", raw, i * 24)
            self.index[names[i].decode()] = (off, stored, size, codec, type_)

    def keys(self):
        return self.index.keys()

    def read(self, key):
        off, stored, size, codec, type_ = self.index[key]
        self.f.seek(off)
        d = self.f.read(stored)
        return lz4.block.decompress(d, uncompressed_size=size) if codec == CODEC_LZ4 else d
