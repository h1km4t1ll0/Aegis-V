#!/usr/bin/env python3
"""Wrap a raw RV64 image linked at 0x82000000 as ELF64 ET_EXEC."""
import struct
import sys

EM_RISCV = 243
PT_LOAD = 1
PF_X = 1
PF_W = 2
PF_R = 4
LOAD_VA = 0x82000000
PAGE = 0x1000


def wrap(raw, va=LOAD_VA):
    phoff = 64
    ehdr_size = 64
    phdr_size = 56
    offset = PAGE
    filesz = len(raw)
    ehdr = bytearray(ehdr_size)
    ehdr[0:4] = b"\x7fELF"
    ehdr[4] = 2  # ELFCLASS64
    ehdr[5] = 1  # ELFDATA2LSB
    ehdr[6] = 1  # EV_CURRENT
    struct.pack_into("<H", ehdr, 16, 2)  # ET_EXEC
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


def main():
    inp = sys.argv[1]
    out = sys.argv[2]
    raw = open(inp, "rb").read()
    open(out, "wb").write(wrap(raw))


if __name__ == "__main__":
    main()
