#!/usr/bin/env python3
"""Checks a finished PokeRogue.3dsx and/or PokeRogue.cia against the packs it was made from. Standard library only,
and it shares no code with bundle.py, so a mistake in the writer isn't repeated in the check.

    check_bundle.py PACK_DIR [--3dsx FILE] [--cia FILE]

3dsx: the header's RomFS offset, the RomFS found by name through its hash table (the way libctru's romfs driver
      does it), and every pack's bytes equal to the file in the pack folder (SHA-256).
cia:  the CIA's header and sections, the TMD's hash chain down to the content, the NCCH inside, its RomFS (IVFC
      header, then the level 3 data found the same way as above) and every pack's bytes in it.
Exit status 0 only if everything held; every line printed starts with ok or FAIL.
"""
import hashlib
import mmap
import os
import struct
import sys

PACKS = ["gfx.pak", "snd.pak", "bgm.pak", "font.pak", "data.pak"]
NONE = 0xFFFFFFFF
failed = []


def ok(cond, msg):
    print(("ok   " if cond else "FAIL ") + msg, flush=True)
    if not cond:
        failed.append(msg)
    return cond


def name_hash(name):
    h = 123456789
    for unit in struct.unpack(f"<{len(name.encode('utf-16le')) // 2}H", name.encode("utf-16le")):
        h = (((h >> 5) | (h << 27)) & 0xFFFFFFFF) ^ unit
    return h


def sha256_range(m, start, size):
    h = hashlib.sha256()
    end = start + size
    while start < end:
        n = min(8 << 20, end - start)
        h.update(m[start:start + n])
        start += n
    return h.hexdigest()


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(8 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def romfs_lookup(head, name):
    """head = the RomFS level 3 header and tables (everything before the file data). Finds name in the root folder
    through the hash table, as libctru does. Returns (offset of the data from the start of level 3, size) or None."""
    hdr = struct.unpack_from("<10I", head, 0)
    if hdr[0] != 0x28:
        return None
    dir_off, dir_size, file_hash_off, file_hash_size, file_off, data_off = hdr[3], hdr[4], hdr[5], hdr[6], hdr[7], hdr[9]
    root = struct.unpack_from("<6I", head, dir_off)
    if root[0] != 0 or root[5] != 0:  # the root folder: its own parent, no name
        return None
    raw = name.encode("utf-16le")
    at = struct.unpack_from("<I", head, file_hash_off + 4 * (name_hash(name) % (file_hash_size // 4)))[0]
    while at != NONE:
        parent, _sib, off, size, hash_sib, name_len = struct.unpack_from("<2IQQ2I", head, file_off + at)
        if parent == 0 and head[file_off + at + 32:file_off + at + 32 + name_len] == raw:
            return data_off + off, size
        at = hash_sib
    return None


def root_listing(head):
    hdr = struct.unpack_from("<10I", head, 0)
    dir_off, file_off = hdr[3], hdr[7]
    at = struct.unpack_from("<6I", head, dir_off)[3]
    names = []
    while at != NONE:
        _p, sib, _off, _size, _hs, name_len = struct.unpack_from("<2IQQ2I", head, file_off + at)
        names.append(head[file_off + at + 32:file_off + at + 32 + name_len].decode("utf-16le"))
        at = sib
    return names


def check_level3(m, l3, limit, pack_dir, label):
    """l3 = where the RomFS level 3 starts in m; limit = where the space the RomFS may use ends."""
    if not ok(struct.unpack_from("<I", m, l3)[0] == 0x28, f"{label}: level 3 header starts with 0x28"):
        return
    head = m[l3:l3 + struct.unpack_from("<I", m, l3 + 36)[0]]  # up to the file data
    ok(sorted(root_listing(head)) == sorted(PACKS), f"{label}: the root folder lists exactly the five packs")
    for name in PACKS:
        want = os.path.join(pack_dir, name)
        found = romfs_lookup(head, name)
        if not ok(found is not None, f"{label}: {name} found through the hash table"):
            continue
        off, size = found
        ok(size == os.path.getsize(want), f"{label}: {name} is {size} bytes, the pack is {os.path.getsize(want)}")
        ok(l3 + off + size <= limit, f"{label}: {name} ends inside the RomFS")
        ok(sha256_range(m, l3 + off, size) == sha256_file(want), f"{label}: {name} has the pack's SHA-256")


def check_3dsx(path, pack_dir):
    with open(path, "rb") as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as m:
        ok(m[:4] == b"3DSX", "3dsx: magic")
        hdr_size = struct.unpack_from("<H", m, 4)[0]
        ok(hdr_size >= 44, f"3dsx: extended header ({hdr_size} bytes)")
        smdh_off, smdh_size, fs = struct.unpack_from("<3I", m, 32)
        ok(0 < smdh_off < fs and smdh_size > 0, "3dsx: icon (SMDH) is in the app part")
        ok(0 < fs < len(m), f"3dsx: RomFS offset {fs} is inside the {len(m)} byte file")
        ok(fs % 4 == 0, "3dsx: RomFS offset is a multiple of 4")
        if 0 < fs < len(m):
            check_level3(m, fs, len(m), pack_dir, "3dsx")


def be32(m, o):
    return struct.unpack_from(">I", m, o)[0]


def al(n, a):
    return (n + a - 1) // a * a


def check_cia(path, pack_dir):
    with open(path, "rb") as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as m:
        hdr, typ, ver, cert, tik, tmd, meta = struct.unpack_from("<I2H4I", m, 0)
        csize = struct.unpack_from("<Q", m, 0x18)[0]
        ok(hdr == 0x2020 and typ == 0, f"cia: header size 0x{hdr:X}, type {typ}")
        cert_off = al(hdr, 64)
        tik_off = al(cert_off + cert, 64)
        tmd_off = al(tik_off + tik, 64)
        con_off = al(tmd_off + tmd, 64)  # header, certificates, ticket, TMD, content, then the meta footer
        meta_off = al(con_off + csize, 64)
        ok(len(m) == meta_off + meta, "cia: sections add up to the file's size")
        sig = {0x10003: 0x240, 0x10004: 0x140}.get(be32(m, tmd_off))
        if not ok(sig is not None, "cia: TMD signature type is RSA-4096/2048 SHA-256"):
            return
        body = tmd_off + sig
        title = struct.unpack_from(">Q", m, body + 0x4C)[0]
        count = struct.unpack_from(">H", m, body + 0x9E)[0]
        ok(count == 1, f"cia: one content (title id {title:016X})")
        info_hash = bytes(m[body + 0xA4:body + 0xC4])
        infos = bytes(m[body + 0xC4:body + 0xC4 + 64 * 0x24])
        ok(hashlib.sha256(infos).digest() == info_hash, "cia: TMD header hash of the content info records")
        cmd_count = struct.unpack_from(">H", infos, 2)[0]
        chunks = body + 0xC4 + 0x900
        ok(hashlib.sha256(bytes(m[chunks:chunks + 0x30 * cmd_count])).digest() == infos[4:36],
           "cia: content info record hash of the content chunk records")
        cid, cidx, ctype, csz = struct.unpack_from(">IHHQ", m, chunks)
        chash = bytes(m[chunks + 16:chunks + 48])
        ok(csz == csize, f"cia: content size {csz} equals the header's {csize}")
        ok(ctype & 1 == 0, "cia: content is not encrypted in the CIA")
        ok(bytes.fromhex(sha256_range(m, con_off, csz)) == chash, "cia: content SHA-256 matches the TMD")

        nc = con_off
        ok(m[nc + 0x100:nc + 0x104] == b"NCCH", "ncch: magic")
        ok(struct.unpack_from("<I", m, nc + 0x104)[0] * 0x200 == csz, "ncch: size in its header equals the content size")
        ok(struct.unpack_from("<Q", m, nc + 0x108)[0] == title, "ncch: title id equals the TMD's")
        ok(m[nc + 0x18F] & 4 != 0, "ncch: NoCrypto flag set")
        exefs_size = struct.unpack_from("<I", m, nc + 0x1A4)[0]
        romfs_off, romfs_size = struct.unpack_from("<2I", m, nc + 0x1B0)
        ok(exefs_size > 0, "ncch: has an ExeFS")
        if not ok(romfs_size > 0 and (romfs_off + romfs_size) * 0x200 <= csz, "ncch: RomFS fits in the NCCH"):
            return
        rb = nc + romfs_off * 0x200
        ok(m[rb:rb + 4] == b"IVFC" and struct.unpack_from("<I", m, rb + 4)[0] == 0x10000, "romfs: IVFC header")
        master = struct.unpack_from("<I", m, rb + 8)[0]
        l3_size, l3_log = struct.unpack_from("<QI", m, rb + 0x44)
        hdr_len = struct.unpack_from("<I", m, rb + 0x54)[0]
        l3 = rb + al(hdr_len + master, 1 << l3_log)  # the data level is stored first, the hash levels after it
        ok(l3_size <= romfs_size * 0x200, f"romfs: level 3 is {l3_size} bytes")
        check_level3(m, l3, l3 + l3_size, pack_dir, "cia")


def main(argv):
    if not argv or argv[0].startswith("-"):
        print(__doc__)
        return 2
    pack_dir, args = argv[0], argv[1:]
    for p in PACKS:
        if not os.path.exists(os.path.join(pack_dir, p)):
            print(f"FAIL {p} is missing from {pack_dir}")
            return 1
    want = 0
    while args:
        kind, path = args[0], args[1]
        args = args[2:]
        want += 1
        print(f"== {path}")
        if kind == "--3dsx":
            check_3dsx(path, pack_dir)
        elif kind == "--cia":
            check_cia(path, pack_dir)
        else:
            print(__doc__)
            return 2
    if not want:
        print(__doc__)
        return 2
    print("ALL OK" if not failed else f"{len(failed)} CHECKS FAILED")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
