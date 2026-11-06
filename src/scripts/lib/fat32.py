"""FAT32 initrd for Tilck, built from bytes (no mkfs.fat, no mtools).

pack_fat32({"bin/init": b"..."}) lays the files out like Tilck's own build
(mformat + fathack): 512-byte sectors, 4096-byte clusters, one FAT, the
first data sector page aligned (Tilck mmaps read-only ELF segments straight
from ramdisk clusters, kernel/fs/fat32_mm.c), every entry with a long name
(Tilck compares long names case-sensitively, short names case-insensitively)
and the image cut after the last used cluster (fathack --truncate). The BPB
still declares >= 65525 clusters, so the volume is FAT32 by its cluster
count (fat_get_type()).

read_fat32(img) is the reader for the host round-trip test (python3 -I
src/scripts/lib/fat32.py).
"""
import struct

BPS = 512
SPC = 8
CLUSTER = BPS * SPC
MIN_CLUSTERS = 65525 + 16  # FAT32 by count, away from the 65525 edge
EOC = 0x0FFFFFFF
LFN_CHARS = 13
ATTR_DIR = 0x10
ATTR_ARCHIVE = 0x20
ATTR_LFN = 0x0F
DATE_1980 = 0x0021  # 1980-01-01: fixed, the image is reproducible


def _short_name(name, used):
    """Unique 11-byte 8.3 name; the long name is the one Tilck matches."""
    base, _, ext = name.rpartition(".") if "." in name.strip(".") else (name, "", "")
    keep = lambda s: "".join(c for c in s.upper() if c.isalnum() or c in "_-")
    base, ext = keep(base) or "X", keep(ext)[:3]
    cand = base[:8].ljust(8) + ext.ljust(3)
    n = 1
    while cand in used or (len(base) > 8 and n == 1):
        tail = "~%d" % n
        cand = (base[: 8 - len(tail)] + tail).ljust(8) + ext.ljust(3)
        n += 1
    used.add(cand)
    return cand.encode("ascii")


def _checksum(short):
    s = 0
    for b in short:
        s = (((s & 1) << 7) + (s >> 1) + b) & 0xFF
    return s


def _lfn_entries(name, short):
    """LFN entries in on-disk order (last part first)."""
    units = [ord(c) for c in name]
    if any(u > 0x7E or u < 0x20 for u in units):
        raise ValueError("non-ASCII or control char in %r" % name)
    if len(units) % LFN_CHARS:
        units.append(0)
    while len(units) % LFN_CHARS:
        units.append(0xFFFF)
    parts = len(units) // LFN_CHARS
    chk = _checksum(short)
    out = []
    for i in range(parts, 0, -1):
        u = units[(i - 1) * LFN_CHARS : i * LFN_CHARS]
        ord_ = i | (0x40 if i == parts else 0)
        out.append(
            struct.pack("<B5HBBB6HH2H", ord_, *u[:5], ATTR_LFN, 0, chk, *u[5:11], 0, *u[11:13])
        )
    return out


def _dirent(short, attr, clus, size):
    return struct.pack(
        "<11sBBBHHHHHHHI",
        short, attr, 0, 0, 0, DATE_1980, DATE_1980,
        clus >> 16, 0, DATE_1980, clus & 0xFFFF, size,
    )


def _tree(files):
    """{dir path: {name: bytes or None for a subdir}} with "" the root."""
    dirs = {"": {}}
    for path, data in sorted(files.items()):
        parts = [p for p in path.split("/") if p]
        if not parts or any(p in (".", "..") for p in parts):
            raise ValueError("bad path %r" % path)
        cur = ""
        for p in parts[:-1]:
            sub = cur + "/" + p if cur else p
            if dirs[cur].get(p, None) is not None:
                raise ValueError("%r is a file" % sub)
            dirs[cur][p] = None
            dirs.setdefault(sub, {})
            cur = sub
        if parts[-1] in dirs[cur]:
            raise ValueError("duplicate %r" % path)
        dirs[cur][parts[-1]] = bytes(data)
    return dirs


def pack_fat32(files):
    """files: {"dir/name": bytes}. Returns the image bytes."""
    dirs = _tree(files)
    order = sorted(dirs, key=lambda d: (d.count("/"), d))
    # directory sizes first: entries per dir (".", "..", LFN + short each)
    shorts = {}
    nents = {}
    for d in order:
        used = set()
        n = 0 if d == "" else 2
        for name in sorted(dirs[d]):
            short = _short_name(name, used)
            shorts[(d, name)] = short
            n += len(_lfn_entries(name, short)) + 1
        nents[d] = n + 1  # a free entry ends the listing (fat_walk stops)
    # cluster allocation: directories in order, then files
    next_clu = 2
    first = {}
    nclus = {}
    for d in order:
        nclus[d] = max(1, -(-nents[d] * 32 // CLUSTER))
        first[d] = next_clu
        next_clu += nclus[d]
    for d in order:
        for name, data in sorted(dirs[d].items()):
            if data is None or not data:
                continue
            key = (d + "/" + name) if d else name
            nclus[key] = -(-len(data) // CLUSTER)
            first[key] = next_clu
            next_clu += nclus[key]
    used_clusters = next_clu - 2
    count = max(MIN_CLUSTERS, used_clusters)
    fatsz = -(-(count + 2) * 4 // BPS)
    rsvd = 32
    while (rsvd + fatsz) % SPC:
        rsvd += 1
    totsec = rsvd + fatsz + count * SPC
    data_off = (rsvd + fatsz) * BPS
    img = bytearray(data_off + used_clusters * CLUSTER)
    # boot sector, FSInfo, backup copies at sectors 6 and 7
    bs = bytearray(BPS)
    bs[0:3] = b"\xEB\x58\x90"
    bs[3:11] = b"AEGISV  "
    struct.pack_into("<HBHBHHBHHHII", bs, 11, BPS, SPC, rsvd, 1, 0, 0, 0xF8, 0, 32, 64, 0, totsec)
    struct.pack_into("<IHHIHH12sBBBI11s8s", bs, 36, fatsz, 0, 0, 2, 1, 6, b"", 0x80, 0, 0x29,
                     0x41454749, b"TILCKINITRD", b"FAT32   ")
    bs[510:512] = b"\x55\xAA"
    fsi = bytearray(BPS)
    struct.pack_into("<I", fsi, 0, 0x41615252)
    struct.pack_into("<III", fsi, 484, 0x61417272, count - used_clusters, next_clu)
    struct.pack_into("<I", fsi, 508, 0xAA550000)
    for at in (0, 6):
        img[(at) * BPS : (at + 1) * BPS] = bs
        img[(at + 1) * BPS : (at + 2) * BPS] = fsi
    # FAT
    fat = rsvd * BPS
    struct.pack_into("<II", img, fat, 0x0FFFFFF8, EOC)

    def chain(start, n):
        for i in range(n):
            nxt = EOC if i == n - 1 else start + i + 1
            struct.pack_into("<I", img, fat + (start + i) * 4, nxt)

    def clu_off(c):
        return data_off + (c - 2) * CLUSTER

    for d in order:
        chain(first[d], nclus[d])
        ents = []
        if d:
            parent = d.rpartition("/")[0]
            pclu = first[parent] if parent else 0  # ".." of a root child is 0
            ents.append(_dirent(b".          ", ATTR_DIR, first[d], 0))
            ents.append(_dirent(b"..         ", ATTR_DIR, pclu, 0))
        for name, data in sorted(dirs[d].items()):
            short = shorts[(d, name)]
            ents.extend(_lfn_entries(name, short))
            key = (d + "/" + name) if d else name
            if data is None:
                ents.append(_dirent(short, ATTR_DIR, first[key], 0))
            else:
                ents.append(_dirent(short, ATTR_ARCHIVE, first.get(key, 0), len(data)))
        blob = b"".join(ents)
        img[clu_off(first[d]) : clu_off(first[d]) + len(blob)] = blob
        for name, data in dirs[d].items():
            key = (d + "/" + name) if d else name
            if data:
                chain(first[key], nclus[key])
                img[clu_off(first[key]) : clu_off(first[key]) + len(data)] = data
    return bytes(img)


def read_fat32(img):
    """{"dir/name": bytes} of every file (directories are implied)."""
    bps, spc, rsvd, nfats = struct.unpack_from("<HBHB", img, 11)
    totsec = struct.unpack_from("<I", img, 32)[0]
    fatsz, _, _, root = struct.unpack_from("<IHHI", img, 36)
    data_sec = rsvd + nfats * fatsz
    count = (totsec - data_sec) // spc
    if count < 65525:
        raise ValueError("not FAT32: %d clusters" % count)
    cs = bps * spc
    fat = rsvd * bps

    def clusters(c):
        out = []
        while 2 <= c < 0x0FFFFFF8:
            out.append(c)
            c = struct.unpack_from("<I", img, fat + c * 4)[0] & 0x0FFFFFFF
        return out

    def body(c):
        return b"".join(img[(data_sec + (x - 2) * spc) * bps :][:cs] for x in clusters(c))

    files = {}

    def walk(c, prefix):
        raw = body(c)
        lname = []
        for i in range(0, len(raw), 32):
            e = raw[i : i + 32]
            if e[0] == 0:
                return
            if e[0] == 0xE5:
                lname = []
                continue
            if e[11] == ATTR_LFN:
                u = struct.unpack_from("<5H", e, 1) + struct.unpack_from("<6H", e, 14) + struct.unpack_from("<2H", e, 28)
                lname.insert(0, ("".join(chr(x) for x in u if x not in (0, 0xFFFF)), e[13]))
                continue
            short = bytes(e[:11])
            if short in (b".          ", b"..         ") or e[11] & 0x08:
                lname = []
                continue
            if lname and all(k == _checksum(short) for _, k in lname):
                name = "".join(s for s, _ in lname)
            else:
                b, x = short[:8].decode().rstrip(), short[8:].decode().rstrip()
                name = b + ("." + x if x else "")
            lname = []
            hi, lo, size = struct.unpack_from("<H", e, 20)[0], struct.unpack_from("<H", e, 26)[0], struct.unpack_from("<I", e, 28)[0]
            clu = (hi << 16) | lo
            path = prefix + name
            if e[11] & ATTR_DIR:
                walk(clu, path + "/")
            else:
                files[path] = body(clu)[:size] if size else b""

    walk(root, "")
    return files


def _self_test():
    import hashlib

    blobs = {
        "bin/init": bytes(range(256)) * 37,
        "bin/proc": b"\x7fELF" + b"p" * 9000,
        "bin/child": b"c" * CLUSTER,
        "empty": b"",
        "A Long File Name.with.dots": b"lfn\n",
        "UPPER.TXT": b"upper\n",
        "deep/er/still/x.c": b"int x;\n",
    }
    for i in range(140):  # a directory spanning more than one cluster
        blobs["many/file-%03d.dat" % i] = hashlib.sha256(b"%d" % i).digest() * (i % 3)
    img = pack_fat32(blobs)
    back = read_fat32(img)
    if back != blobs:
        bad = sorted(set(blobs) ^ set(back)) or [k for k in blobs if blobs[k] != back[k]]
        raise SystemExit("fat32 round-trip FAIL: %r" % bad[:5])
    rsvd = struct.unpack_from("<H", img, 14)[0]
    fatsz = struct.unpack_from("<I", img, 36)[0]
    if (rsvd + fatsz) * BPS % 4096:
        raise SystemExit("fat32 first data sector not page aligned")
    print("fat32 round-trip ok: %d files, image %d bytes" % (len(back), len(img)))


if __name__ == "__main__":
    _self_test()
