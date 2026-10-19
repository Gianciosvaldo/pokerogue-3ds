"""Puts the game app and its five packs into one file for the 3DS. Standard library only.

Two kinds of file come out of here:
  * a .3dsx with the packs as its RomFS: the plain app, then the RomFS appended, with its offset written into the
    header. This is what devkitPro's `3dsxtool --romfs=DIR` does, and libctru's romfsMountSelf() reads it when the
    Homebrew Launcher starts the file. sd-builder/tests/bundle_test.py compares both byte for byte.
  * an installable .cia, made by makerom from the app's ELF, with the packs as the RomFS of the title. makerom
    builds that RomFS itself, from a folder holding the packs.

The .3dsx's RomFS is a flat folder (the packs sit in its root), written the way devkitPro's mkromfs3ds writes it.
Nothing here knows what the files are; the packs are Pokemon and PokeRogue content and the result is for the
builder's own console only.
"""
import os
import shutil
import struct
import subprocess

NONE = 0xFFFFFFFF
ALIGN = 4  # mkromfs3ds aligns file data to 4 bytes
COPY = 8 << 20


def _hash_count(entries):
    """Size of a RomFS hash table for this many entries (3dbrew, "RomFS")."""
    if entries < 3:
        return 3
    if entries < 19:
        return entries | 1
    n = entries
    while any(n % p == 0 for p in (2, 3, 5, 7, 11, 13, 17)):
        n += 1
    return n


def _name_hash(parent, name):
    """The hash libctru and the 3DS file system look names up by (3dbrew, "RomFS"): over UTF-16 code units."""
    raw = name.encode("utf-16le")
    h = parent ^ 123456789
    for unit in struct.unpack(f"<{len(raw) // 2}H", raw):
        h = ((h >> 5) | (h << 27)) & 0xFFFFFFFF
        h ^= unit
    return h


def _pad4(n):
    return (n + 3) & ~3


def romfs_tables(files):
    """files: [(name, size)] in the order they are stored, all in the root folder. Returns (the header and tables,
    the RomFS offset of each file's data, the RomFS's total size)."""
    names = [n for n, _ in files]
    if len(set(names)) != len(names) or any(not n or "/" in n or "\\" in n for n in names):
        raise ValueError("RomFS names must be unique, non-empty and without folders")
    meta_off, o = [], 0  # where each file's entry sits in the file table
    for n in names:
        meta_off.append(o)
        o += 32 + _pad4(len(n.encode("utf-16le")))
    meta_size = o

    data_off, o = [], 0
    for _, size in files:
        o = (o + ALIGN - 1) & ~(ALIGN - 1)
        data_off.append(o)
        o += size
    data_size = o

    buckets = _hash_count(len(files))
    hash_table = [NONE] * buckets
    hash_sibling = [NONE] * len(files)
    sibling = [NONE] * len(files)
    first_file = NONE
    for i, name in enumerate(names):
        sibling[i] = first_file  # the newest file is the folder's first, as mkromfs3ds does it
        first_file = meta_off[i]
        b = _name_hash(0, name) % buckets
        hash_sibling[i] = hash_table[b]
        hash_table[b] = meta_off[i]
    file_meta = b"".join(
        struct.pack("<2IQQ2I", 0, sibling[i], data_off[i], size, hash_sibling[i], len(n.encode("utf-16le")))
        + n.encode("utf-16le").ljust(_pad4(len(n.encode("utf-16le"))), b"\0")
        for i, (n, size) in enumerate(files))
    assert len(file_meta) == meta_size
    dir_meta = struct.pack("<6I", 0, NONE, NONE, first_file, NONE, 0)  # the root folder, which has no name
    assert _name_hash(0, "") % 3 == 0
    dir_hash = struct.pack("<3I", 0, NONE, NONE)  # one folder: 3 buckets, the root's in the first
    file_hash = struct.pack(f"<{buckets}I", *hash_table)

    off_dir_hash = 0x28
    off_dir_meta = off_dir_hash + len(dir_hash)
    off_file_hash = off_dir_meta + len(dir_meta)
    off_file_meta = off_file_hash + len(file_hash)
    off_data = off_file_meta + len(file_meta)
    header = struct.pack("<10I", 0x28, off_dir_hash, len(dir_hash), off_dir_meta, len(dir_meta), off_file_hash,
                         len(file_hash), off_file_meta, len(file_meta), off_data)
    head = header + dir_hash + dir_meta + file_hash + file_meta
    assert len(head) == off_data
    return head, [off_data + d for d in data_off], _pad4(off_data + data_size)  # mkromfs3ds pads the end too


def _write_romfs(out, files, progress):
    """Writes the RomFS for files [(name, path)] at the current position of the open file out."""
    start = out.tell()
    sizes = [(n, os.path.getsize(p)) for n, p in files]
    head, offsets, total = romfs_tables(sizes)
    out.write(head)
    done = 0
    for (name, path), off, (_, size) in zip(files, offsets, sizes):
        out.write(b"\0" * (start + off - out.tell()))
        with open(path, "rb") as f:
            while True:
                chunk = f.read(COPY)
                if not chunk:
                    break
                out.write(chunk)
                done += len(chunk)
                if progress:
                    progress(done)
        if out.tell() != start + off + size:
            raise OSError(f"{path} changed size while it was being copied")
    out.write(b"\0" * (start + total - out.tell()))
    return total


def write_romfs(dest, files, progress=None):
    """A RomFS file for files [(name, path)], as mkromfs3ds writes it. Returns its size."""
    with open(dest, "wb") as out:
        total = _write_romfs(out, files, progress)
        out.flush()
        os.fsync(out.fileno())
    return total


def make_3dsx(plain_3dsx, files, dest, progress=None):
    """dest = the plain .3dsx with a RomFS of files [(name, path)] appended and its offset in the header, which is
    what `3dsxtool --romfs` writes. The app must have the extended header (an SMDH), as every game/ build has.
    Returns the RomFS offset."""
    with open(plain_3dsx, "rb") as f:
        plain = f.read()
    if len(plain) < 44 or plain[:4] != b"3DSX":
        raise ValueError(f"{plain_3dsx} isn't a .3dsx")
    if struct.unpack_from("<H", plain, 4)[0] < 44:
        raise ValueError(f"{plain_3dsx} has no extended header (it was built without an icon)")
    if struct.unpack_from("<I", plain, 40)[0] != 0:
        raise ValueError(f"{plain_3dsx} already has a RomFS")
    total = sum(os.path.getsize(p) for _, p in files) + len(plain)
    if total + 4096 >= 1 << 32:
        raise ValueError("a .3dsx can't be 4 GB or more")
    with open(dest, "wb") as out:
        out.write(plain[:40] + struct.pack("<I", len(plain)) + plain[44:])
        _write_romfs(out, files, progress)
        out.flush()
        os.fsync(out.fileno())
    return len(plain)


def stage_romfs_dir(stage, files):
    """The folder makerom builds the title's RomFS from: the packs, hard-linked (copied where the drive can't)."""
    shutil.rmtree(stage, ignore_errors=True)
    os.makedirs(stage)
    for name, path in files:
        try:
            os.link(path, os.path.join(stage, name))
        except OSError:
            shutil.copyfile(path, os.path.join(stage, name))


def make_cia(makerom, work, out_name, log=None):
    """Runs makerom in the folder work, which holds pokerogue.rsf, pokerogue.elf, pokerogue.smdh, banner.bnr and the
    folder romfs (stage_romfs_dir). Everything is named relatively, so the folder's own path (spaces, accents) never
    reaches makerom. -target t is the key set homebrew is installed with. Returns the output path."""
    cmd = [makerom, "-f", "cia", "-o", out_name, "-rsf", "pokerogue.rsf", "-DROMFS_DIR=romfs", "-target", "t",
           "-elf", "pokerogue.elf", "-icon", "pokerogue.smdh", "-banner", "banner.bnr",
           "-major", "0", "-minor", "1", "-micro", "0"]
    p = subprocess.run(cmd, cwd=work, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
    if log:
        log(f"makerom exit {p.returncode}\n{p.stdout}")
    out = os.path.join(work, out_name)
    if p.returncode != 0 or not os.path.exists(out):
        last = p.stdout.strip().splitlines()[-1] if p.stdout.strip() else f"exit {p.returncode}"
        raise RuntimeError(f"makerom failed: {last}")
    return out
