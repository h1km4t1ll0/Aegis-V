#!/usr/bin/env python3
"""Dump named Simon ramfs files from a live QEMU process via lldb."""
import os
import struct
import subprocess
import sys

GUEST_RAM = 0x80000000
FILE_NAME_BASE = 0x80920000
FILE_NAME_SLOT = 0x400
FILE_NAME_COUNT = 512
FD_BASE = 0x80820000
STAGE2 = 0x80800000
DEFAULT_RAM_HOST = 0x300000000

WANTED = (
    "tcc.bin",
    "tcc.hex2",
    "tccin.M1",
    "hi.s",
    "tccpp.s",
    "tccgen.s",
    "tccelf.s",
    "libtcc.s",
    "riscv64-gen.s",
    "riscv64-link.s",
    "riscv64-asm.s",
    "tccasm.s",
    "tcc.s",
)


def guest_to_host(guest, ram_host):
    """Translate guest physical address to QEMU host virtual address.

    @param guest - Guest physical address
    @param ram_host - Host mapping of guest RAM base
    @returns Host virtual address
    """
    return ram_host + (guest - GUEST_RAM)


def lldb_read(pid, host_addr, size, dest):
    """Read process memory with lldb into dest.

    @param pid - QEMU PID
    @param host_addr - Host virtual address
    @param size - Byte count
    @param dest - Output path
    @returns None
    @throws RuntimeError If lldb fails
    """
    cmd = (
        f"memory read --outfile {dest} --binary --force "
        f"--count {size} {host_addr:#x}"
    )
    proc = subprocess.run(
        ["lldb", "-p", str(pid), "-b", "-o", cmd, "-o", "detach", "-o", "quit"],
        capture_output=True,
        text=True,
        timeout=90,
        check=False,
    )
    if proc.returncode != 0 or not os.path.isfile(dest) or os.path.getsize(dest) != size:
        tail = (proc.stderr or proc.stdout or "")[-800:]
        raise RuntimeError(f"lldb read {host_addr:#x} size={size}: {tail}")


def dump_named(pid, out_dir, ram_host=DEFAULT_RAM_HOST):
    """Copy WANTED ramfs files from QEMU into out_dir.

    @param pid - QEMU PID
    @param out_dir - Destination directory
    @param ram_host - Host mapping of guest 0x80000000
    @returns Dict of name -> byte length
    @throws RuntimeError If tables cannot be read
    """
    os.makedirs(out_dir, exist_ok=True)
    names_path = os.path.join(out_dir, "_names.bin")
    fds_path = os.path.join(out_dir, "_fds.bin")
    sish_path = os.path.join(out_dir, "_sish.bin")
    lldb_read(pid, guest_to_host(STAGE2, ram_host), 0x4000, sish_path)
    sish = open(sish_path, "rb").read()
    if sish.find(b"SiSH") < 0:
        raise RuntimeError("RAM base check failed: no SiSH in STAGE2")
    lldb_read(
        pid,
        guest_to_host(FILE_NAME_BASE, ram_host),
        FILE_NAME_COUNT * FILE_NAME_SLOT,
        names_path,
    )
    lldb_read(pid, guest_to_host(FD_BASE, ram_host), FILE_NAME_COUNT * 16, fds_path)
    names = open(names_path, "rb").read()
    fds = open(fds_path, "rb").read()
    found = {}
    wanted = set(WANTED)
    for index in range(FILE_NAME_COUNT):
        slot = names[index * FILE_NAME_SLOT : (index + 1) * FILE_NAME_SLOT]
        name = slot.split(b"\0", 1)[0].decode("ascii", "replace")
        if name not in wanted:
            continue
        addr, length = struct.unpack_from("<QQ", fds, index * 16)
        if length == 0 or length > 0x4000000:
            raise RuntimeError(f"{name}: bad length {length} addr={addr:#x}")
        raw_path = os.path.join(out_dir, name)
        lldb_read(pid, guest_to_host(addr, ram_host), length, raw_path)
        found[name] = length
        print(f"[extract] {name}: {length} bytes", flush=True)
    return found


def main():
    """CLI: guest_lldb_dump.py <pid> <out_dir>."""
    if len(sys.argv) != 3:
        raise SystemExit("usage: guest_lldb_dump.py <pid> <out_dir>")
    found = dump_named(int(sys.argv[1]), sys.argv[2])
    if "tcc.bin" not in found:
        raise SystemExit("tcc.bin not in guest FILE_NAME table")
    print(f"[extract] ok {len(found)} files", flush=True)


if __name__ == "__main__":
    main()
