#!/usr/bin/env python3
"""Clone the saved guest disk and transport source plus our own boot artifacts."""
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
OUT = HERE.parent / 'artifacts'


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for b in iter(lambda: f.read(1024 * 1024), b''): h.update(b)
    return h.hexdigest()


def main():
    checkpoint = OUT / 'rootfs-linux-selfhost.tar'
    assert digest(checkpoint) == '2ce05a5775e2b29588ca9bf60a124a0794ece36fc24692ba7095877f89566df9'
    disk = OUT / 'linux-diskroot.raw'
    sidecar = disk.with_name(disk.name + '.owner.json')
    if disk.exists() or sidecar.exists():
        raise FileExistsError('disk-root clone already exists')
    original = OUT / 'linux-selfhost.raw'
    metadata = json.loads(original.with_name(original.name + '.owner.json').read_text())
    assert metadata['raw_path'] == str(original.resolve())
    assert original.stat().st_size == metadata['size_bytes'] == 4 * 1024**3
    subprocess.run(['cp', '-c', str(original), str(disk)], check=True)
    metadata['raw_path'] = str(disk.resolve())
    with sidecar.open('x') as f:
        json.dump(metadata, f, indent=2)
        f.write('\n')
    files = {p.name: p.read_bytes() for p in HERE.iterdir() if p.suffix in ('.c', '.sh')}
    files['pthread.c'] = (HERE.parent / 'selfhost/pthread.c').read_bytes()
    files['flush.c'] = (HERE.parent / 'selfhost/flush.c').read_bytes()
    source = ROOT / 'build/sources/linux-6.6.tar.gz'
    assert digest(source) == '3c88ff4648122e81832b7067be6456ef28e119b64649a29e84f7f04691e489f9'
    with tarfile.open(source) as kernel:
        files['gen_init_cpio.c'] = kernel.extractfile('linux-6.6/usr/gen_init_cpio.c').read()
    for name in ('hello.c', 'hello.cpp'):
        files[name] = (HERE.parent / 'kernel/tests' / name).read_bytes()
    with tarfile.open(checkpoint) as previous:
        for name in ('Image', 'sbi.bin', 'config'):
            files[name] = previous.extractfile('usr/share/aegis/linux66/' + name).read()
    files['SHA256SUMS'] = ''.join(hashlib.sha256(b).hexdigest() + '  ' + n + '\n'
                                 for n, b in sorted(files.items())).encode()
    bundle = OUT / 'linux-diskroot-input.tar'
    with bundle.open('xb') as raw, tarfile.open(fileobj=raw, mode='w', format=tarfile.USTAR_FORMAT) as tf:
        for name, b in sorted(files.items()):
            m = tarfile.TarInfo(name)
            m.size, m.mode = len(b), 0o644
            tf.addfile(m, io.BytesIO(b))
    print('disk clone', disk, 'owner', metadata['token'])
    print('input tar', bundle, bundle.stat().st_size, digest(bundle))


if __name__ == '__main__':
    main()
