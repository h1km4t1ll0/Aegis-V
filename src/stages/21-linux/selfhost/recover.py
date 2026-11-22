#!/usr/bin/env python3
"""Promote exact guest-written checkpoint bytes after matching its UART hash."""
import argparse
import hashlib
from pathlib import Path
import re
import tarfile


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('export', type=Path)
    ap.add_argument('log', type=Path)
    ap.add_argument('checkpoint', type=Path)
    a = ap.parse_args()
    log = a.log.read_text().replace('\r', '')
    for marker in ('selfhost export ok', 'selfhost build PASS', 'linux boot ok'):
        if marker not in log:
            raise ValueError('incomplete guest gate: ' + marker)
    size = int(re.findall(r'^selfhost checkpoint bytes[ \t]+([0-9]+)[ \t]*$', log, re.M)[-1])
    expected = re.findall(r'^([0-9a-f]{64})  /mnt/data/linux-selfhost/checkpoint.tar$', log, re.M)[-1]
    if not 0 < size <= a.export.stat().st_size or a.checkpoint.exists():
        raise ValueError('invalid byte count or existing destination')
    h = hashlib.sha256()
    with a.export.open('rb') as f:
        remaining = size
        while remaining:
            block = f.read(min(remaining, 1024 * 1024))
            if not block:
                raise ValueError('short raw export')
            h.update(block)
            remaining -= len(block)
    if h.hexdigest() != expected:
        raise ValueError('guest/host checkpoint SHA256 mismatch')
    with tarfile.open(a.export) as tf:
        base = 'usr/share/aegis/linux66/'
        sums = tf.extractfile(base + 'selfhost/artifacts.sha256').read().decode()
        for line in sums.splitlines():
            wanted, name = line.split('  ', 1)
            if name not in ('Image', 'vmlinux', 'config', 'sbi.bin', 'initramfs.cpio'):
                raise ValueError('unexpected artifact: ' + name)
            got = hashlib.sha256(tf.extractfile(base + name).read()).hexdigest()
            if got != wanted:
                raise ValueError('artifact SHA256 mismatch: ' + name)
            print(name, got)
    with a.export.open('r+b') as f:
        f.truncate(size)
    a.export.rename(a.checkpoint)
    print('guest/host checkpoint SHA256 match', expected, size, a.checkpoint)


if __name__ == '__main__':
    main()
