#!/usr/bin/env python3
"""Pack (path, bytes) into KFS1."""
import struct
import sys

def pack(entries):
    body = b""
    n = 0
    for path, data in entries:
        pb = path.encode("ascii")
        body += struct.pack("<H", len(pb)) + pb
        body += struct.pack("<I", len(data)) + data
        n += 1
    return b"KFS1" + struct.pack("<I", n) + body

def main():
    # args: -o out.kfs path1=file1 path2=file2
    out = sys.argv[2]
    ents = []
    for spec in sys.argv[3:]:
        path, fn = spec.split("=", 1)
        ents.append((path, open(fn, "rb").read()))
    open(out, "wb").write(pack(ents))

if __name__ == "__main__":
    main()
