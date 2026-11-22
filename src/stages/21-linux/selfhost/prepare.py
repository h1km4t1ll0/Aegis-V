#!/usr/bin/env python3
"""Transport pinned source/recipes and clone our existing guest-formatted disk."""
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[4]
HERE = Path(__file__).resolve().parent
OUT = HERE.parent / 'artifacts'
SOURCE_SHA = '3c88ff4648122e81832b7067be6456ef28e119b64649a29e84f7f04691e489f9'
READY_SHA = '7b69cb22aef5d990204b96d099623760f19981b0fb1af54a8f8201efa9ea781d'


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def main():
    ready = OUT / 'rootfs-linux-ready.tar'
    source = ROOT / 'build/sources/linux-6.6.tar.gz'
    assert digest(ready) == READY_SHA, 'ready checkpoint hash mismatch'
    assert digest(source) == SOURCE_SHA, 'kernel source hash mismatch'
    disk = OUT / 'linux-selfhost.raw'
    sidecar = disk.with_name(disk.name + '.owner.json')
    if not disk.exists() and not sidecar.exists():
        original = OUT / 'linux-data-03.raw'
        metadata = json.loads(original.with_name(original.name + '.owner.json').read_text())
        assert metadata['raw_path'] == str(original.resolve())
        assert original.stat().st_size == metadata['size_bytes'] == 4 * 1024**3
        subprocess.run(['cp', '-c', str(original), str(disk)], check=True)
        metadata['raw_path'] = str(disk.resolve())
        with sidecar.open('x') as f:
            json.dump(metadata, f, indent=2)
            f.write('\n')
    metadata = json.loads(sidecar.read_text())
    assert metadata['raw_path'] == str(disk.resolve())
    assert disk.stat().st_size == metadata['size_bytes'] == 4 * 1024**3
    bundle = OUT / 'linux-selfhost-input.tar'
    with bundle.open('xb') as dest, tarfile.open(fileobj=dest, mode='w', format=tarfile.USTAR_FORMAT) as tf:
        def add(name, data):
            info = tarfile.TarInfo(name)
            info.size = len(data)
            info.mode = 0o644
            tf.addfile(info, io.BytesIO(data))
        add('linux-6.6.tar.gz', source.read_bytes())
        with tarfile.open(ready) as previous:
            for name in ('config', 'sbi.bin'):
                add(name, previous.extractfile('usr/share/aegis/linux66/' + name).read())
        for name in ('build.sh', 'smoke.sh', 'pthread.c', 'flush.c', 'init-list'):
            add(name, (HERE / name).read_bytes())
        for name in ('init.c', 'smoke.sh', 'tests/hello.c', 'tests/hello.cpp'):
            add('original/' + name, (HERE.parent / 'kernel' / name).read_bytes())
    print('input tar', bundle, bundle.stat().st_size, digest(bundle))
    print('data disk', disk, 'owner', metadata['token'])


if __name__ == '__main__':
    main()
