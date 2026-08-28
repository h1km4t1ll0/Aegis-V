#!/usr/bin/env python3
"""Host image formats: hex0 text, KFS1, ELF wrap, files.pl."""
import struct
import sys
from pathlib import Path

EM_RISCV = 243
PT_LOAD = 1
PF_X = 1
PF_W = 2
PF_R = 4
LOAD_VA = 0x82000000
PAGE = 0x1000


def bytes_to_hex0(data):
    """Encode a binary as Simon hex0 text."""
    lines = []
    for i in range(0, len(data), 4):
        chunk = data[i : i + 4]
        lines.append(" ".join(f"{byte:02X}" for byte in chunk))
    return "\n".join(lines) + ("\n" if lines else "")


def pack_kfs(entries):
    """Pack (path, bytes) into KFS1."""
    body = b""
    n = 0
    for path, data in entries:
        pb = path.encode("ascii")
        body += struct.pack("<H", len(pb)) + pb
        body += struct.pack("<I", len(data)) + data
        n += 1
    return b"KFS1" + struct.pack("<I", n) + body


def wrap_elf(raw, va=LOAD_VA):
    """Wrap a raw RV64 image linked at va as ELF64 ET_EXEC."""
    phoff = 64
    ehdr_size = 64
    phdr_size = 56
    offset = PAGE
    filesz = len(raw)
    ehdr = bytearray(ehdr_size)
    ehdr[0:4] = b"\x7fELF"
    ehdr[4] = 2
    ehdr[5] = 1
    ehdr[6] = 1
    struct.pack_into("<H", ehdr, 16, 2)
    struct.pack_into("<H", ehdr, 18, EM_RISCV)
    struct.pack_into("<I", ehdr, 20, 1)
    struct.pack_into("<Q", ehdr, 24, va)
    struct.pack_into("<Q", ehdr, 32, phoff)
    struct.pack_into("<Q", ehdr, 40, 0)
    struct.pack_into("<I", ehdr, 48, 0)
    struct.pack_into("<H", ehdr, 52, ehdr_size)
    struct.pack_into("<H", ehdr, 54, phdr_size)
    struct.pack_into("<H", ehdr, 56, 1)
    struct.pack_into("<H", ehdr, 58, 0)
    struct.pack_into("<H", ehdr, 60, 0)
    struct.pack_into("<H", ehdr, 62, 0)
    phdr = struct.pack(
        "<IIQQQQQQ",
        PT_LOAD,
        PF_R | PF_W | PF_X,
        offset,
        va,
        va,
        filesz,
        filesz,
        PAGE,
    )
    pad = b"\0" * (offset - ehdr_size - phdr_size)
    return bytes(ehdr) + phdr + pad + raw


def parse_pl(data):
    """Split a files.pl blob into (name, content) records."""
    recs = []
    i = 0
    n = len(data)
    while i < n:
        if data[i : i + 4] != b"src ":
            z = data.find(b"\0", i)
            if z < 0:
                break
            i = z + 1
            continue
        nl = data.find(b"\n", i)
        if nl < 0:
            break
        name = data[i + 4 : nl].decode("ascii")
        z = data.find(b"\0", nl + 1)
        if z < 0:
            recs.append((name, data[nl + 1 :]))
            break
        recs.append((name, data[nl + 1 : z]))
        i = z + 1
    return recs


def serialize_pl(recs):
    """Write files.pl records."""
    chunks = []
    for name, content in recs:
        chunks.append(b"src " + name.encode("ascii") + b"\n" + content + b"\0")
    return b"".join(chunks)


def main():
    if len(sys.argv) == 4 and sys.argv[1] == "hex0":
        data = Path(sys.argv[2]).read_bytes()
        Path(sys.argv[3]).write_text(bytes_to_hex0(data))
        return
    if len(sys.argv) == 3:
        data = Path(sys.argv[1]).read_bytes()
        Path(sys.argv[2]).write_text(bytes_to_hex0(data))
        return
    print(
        "usage: image.py hex0 IN.bin OUT.hex0 | image.py IN.bin OUT.hex0",
        file=sys.stderr,
    )
    sys.exit(2)


if __name__ == "__main__":
    main()
