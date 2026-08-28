#!/usr/bin/env python3
"""Simon ramfs: plant and dump named files in a live QEMU via lldb."""
import os
import re
import struct
import subprocess
import sys

GUEST_RAM = 0x80000000
FILE_NAME_BASE = 0x80920000
FILE_NAME_SLOT = 0x400
FILE_NAME_COUNT = 512
FILE_NAME_BYTES = FILE_NAME_COUNT * FILE_NAME_SLOT
FD_BASE = 0x80820000
FD_BYTES = 1024 * 16
FILE_PTR_GUEST = 0x80802418
USER_CODE_BASE = 0x82000000
USER_CODE_END = 0x84000000
DEFAULT_RAM_HOST = 0x300000000


def guest_to_host(guest, ram_host):
    """Translate guest physical address to QEMU host VA."""
    return ram_host + (guest - GUEST_RAM)


def lldb_cmds(pid, cmds, timeout=180):
    """Run lldb batch commands against QEMU."""
    args = ["lldb", "-p", str(pid), "-b"]
    for cmd in cmds:
        args += ["-o", cmd]
    args += ["-o", "detach", "-o", "quit"]
    proc = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
    if proc.returncode != 0:
        raise RuntimeError((proc.stderr or proc.stdout or "")[-800:])
    return proc


def find_ram_host(pid):
    """Find host VA of guest RAM (first VM_ALLOCATE at/above 0x300000000)."""
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
    """Bump a guest file pointer past the `do` execute window."""
    if USER_CODE_BASE <= ptr < USER_CODE_END:
        return USER_CODE_END
    return ptr


def plant_files(pid, ram_host, files, dest_base, update_file_ptr=True):
    """Write named blobs into ramfs slots starting at dest_base."""
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
    if update_file_ptr:
        open(tmp + "/fp.bin", "wb").write(struct.pack("<Q", ptr))
        cmds.append("memory write --infile %s/fp.bin %s" % (
            tmp, hex(guest_to_host(FILE_PTR_GUEST, ram_host))))
    lldb_cmds(pid, cmds, timeout=300)
    return ptr


def dump_named(pid, out_dir, wanted, ram_host=DEFAULT_RAM_HOST):
    """Copy named ramfs files from QEMU into out_dir."""
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
    pid = int(sys.argv[1])
    out_dir = sys.argv[2]
    wanted = sys.argv[3:]
    result = dump_named(pid, out_dir, wanted)
    for name, length in result.items():
        print("ok %s %d" % (name, length))


if __name__ == "__main__":
    main()
