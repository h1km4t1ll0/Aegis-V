#!/usr/bin/env python3
"""Map Simon guest RAM in a live QEMU process and plant ramfs files."""
import os
import re
import struct
import subprocess

GUEST_RAM = 0x80000000
FILE_NAME_BASE = 0x80920000
FILE_NAME_SLOT = 0x400
FILE_NAME_COUNT = 512
FD_BASE = 0x80820000
FILE_PTR_GUEST = 0x80802418
USER_CODE_BASE = 0x82000000
USER_CODE_END = 0x84000000


def guest_to_host(guest, ram_host):
    """Translate guest physical address to QEMU host VA.

    @param guest - Guest physical address
    @param ram_host - Host mapping of guest RAM base
    @returns Host virtual address
    """
    return ram_host + (guest - GUEST_RAM)


def lldb_cmds(pid, cmds, timeout=180):
    """Run lldb batch commands against QEMU.

    @param pid - QEMU PID
    @param cmds - lldb command strings
    @param timeout - Seconds
    @returns None
    @throws RuntimeError On lldb failure
    """
    args = ["lldb", "-p", str(pid), "-b"]
    for cmd in cmds:
        args += ["-o", cmd]
    args += ["-o", "detach", "-o", "quit"]
    proc = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
    if proc.returncode != 0:
        raise RuntimeError((proc.stderr or proc.stdout or "")[-800:])


def find_ram_host(pid):
    """Find host VA of guest RAM (first VM_ALLOCATE at/above 0x300000000).

    @param pid - QEMU PID
    @returns Host virtual address
    @throws RuntimeError If no mapping is found
    """
    out = subprocess.check_output(["vmmap", str(pid)], text=True, errors="replace")
    best = None
    for line in out.splitlines():
        match = re.search(
            r"(?:0x)?([0-9a-fA-F]{8,})-(?:0x)?([0-9a-fA-F]{8,})",
            line,
        )
        if not match:
            continue
        addr = int(match.group(1), 16)
        if "reserved" in line.lower() or "SM=NUL" in line:
            continue
        if addr >= 0x300000000 and "VM_ALLOCATE" in line:
            if best is None or addr < best:
                best = addr
    if best is None:
        raise RuntimeError("no guest RAM mapping >= 0x300000000")
    return best


def skip_user_code(ptr):
    """Bump a guest file pointer past the `do` execute window.

    @param ptr - Guest address
    @returns Safe guest address
    """
    if USER_CODE_BASE <= ptr < USER_CODE_END:
        return USER_CODE_END
    return ptr


def plant_files(pid, ram_host, files, dest_base):
    """Write named blobs into ramfs slots starting at dest_base.

    @param pid - QEMU PID
    @param ram_host - Host mapping of guest RAM base
    @param files - Sequence of (name, bytes)
    @param dest_base - Guest address for the first body
    @returns Next free guest address
    """
    names_host = guest_to_host(FILE_NAME_BASE, ram_host)
    fds_host = guest_to_host(FD_BASE, ram_host)
    tmp = "/tmp/simon_plant"
    os.makedirs(tmp, exist_ok=True)
    lldb_cmds(pid, [
        "memory read --outfile %s/names.bin --binary --force --count %d %s" % (
            tmp, FILE_NAME_COUNT * FILE_NAME_SLOT, hex(names_host)),
        "memory read --outfile %s/fds.bin --binary --force --count %d %s" % (
            tmp, FILE_NAME_COUNT * 16, hex(fds_host)),
    ])
    names = open(tmp + "/names.bin", "rb").read()
    free = [
        i for i in range(FILE_NAME_COUNT)
        if not names[i * FILE_NAME_SLOT:(i + 1) * FILE_NAME_SLOT].split(b"\0", 1)[0]
    ]
    if len(free) < len(files):
        raise RuntimeError("need %d free slots, have %d" % (len(files), len(free)))
    ptr = skip_user_code((dest_base + 15) & ~15)
    cmds = []
    for idx, (name, data) in enumerate(files):
        slot = free[idx]
        open("%s/n%d.bin" % (tmp, idx), "wb").write(name.encode("ascii") + b"\0")
        open("%s/b%d.bin" % (tmp, idx), "wb").write(data)
        open("%s/f%d.bin" % (tmp, idx), "wb").write(struct.pack("<QQ", ptr, len(data)))
        cmds += [
            "memory write --infile %s/n%d.bin %s" % (
                tmp, idx, hex(guest_to_host(FILE_NAME_BASE + slot * FILE_NAME_SLOT, ram_host))),
            "memory write --infile %s/f%d.bin %s" % (
                tmp, idx, hex(guest_to_host(FD_BASE + slot * 16, ram_host))),
            "memory write --infile %s/b%d.bin %s" % (
                tmp, idx, hex(guest_to_host(ptr, ram_host))),
        ]
        print("plant %s slot %d @ %s len %d" % (name, slot, hex(ptr), len(data)), flush=True)
        ptr = skip_user_code((ptr + len(data) + 15) & ~15)
    open(tmp + "/fp.bin", "wb").write(struct.pack("<Q", ptr))
    cmds.append("memory write --infile %s/fp.bin %s" % (
        tmp, hex(guest_to_host(FILE_PTR_GUEST, ram_host))))
    lldb_cmds(pid, cmds, timeout=300)
    return ptr
