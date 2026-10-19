#!/usr/bin/env python3
"""Tests for sd-builder/bundle.py. Standard library only.

    python3 sd-builder/tests/bundle_test.py                      # the RomFS looked up the way libctru does it
    python3 sd-builder/tests/bundle_test.py --reference [ELF SMDH]   # also against devkitPro's own tools

--reference runs mkromfs3ds (and 3dsxtool, given the app's ELF and SMDH) from the devkitpro/devkitarm Docker image
and requires the files bundle.py writes to be byte-for-byte what those tools write.
"""
import hashlib
import os
import random
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import bundle  # noqa: E402

NONE = 0xFFFFFFFF
REFERENCE = None  # (elf, smdh) or (None, None) when --reference is given


def rd(path):
    with open(path, "rb") as f:
        return f.read()


def lookup(romfs, name):
    """libctru's romfs_dev.c: find a file in the root folder through the hash table. Returns (offset, size)."""
    hdr = struct.unpack_from("<10I", romfs, 0)
    assert hdr[0] == 0x28
    dir_hash_off, dir_hash_size, dir_off, dir_size, file_hash_off, file_hash_size, file_off, file_size, data_off = hdr[1:]
    # the root folder: its entry is the first in the folder table, found through the folder hash table
    root_bucket = bundle._name_hash(0, "") % (dir_hash_size // 4)
    assert struct.unpack_from("<I", romfs, dir_hash_off + 4 * root_bucket)[0] == 0
    h = bundle._name_hash(0, name)
    at = struct.unpack_from("<I", romfs, file_hash_off + 4 * (h % (file_hash_size // 4)))[0]
    raw = name.encode("utf-16le")
    while at != NONE:
        parent, sibling, off, size, hash_sibling, name_len = struct.unpack_from("<2IQQ2I", romfs, file_off + at)
        if parent == 0 and name_len == len(raw) and romfs[file_off + at + 32:file_off + at + 32 + name_len] == raw:
            return data_off + off, size
        at = hash_sibling
    return None


def listing(romfs):
    """The root folder's files, walked through the sibling chain the way readdir does."""
    hdr = struct.unpack_from("<10I", romfs, 0)
    dir_off, file_off = hdr[3], hdr[7]
    root = struct.unpack_from("<6I", romfs, dir_off)
    assert root[0] == 0 and root[1] == NONE and root[2] == NONE and root[5] == 0
    out, at = [], root[3]
    while at != NONE:
        parent, sibling, off, size, hs, name_len = struct.unpack_from("<2IQQ2I", romfs, file_off + at)
        out.append(romfs[file_off + at + 32:file_off + at + 32 + name_len].decode("utf-16le"))
        at = sibling
    return out


def make_files(d, names, rng):
    files = []
    for n in names:
        p = os.path.join(d, "src-" + hashlib.sha1(n.encode()).hexdigest())
        with open(p, "wb") as f:
            f.write(rng.randbytes(rng.choice([0, 1, 3, 4, 5, 1000, 70000])))
        files.append((n, p))
    return files


class RomfsTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="bundle-test-")
        self.addCleanup(shutil.rmtree, self.tmp, True)
        self.rng = random.Random(7)

    def test_hash_counts(self):
        want = {0: 3, 1: 3, 2: 3, 3: 3, 4: 5, 5: 5, 17: 17, 18: 19, 19: 19, 20: 23, 27: 29, 30: 31}
        for n, c in want.items():
            self.assertEqual(bundle._hash_count(n), c, n)

    def check_romfs(self, names):
        files = make_files(self.tmp, names, self.rng)
        dest = os.path.join(self.tmp, "out.romfs")
        total = bundle.write_romfs(dest, files)
        romfs = rd(dest)
        self.assertEqual(len(romfs), total)
        self.assertEqual(total % 4, 0)
        self.assertEqual(sorted(listing(romfs)), sorted(names))
        for n, p in files:
            off, size = lookup(romfs, n)
            self.assertEqual(romfs[off:off + size], rd(p), n)
        self.assertIsNone(lookup(romfs, "missing.pak"))
        return romfs

    def test_the_five_packs(self):
        self.check_romfs(["gfx.pak", "snd.pak", "bgm.pak", "font.pak", "data.pak"])

    def test_many_and_unusual_names(self):
        self.check_romfs([f"file{i:02d}.bin" for i in range(40)] + ["é.bin", "x", "日本.pak", "a" * 100])

    def test_empty(self):
        self.check_romfs([])

    def test_size_guard(self):
        bundle.check_fits("3dsx", 850 << 20)
        bundle.check_fits("cia", 3 << 30)
        for kind in ("3dsx", "cia"):
            with self.assertRaises(ValueError) as e:
                bundle.check_fits(kind, (4 << 30) - (8 << 20))
            self.assertIn("loose files", str(e.exception))

    def test_bad_names(self):
        for bad in ([("a", 1), ("a", 2)], [("", 1)], [("d/f", 1)], [("d\\f", 1)]):
            with self.assertRaises(ValueError):
                bundle.romfs_tables(bad)

    def plain_3dsx(self, header_size=44, fs=0):
        body = bytes(range(256)) * 20
        hdr = struct.pack("<4sHHIIIIII", b"3DSX", header_size, 8, 0, 0, 100, 200, 300, 400)
        ext = struct.pack("<3I", 2000, 100, fs)
        return (hdr + (ext if header_size >= 44 else b"")) + body

    def test_3dsx(self):
        plain = os.path.join(self.tmp, "plain.3dsx")
        with open(plain, "wb") as f:
            f.write(self.plain_3dsx())
        files = make_files(self.tmp, ["gfx.pak", "bgm.pak"], self.rng)
        dest = os.path.join(self.tmp, "full.3dsx")
        fs = bundle.make_3dsx(plain, files, dest)
        a, b = rd(plain), rd(dest)
        self.assertEqual(fs, len(a))
        self.assertEqual(struct.unpack_from("<I", b, 40)[0], len(a))
        self.assertEqual(b[:40] + b[44:len(a)], a[:40] + a[44:])
        romfs = b[fs:]
        self.assertEqual(sorted(listing(romfs)), ["bgm.pak", "gfx.pak"])
        ref = os.path.join(self.tmp, "ref.romfs")
        bundle.write_romfs(ref, files)
        self.assertEqual(romfs, rd(ref))

    def test_3dsx_refusals(self):
        files = make_files(self.tmp, ["gfx.pak"], self.rng)
        for name, data in (("none", b"\0" * 64), ("short", b"3DSX" + b"\0" * 20), ("noext", self.plain_3dsx(32)),
                           ("has", self.plain_3dsx(44, fs=999))):
            p = os.path.join(self.tmp, name)
            with open(p, "wb") as f:
                f.write(data)
            with self.assertRaises(ValueError, msg=name):
                bundle.make_3dsx(p, files, p + ".out")


def docker(args, mounts):
    cmd = ["docker", "run", "--rm"]
    for host, ctr in mounts:
        cmd += ["-v", f"{host}:{ctr}"]
    return subprocess.run(cmd + ["devkitpro/devkitarm"] + args, check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


class ReferenceTests(unittest.TestCase):
    """The same bytes as devkitPro's tools. mkromfs3ds stores files in directory order, so the order is read back
    from its output and bundle.py is asked for that one."""

    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="bundle-ref-")
        self.addCleanup(shutil.rmtree, self.tmp, True)

    def reference_dir(self):
        d = os.path.join(self.tmp, "dir")
        os.makedirs(d)
        rng = random.Random(3)
        for n in ["gfx.pak", "snd.pak", "bgm.pak", "font.pak", "data.pak"] + [f"f{i}.bin" for i in range(22)] + ["é.bin"]:
            with open(os.path.join(d, n), "wb") as f:
                f.write(rng.randbytes(rng.choice([0, 3, 5, 4096, 40001])))
        return d

    def order(self, romfs):
        file_off, file_size = struct.unpack_from("<2I", romfs, 0x1C)
        names, o = [], 0
        while o < file_size:
            n = struct.unpack_from("<I", romfs, file_off + o + 28)[0]
            names.append(romfs[file_off + o + 32:file_off + o + 32 + n].decode("utf-16le"))
            o += 32 + bundle._pad4(n)
        return names

    def test_romfs_matches_mkromfs3ds(self):
        d = self.reference_dir()
        docker(["mkromfs3ds", "/w/dir", "/w/ref.romfs"], [(self.tmp, "/w")])
        ref = rd(os.path.join(self.tmp, "ref.romfs"))
        mine = os.path.join(self.tmp, "mine.romfs")
        bundle.write_romfs(mine, [(n, os.path.join(d, n)) for n in self.order(ref)])
        self.assertEqual(rd(mine), ref)

    def test_3dsx_matches_3dsxtool(self):
        if not REFERENCE or not REFERENCE[0]:
            self.skipTest("needs --reference ELF SMDH")
        elf, smdh = REFERENCE
        d = self.reference_dir()
        shutil.copy(elf, os.path.join(self.tmp, "app.elf"))
        shutil.copy(smdh, os.path.join(self.tmp, "app.smdh"))
        docker(["sh", "-c", "3dsxtool /w/app.elf /w/plain.3dsx --smdh=/w/app.smdh && "
                            "3dsxtool /w/app.elf /w/ref.3dsx --smdh=/w/app.smdh --romfs=/w/dir"], [(self.tmp, "/w")])
        ref = rd(os.path.join(self.tmp, "ref.3dsx"))
        fs = struct.unpack_from("<I", ref, 40)[0]
        mine = os.path.join(self.tmp, "mine.3dsx")
        bundle.make_3dsx(os.path.join(self.tmp, "plain.3dsx"), [(n, os.path.join(d, n)) for n in self.order(ref[fs:])], mine)
        self.assertEqual(rd(mine), ref)


if __name__ == "__main__":
    args = sys.argv[1:]
    if args and args[0] == "--reference":
        REFERENCE = (os.path.abspath(args[1]), os.path.abspath(args[2])) if len(args) >= 3 else (None, None)
        del sys.argv[1:]
    else:
        del ReferenceTests
    unittest.main()
