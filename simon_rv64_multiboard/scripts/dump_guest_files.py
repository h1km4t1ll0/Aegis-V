#!/usr/bin/env python3
"""Dump named ramfs files from a live QEMU via lldb."""
import os
import struct
import subprocess
import sys

GUEST_RAM = 0x80000000
FILE_NAME_BASE = 0x80920000
FILE_NAME_SLOT = 0x400
FILE_NAME_COUNT = 512
FD_BASE = 0x80820000
DEFAULT_RAM_HOST = 0x300000000


def guest_to_host(guest, ram_host):
    """Translate guest physical address to QEMU host VA.

    @param guest - Guest physical address
    @param ram_host - Host mapping of guest RAM base
    @returns Host virtual address
    """
    return ram_host + (guest - GUEST_RAM)


def lldb_cmds(pid, cmds, timeout=120):
    """Run lldb batch commands against QEMU.

    @param pid - QEMU PID
    @param cmds - lldb command strings
    @param timeout - Seconds
    @returns Completed process
    @throws RuntimeError On lldb failure
    """
    args = ["lldb", "-p", str(pid), "-b"]
    for cmd in cmds:
        args += ["-o", cmd]
    args += ["-o", "detach", "-o", "quit"]
    proc = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
    if proc.returncode != 0:
        tail = (proc.stderr or proc.stdout or "")[-800:]
        raise RuntimeError(tail)
    return proc


def dump_named(pid, out_dir, wanted, ram_host=DEFAULT_RAM_HOST):
    """Copy named ramfs files from QEMU into out_dir.

    @param pid - QEMU PID
    @param out_dir - Destination directory
    @param wanted - File names to extract
    @param ram_host - Host mapping of guest 0x80000000
    @returns Dict of name -> byte length
    @throws RuntimeError If a requested file is missing
    """
    os.makedirs(out_dir, exist_ok=True)
    names_path = os.path.join(out_dir, "_names.bin")
    fds_path = os.path.join(out_dir, "_fds.bin")
    lldb_cmds(pid, [
        "memory read --outfile %s --binary --force --count %d %s" % (
            names_path, FILE_NAME_COUNT * FILE_NAME_SLOT,
            hex(guest_to_host(FILE_NAME_BASE, ram_host))),
        "memory read --outfile %s --binary --force --count %d %s" % (
            fds_path, FILE_NAME_COUNT * 16,
            hex(guest_to_host(FD_BASE, ram_host))),
    ])
    names = open(names_path, "rb").read()
    fds = open(fds_path, "rb").read()
    by_name = {}
    for i in range(FILE_NAME_COUNT):
        name = names[i * FILE_NAME_SLOT:(i + 1) * FILE_NAME_SLOT].split(b"\0", 1)[0]
        if not name:
            continue
        addr, length = struct.unpack_from("<QQ", fds, i * 16)
        by_name[name.decode("ascii", "replace")] = (i, addr, length)
    found = {}
    cmds = []
    for name in wanted:
        if name not in by_name:
            raise RuntimeError("missing ramfs file: %s" % name)
        slot, addr, length = by_name[name]
        dest = os.path.join(out_dir, name.replace("/", "_"))
        print("dump %s slot %d @ %s len %d" % (name, slot, hex(addr), length), flush=True)
        if length == 0:
            open(dest, "wb").write(b"")
            found[name] = 0
            continue
        cmds.append(
            "memory read --outfile %s --binary --force --count %d %s" % (
                dest, length, hex(guest_to_host(addr, ram_host)))
        )
        found[name] = length
    if cmds:
        lldb_cmds(pid, cmds, timeout=300)
    return found


def main():
    """CLI: dump_guest_files.py PID OUT_DIR name [name...]

    @returns None
    """
    pid = int(sys.argv[1])
    out_dir = sys.argv[2]
    wanted = sys.argv[3:]
    result = dump_named(pid, out_dir, wanted)
    for name, length in result.items():
        print("ok %s %d" % (name, length))


if __name__ == "__main__":
    main()
