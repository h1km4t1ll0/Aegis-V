#!/usr/bin/env python3
"""Recover an exact guest-produced disk-root boot archive after its install gate."""
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
    for marker in ('diskroot install PASS', 'selfhost export fsync ok', 'linux boot ok'):
        if marker not in log: raise ValueError('incomplete gate: ' + marker)
    size = int(re.findall(r'^diskroot checkpoint bytes[ \t]+([0-9]+)[ \t]*$', log, re.M)[-1])
    expected = re.findall(r'^([0-9a-f]{64})  /tmp/diskroot-work/checkpoint.tar$', log, re.M)[-1]
    if not 0 < size <= a.export.stat().st_size or a.checkpoint.exists():
        raise ValueError('invalid byte count or existing destination')
    h = hashlib.sha256()
    with a.export.open('rb') as f:
        remaining = size
        while remaining:
            b = f.read(min(remaining, 1024 * 1024))
            if not b: raise ValueError('short export')
            h.update(b)
            remaining -= len(b)
    if h.hexdigest() != expected: raise ValueError('guest/host archive hash mismatch')
    with tarfile.open(a.export) as tf:
        base = 'usr/share/aegis/linux66/'
        manifest = tf.extractfile(base + 'diskroot/artifacts.sha256').read().decode()
        checked = set()
        for line in manifest.splitlines():
            wanted, name = line.split('  ', 1)
            if name not in ('Image', 'sbi.bin', 'config', 'initramfs.cpio'):
                raise ValueError('unexpected artifact')
            got = hashlib.sha256(tf.extractfile(base + name).read()).hexdigest()
            if got != wanted: raise ValueError('artifact hash mismatch: ' + name)
            checked.add(name)
            print(name, got)
        if len(checked) != 4: raise ValueError('missing boot artifacts')
    with a.export.open('r+b') as f: f.truncate(size)
    a.export.rename(a.checkpoint)
    print('diskroot guest/host SHA256 match', expected, size, a.checkpoint)


if __name__ == '__main__':
    main()
