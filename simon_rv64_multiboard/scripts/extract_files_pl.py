#!/usr/bin/env python3
"""Write one files.pl record to stdout or a file."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from stage_images import FILES_PL, parse_pl

def main():
    name = sys.argv[1]
    dest = sys.argv[2]
    recs = parse_pl(open(FILES_PL, "rb").read())
    for n, blob in recs:
        if n == name:
            open(dest, "wb").write(blob)
            print("wrote %s %d bytes" % (dest, len(blob)))
            return
    raise SystemExit("no record %s in %s" % (name, FILES_PL))

if __name__ == "__main__":
    main()
