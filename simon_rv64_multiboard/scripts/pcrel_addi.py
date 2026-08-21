#!/usr/bin/env python3
"""Rewrite auipc+addiw same-rd pairs to auipc+addi for RV64 pcrel addresses."""
import sys

AUIPC = 0x17
ADDIW = 0x1B
ADDI = 0x13


def patch_pcrel_addiw(data):
    """Patch pcrel address materialization in a RISC-V binary.

    @param data - Mutable bytearray
    @returns Number of patched pairs
    """
    patched = 0
    limit = len(data) - 7
    offset = 0
    while offset <= limit:
        first = int.from_bytes(data[offset:offset + 4], "little")
        if (first & 0x7F) != AUIPC:
            offset += 2
            continue
        second = int.from_bytes(data[offset + 4:offset + 8], "little")
        if (second & 0x7F) != ADDIW:
            offset += 2
            continue
        if ((second >> 12) & 0x7) != 0:
            offset += 2
            continue
        rd = (first >> 7) & 0x1F
        rd2 = (second >> 7) & 0x1F
        rs1 = (second >> 15) & 0x1F
        if rd == 0 or rd != rd2 or rd != rs1:
            offset += 2
            continue
        data[offset + 4] = (data[offset + 4] & 0x80) | ADDI
        patched += 1
        offset += 8
    return patched


def main():
    """CLI: pcrel_addi.py IN OUT

    @returns None
    """
    src = open(sys.argv[1], "rb").read()
    buf = bytearray(src)
    count = patch_pcrel_addiw(buf)
    open(sys.argv[2], "wb").write(buf)
    print("patched %d auipc+addiw pairs (%d -> %d bytes)" % (
        count, len(src), len(buf)))


if __name__ == "__main__":
    main()
