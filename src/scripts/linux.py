#!/usr/bin/env python3
"""Boot the guest-built Linux Image, initramfs and SBI on QEMU virt."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
import select
import struct
import subprocess
import sys
import tarfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parent))
from lib.qemu import QEMU, ROOT

OUT = Path(ROOT) / 'src/stages/21-linux/artifacts'
IMAGE_ADDR = 0x88000000
BOOTINFO_ADDR = 0x87f00000
INITRD_ADDR = 0x90000000
LOAD_ADDR = 0x80200000
MAGIC = 0x32544f4f424b4c54
FILES = ('Image', 'initramfs.cpio', 'sbi.bin')
DISK_BYTES = 4 * 1024**3
CMDLINE_BYTES = 200
MARKS = ('Linux version 6.6', 'linux uname Linux 6.6',
         'linux fork pipe ok', 'linux file io ok', 'linux busybox ok',
         'hello from gcc on Linux', 'hello from g++ on Linux', 'linux toolchain ok', 'linux init ok')
DISK_ROOT_MARKS = ('diskroot root ext4', 'diskroot root remounted ro')


def validate_image(data):
    if len(data) < 64 or data[48:56] != b'RISCV\0\0\0' or data[56:60] != b'RSC\x05':
        raise ValueError('invalid RV64 Linux Image header')
    offset, size, flags = struct.unpack_from('<QQQ', data, 8)
    if offset != 0x200000 or flags & 1:
        raise ValueError('expected little-endian S-mode Image with 2 MB text offset')
    if not len(data) <= size <= BOOTINFO_ADDR - LOAD_ADDR:
        raise ValueError('Image overlaps boot metadata or has invalid memory size')
    return size


def kernel_cmdline(shell=False, bootargs=''):
    args = ('console=ttyS0 earlycon=uart8250,mmio,0x10000000 rdinit=/init panic=-1'
            ' PATH=/usr/bin:/usr/sbin:/bin:/sbin')
    if shell:
        args += ' aegis.shell=1'
    if bootargs:
        args += ' ' + bootargs
    try:
        encoded = args.encode('ascii')
    except UnicodeEncodeError as exc:
        raise ValueError('kernel command line must be ASCII') from exc
    if b'\0' in encoded or len(encoded) >= CMDLINE_BYTES:
        raise ValueError('kernel command line must fit 199 bytes without embedded NUL')
    return encoded


def existing_disk(path):
    path = path.resolve()
    if not path.is_file() or ',' in str(path):
        raise ValueError('disk must be an existing raw file, path without commas')
    return path


def create_disk(path):
    # Resolve parents only: xb must reject even a dangling final symlink.
    path = path.parent.resolve() / path.name
    if ',' in str(path):
        raise ValueError('disk path must not contain commas')
    sidecar = path.with_name(path.name + '.owner.json')
    if sidecar.exists() or sidecar.is_symlink():
        raise FileExistsError('refusing existing disk metadata: ' + str(sidecar))
    token = 'aegis-' + secrets.token_hex(7)
    with path.open('xb') as raw:
        with sidecar.open('x', encoding='ascii') as owner:
            raw.truncate(DISK_BYTES)
            raw.flush()
            os.fsync(raw.fileno())
            json.dump({'version': 1, 'raw_path': str(path), 'size_bytes': DISK_BYTES,
                       'token': token}, owner, indent=2)
            owner.write('\n')
            owner.flush()
            os.fsync(owner.fileno())
    print('new blank disk', path, 'owner', token, flush=True)
    return path, token


def load_disk(path):
    path = existing_disk(path)
    sidecar = path.with_name(path.name + '.owner.json')
    metadata = json.loads(sidecar.read_text(encoding='ascii'))
    if (not isinstance(metadata, dict) or metadata.get('version') != 1
            or metadata.get('raw_path') != str(path)
            or metadata.get('size_bytes') != DISK_BYTES
            or path.stat().st_size != DISK_BYTES):
        raise ValueError('owned disk metadata path/size mismatch')
    token = metadata.get('token')
    if not isinstance(token, str) or not re.fullmatch(r'aegis-[0-9a-f]{14}', token):
        raise ValueError('invalid owned disk token')
    return path, token


def recover(rootfs, shell=False, bootargs='', image=None):
    """Copy exact guest-produced file bytes from the stage checkpoint."""
    args = kernel_cmdline(shell, bootargs)
    dest = OUT / 'boot'
    dest.mkdir(parents=True, exist_ok=True)
    with tarfile.open(rootfs, 'r:') as tf:
        for name in FILES:
            member = tf.getmember('usr/share/aegis/linux66/' + name)
            limit = (512 if name == 'initramfs.cpio' else 128) << 20
            if not member.isfile() or not 0 < member.size <= limit:
                raise ValueError('invalid boot artifact: ' + name)
            with tf.extractfile(member) as f:
                data = f.read()
            (dest / name).write_bytes(data)
            print(name, len(data), hashlib.sha256(data).hexdigest(), flush=True)
    if image is not None:
        data = image.read_bytes()
        validate_image(data)
        (dest / 'Image').write_bytes(data)
        print('Image override', len(data), hashlib.sha256(data).hexdigest(), flush=True)
    validate_image((dest / 'Image').read_bytes())
    if (dest / 'sbi.bin').stat().st_size >= 0x100000:
        raise ValueError('SBI exceeds reserved region')
    info = struct.pack('<7Q200s', MAGIC, IMAGE_ADDR, (dest / 'Image').stat().st_size,
                       INITRD_ADDR, (dest / 'initramfs.cpio').stat().st_size,
                       0, 0, args)
    (dest / 'bootinfo.bin').write_bytes(info)
    return dest


def test(rootfs, timeout, shell=False, disk=None, *, new_disk=None, verify_disk=None,
         image=None, input_tar=None, export_disk=None, disk_root=None, root_stage=None):
    if timeout <= 0:
        raise ValueError('timeout must be positive')
    if sum(path is not None for path in (disk, new_disk, verify_disk, disk_root)) > 1:
        raise ValueError('choose only one disk option')
    disk_mode = ''
    token = ''
    if disk_root is not None:
        disk, token = load_disk(disk_root)
    elif new_disk is not None:
        disk, token = create_disk(new_disk)
        disk_mode = 'first'
    elif verify_disk is not None:
        disk, token = load_disk(verify_disk)
        disk_mode = 'verify'
    elif disk is not None:
        disk = existing_disk(disk)
        if disk.with_name(disk.name + '.owner.json').is_file():
            disk, token = load_disk(disk)
    bootargs = ''
    marks = MARKS
    if disk_root is not None:
        if root_stage not in (None, 'first', 'verify'):
            raise ValueError('unknown disk-root stage')
        stage = root_stage or ('shell' if shell else 'verify')
        bootargs = 'AEGIS_ROOT=%s AEGIS_BOOT=%s' % (token, stage)
        if stage == 'shell':
            marks = ('Linux version 6.6', 'linux uname Linux 6.6', 'linux init ok',
                     'diskroot Bash console ready') + DISK_ROOT_MARKS
        else:
            boot_number = 1 if stage == 'first' else 2
            marks += DISK_ROOT_MARKS + ('linux bash ok', 'linux pthread ok',
                                       'diskroot persistence boot%d PASS' % boot_number)
    if disk_mode:
        bootargs = 'aegis.disk-mode=%s aegis.disk-token=%s' % (disk_mode, token)
        boot_number = 1 if disk_mode == 'first' else 2
        marks += ('disk-persistence boot%d PASS owner=%s' % (boot_number, token),)
    if export_disk is not None and input_tar is None:
        raise ValueError('export device requires the read-only input tar at /dev/vdb')
    if input_tar is not None:
        input_tar = existing_disk(input_tar)
        if disk is None:
            raise ValueError('input tar requires a data disk at /dev/vda')
    dest = recover(rootfs, shell, bootargs, image)
    # SBI implements TIME but does not enable menvcfg.STCE; use SBI timers, not SSTC.
    cmd = [QEMU, '-M', 'virt', '-cpu', 'rv64,sstc=false', '-m', '2G', '-smp', '1', '-nographic',
           '-bios', str(dest / 'sbi.bin')]
    if disk is not None:
        device = 'virtio-blk-device,drive=aegisdisk'
        if token:
            device += ',serial=' + token
        cmd += ['-drive', 'file=%s,format=raw,if=none,id=aegisdisk' % disk,
                '-device', device]
    if input_tar is not None:
        cmd += ['-drive', 'file=%s,format=raw,if=none,readonly=on,id=aegisinput' % input_tar,
                '-device', 'virtio-blk-device,drive=aegisinput,serial=aegis-sources']
    if export_disk is not None:
        export_disk = export_disk.parent.resolve() / export_disk.name
        if ',' in str(export_disk):
            raise ValueError('export path must not contain commas')
        with export_disk.open('xb') as raw:
            raw.truncate(768 * 1024**2)
        cmd += ['-drive', 'file=%s,format=raw,if=none,id=aegisexport' % export_disk,
                '-device', 'virtio-blk-device,drive=aegisexport,serial=aegis-export']
    for name, addr in [('Image', IMAGE_ADDR), ('initramfs.cpio', INITRD_ADDR),
                       ('bootinfo.bin', BOOTINFO_ADDR)]:
        cmd += ['-device', 'loader,file=%s,addr=%#x,force-raw=on' % (dest / name, addr)]
    proc = subprocess.Popen(cmd, stdin=None if shell else subprocess.DEVNULL, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    data = bytearray()
    end = float('inf') if shell else time.monotonic() + timeout
    try:
        with (OUT / 'uart-linux.log').open('wb') as log:
            while time.monotonic() < end:
                ready, _, _ = select.select([proc.stdout], [], [], 0.5)
                if ready:
                    chunk = os.read(proc.stdout.fileno(), 65536)
                    if not chunk:
                        break
                    data.extend(chunk)
                    log.write(chunk)
                    log.flush()
                    sys.stdout.buffer.write(chunk)
                    sys.stdout.flush()
                    if any(x in data for x in (b'Kernel panic', b'Oops:', b'linux init fail', b'sbi fail')):
                        raise RuntimeError('Linux boot failure; see uart-linux.log')
                elif proc.poll() is not None:
                    break
            else:
                raise TimeoutError('Linux boot timeout')
        proc.wait(timeout=10)
        missing = [s for s in marks if s.encode() not in data]
        if missing or proc.returncode:
            raise RuntimeError('QEMU exit %s; missing %r' % (proc.returncode, missing))
        print('linux boot ok')
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        proc.stdout.close()


def parse_args(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument('--test', action='store_true')
    mode.add_argument('--shell', action='store_true')
    default_rootfs = OUT / 'rootfs-linux-ready.tar'
    if not default_rootfs.is_file():
        default_rootfs = OUT / 'rootfs-linux-build.tar'
    ap.add_argument('--rootfs', type=Path)
    disks = ap.add_mutually_exclusive_group()
    disks.add_argument('--disk', type=Path, help='attach existing raw disk without formatting')
    disks.add_argument('--new-disk', type=Path, help='exclusively create blank 4 GiB disk; run boot1')
    disks.add_argument('--verify-disk', type=Path, help='load owned disk metadata; run boot2')
    disks.add_argument('--disk-root', type=Path, help='boot the owned ext4 disk as the real root')
    ap.add_argument('--root-stage', choices=('first', 'verify'), help='disk-root persistence test stage')
    ap.add_argument('--timeout', type=int, default=600)
    ap.add_argument('--image', type=Path, help='boot an alternative guest-built RV64 Image')
    ap.add_argument('--input-tar', type=Path, help='read-only source/recipe tar at /dev/vdb')
    ap.add_argument('--export-disk', type=Path, help='exclusively create 768 MiB export at /dev/vdc')
    args = ap.parse_args(argv)
    if args.rootfs is None:
        args.rootfs = OUT / 'rootfs-linux-diskroot.tar' if args.disk_root else default_rootfs
    if args.timeout <= 0:
        ap.error('--timeout must be positive')
    if args.input_tar and not args.disk:
        ap.error('--input-tar requires --disk')
    if args.export_disk and not args.input_tar:
        ap.error('--export-disk requires --input-tar')
    if args.root_stage and not args.disk_root:
        ap.error('--root-stage requires --disk-root')
    return args


if __name__ == '__main__':
    a = parse_args()
    test(a.rootfs, a.timeout, a.shell, a.disk, new_disk=a.new_disk, verify_disk=a.verify_disk,
         image=a.image, input_tar=a.input_tar, export_disk=a.export_disk,
         disk_root=a.disk_root, root_stage=a.root_stage)
