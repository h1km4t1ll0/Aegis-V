#!/usr/bin/env python3
"""Compile 10-k with tcc-pnut in Simon, then boot the raw firmware in QEMU."""
import hashlib
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dump_guest_files import dump_named
from guest_ram import (
    FILE_NAME_BASE,
    FILE_NAME_COUNT,
    FILE_NAME_SLOT,
    find_ram_host,
    guest_to_host,
    lldb_cmds,
    plant_files,
)
from pack_kfs import pack
from raw_to_elf import wrap
from run_m2_qemu_test import Qemu, ROOT
from stage_images import CACHE, IMAGES

KDIR = os.path.join(ROOT, "stages", "10-k")
KOUT = os.path.join(ROOT, "build", "k-out")
PNUT_TCC = os.path.join(ROOT, "build", "pnut_tcc.c")
PNUT_BIN = os.path.join(CACHE, "pnut.bin")
FIRMWARE = os.path.join(ROOT, "build", "k-firmware.bin")
STAGE09 = os.path.join(IMAGES, "stage09.bin")
BUILD_QMP = os.path.join(ROOT, "build", "qemu-k-build-qmp.sock")
RUN_QMP = os.path.join(ROOT, "build", "qemu-k-run-qmp.sock")
KERNEL_FILES = (
    "k.h",
    "entry.s",
    "uart.c",
    "klib.c",
    "kmain.c",
    "k.rsp",
    "trap.c",
    "syscall.c",
    "proc.c",
    "elf.c",
    "kmem.c",
    "ramfs.c",
)
TCC_PNUT = os.path.join(CACHE, "tcc-pnut.bin")
HI_FILES = ("hi.s", "hi.rsp")
READLEN_FILES = ("sys.s", "readlen.c", "readlen.rsp")
USER_C_FILES = ("crt.s", "libc.c", "init.c", "cmp.c")
TCC_C = os.path.join(ROOT, "stages", "08-tcc", "tcc.c")
TCC_DIR = os.path.join(ROOT, "stages", "08-tcc")
USER_DIR = os.path.join(KDIR, "user")
TCC_SRC = (
    "tcc.c",
    "libtcc.c",
    "tccpp.c",
    "tccgen.c",
    "tccelf.c",
    "tccasm.c",
    "riscv64-gen.c",
    "riscv64-link.c",
    "riscv64-asm.c",
    "tcc.h",
    "libtcc.h",
    "tcctok.h",
    "riscv64-tok.h",
    "elf.h",
    "stab.h",
    "stab.def",
)


def _check_seed_elf(blob, name):
    """Seed tcc must emit a loadable ELF64 PT_LOAD (not phoff=0)."""
    import struct

    if len(blob) < 64 + 56:
        raise SystemExit("%s too small for EHDR+PHDR" % name)
    e_entry, e_phoff, e_shoff = struct.unpack_from("<QQQ", blob, 24)
    e_ehsize, e_phentsize, e_phnum = struct.unpack_from("<HHH", blob, 52)
    print(
        "[tcc-self-k] %s ehdr entry=0x%x phoff=0x%x shoff=0x%x "
        "ehsize=%d phentsize=%d phnum=%d"
        % (name, e_entry, e_phoff, e_shoff, e_ehsize, e_phentsize, e_phnum),
        flush=True,
    )
    if e_phoff == 0 or e_phentsize == 0 or e_phnum == 0:
        print("[tcc-self-k] %s ehdr hex %s" % (name, blob[:64].hex()), flush=True)
        raise SystemExit("%s still has broken ELF header (phoff=0)" % name)
    off = e_phoff
    p_type, p_flags = struct.unpack_from("<II", blob, off)
    p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack_from(
        "<QQQQQQ", blob, off + 8
    )
    print(
        "[tcc-self-k] %s phdr type=%d flags=%d off=0x%x va=0x%x "
        "filesz=0x%x memsz=0x%x"
        % (name, p_type, p_flags, p_offset, p_vaddr, p_filesz, p_memsz),
        flush=True,
    )
    if p_type != 1:
        raise SystemExit("%s PHDR0 is not PT_LOAD" % name)
    if p_vaddr != 0x10000:
        raise SystemExit("%s PT_LOAD p_vaddr=0x%x want 0x10000" % (name, p_vaddr))
    if p_offset != 0x1000:
        raise SystemExit("%s PT_LOAD p_offset=0x%x want 0x1000" % (name, p_offset))
    if e_entry == 0:
        raise SystemExit("%s e_entry is 0" % name)


def blank_ramfs_names(pid, ram_host, wanted):
    """Zero first byte of matching ramfs names so plant_files is the lookup hit.

    Simon find_file_by_name returns the first slot. Stage09 already has
    pnut_tcc.c from files.pl; a second plant would be ignored without this.
    """
    tmp = "/tmp/simon_k_blank"
    os.makedirs(tmp, exist_ok=True)
    names_host = guest_to_host(FILE_NAME_BASE, ram_host)
    lldb_cmds(
        pid,
        [
            "memory read --outfile %s/names.bin --binary --force --count %d %s"
            % (tmp, FILE_NAME_COUNT * FILE_NAME_SLOT, hex(names_host)),
        ],
    )
    names = open(tmp + "/names.bin", "rb").read()
    cmds = []
    for i in range(FILE_NAME_COUNT):
        raw = names[i * FILE_NAME_SLOT : (i + 1) * FILE_NAME_SLOT].split(b"\0", 1)[0]
        if not raw:
            continue
        n = raw.decode("ascii", "replace")
        if n not in wanted:
            continue
        path = "%s/z%d.bin" % (tmp, i)
        open(path, "wb").write(b"\0")
        cmds.append(
            "memory write --infile %s %s"
            % (path, hex(guest_to_host(FILE_NAME_BASE + i * FILE_NAME_SLOT, ram_host)))
        )
        print("blank ramfs name %s slot %d" % (n, i), flush=True)
    if cmds:
        lldb_cmds(pid, cmds, timeout=60)


def uart_fail(buf, msg):
    tail = buf[-4000:] if buf else ""
    print("--- UART tail ---", flush=True)
    print(tail, flush=True)
    print("--- end UART tail ---", flush=True)
    raise SystemExit("%s\n%s" % (msg, tail))


def run_boot():
    if not os.path.isfile(STAGE09):
        raise SystemExit("missing %s" % STAGE09)
    if not os.path.isfile(PNUT_TCC):
        raise SystemExit("missing %s; run pack.sh + extract_files_pl.py" % PNUT_TCC)
    if not os.path.isfile(PNUT_BIN):
        raise SystemExit("missing %s" % PNUT_BIN)
    os.makedirs(CACHE, exist_ok=True)
    os.makedirs(KOUT, exist_ok=True)

    pnut_tcc = open(PNUT_TCC, "rb").read()
    pnut_bin = open(PNUT_BIN, "rb").read()
    print(
        "[boot] pnut_tcc.c %d bytes pnut.bin %d bytes"
        % (len(pnut_tcc), len(pnut_bin)),
        flush=True,
    )

    q = Qemu(image=STAGE09, memory="6G", smp="1", qmp_sock=BUILD_QMP)
    try:
        print("[boot] qemu pid=%s image=%s" % (q.proc.pid, STAGE09), flush=True)
        q.wait_prompt(30, "boot")
        q.drain_quiet()
        # stage09.bin is 15MiB; STAGE2 at 0x80800000 overwrites trailing
        # packed hex0 (pnut.hex0). Plant cached pnut.bin like --tcc-self.
        ram_host = find_ram_host(q.proc.pid)
        blank_ramfs_names(q.proc.pid, ram_host, ("pnut_tcc.c", "pnut.bin"))
        plant_files(
            q.proc.pid,
            ram_host,
            [("pnut.bin", pnut_bin), ("pnut_tcc.c", pnut_tcc)],
            dest_base=0x84000000,
        )
        print("\n[note] pnut compiling pnut_tcc.c (Ttext guard)", flush=True)
        q.cmd("do pnut.bin pnut_tcc.c -o tcc-pnut.bin", 600)
        uart = q.last_cmd_buf
        print("--- pnut uart ---", flush=True)
        print(uart[-16000:], flush=True)
        print("--- end pnut uart ---", flush=True)
        if "100%" not in uart:
            uart_fail(uart, "pnut did not finish pnut_tcc.c")
        ram_host = find_ram_host(q.proc.pid)
        dumped = dump_named(
            q.proc.pid, CACHE, ["tcc-pnut.bin"], ram_host=ram_host
        )
        print("[boot] tcc-pnut.bin dump %s" % dumped, flush=True)
        tcc_path = os.path.join(CACHE, "tcc-pnut.bin")
        tcc_size = os.path.getsize(tcc_path)
        print("[boot] tcc-pnut.bin size %d" % tcc_size, flush=True)
        if tcc_size < 50000:
            raise SystemExit("tcc-pnut.bin too small (%d)" % tcc_size)
        q.cmd("do tcc-pnut.bin -version", 20, expect="tcc version 0.9.26")

        kfiles = []
        for name in KERNEL_FILES:
            blob = open(os.path.join(KDIR, name), "rb").read()
            kfiles.append((name, blob))
            print("[plant] %s %d bytes" % (name, len(blob)), flush=True)
        plant_files(
            q.proc.pid,
            ram_host,
            kfiles,
            dest_base=0x85000000,
        )
        print("\n[note] tcc-pnut @k.rsp", flush=True)
        q.cmd("do tcc-pnut.bin @k.rsp", 120)
        uart = q.last_cmd_buf
        print("--- k.rsp uart ---", flush=True)
        print(uart[-8000:], flush=True)
        print("--- end k.rsp uart ---", flush=True)
        if "error:" in uart or "undefined" in uart:
            uart_fail(uart, "kernel compile failed")
        ram_host = find_ram_host(q.proc.pid)
        dumped = dump_named(
            q.proc.pid, KOUT, ["kernel.bin"], ram_host=ram_host
        )
        print("[boot] kernel.bin dump %s" % dumped, flush=True)
        kpath = os.path.join(KOUT, "kernel.bin")
        kernel = open(kpath, "rb").read()
        print(
            "[boot] kernel.bin size %d head %s"
            % (len(kernel), kernel[:16].hex()),
            flush=True,
        )
        if len(kernel) < 256:
            raise SystemExit("kernel.bin too small (%d)" % len(kernel))
        firmware = kernel.ljust(0x200000, b"\0")
        open(FIRMWARE, "wb").write(firmware)
        print("[boot] wrote %s %d bytes" % (FIRMWARE, len(firmware)), flush=True)
    finally:
        q.close()

    q2 = Qemu(image=FIRMWARE, memory="1G", smp="1", qmp_sock=RUN_QMP)
    try:
        print("[boot] second qemu pid=%s firmware=%s" % (q2.proc.pid, FIRMWARE), flush=True)
        deadline = time.time() + 10
        while time.time() < deadline:
            if "k 10-k" in q2.buf:
                print("[boot] UART k 10-k", flush=True)
                print(q2.buf[-2000:], flush=True)
                return
            q2.read_more(min(0.5, max(0.05, deadline - time.time())))
        uart_fail(q2.buf, "no k 10-k in UART within 10s")
    finally:
        q2.close()


def plant_named(q, ram_host, dest_base, items):
    files = []
    for name, blob in items:
        files.append((name, blob))
        print("[plant] %s %d bytes" % (name, len(blob)), flush=True)
    plant_files(q.proc.pid, ram_host, files, dest_base=dest_base)


def load_kernel_sources():
    items = []
    for name in KERNEL_FILES:
        blob = open(os.path.join(KDIR, name), "rb").read()
        items.append((name, blob))
    return items


def compile_rsp(q, rsp, timeout, label):
    print("\n[note] tcc-pnut %s" % rsp, flush=True)
    q.cmd("do tcc-pnut.bin %s" % rsp, timeout)
    uart = q.last_cmd_buf
    print("--- %s uart ---" % label, flush=True)
    print(uart[-8000:], flush=True)
    print("--- end %s uart ---" % label, flush=True)
    if "error:" in uart or "undefined" in uart or "Expected" in uart:
        uart_fail(uart, "%s compile failed" % label)
    return uart


def run_second_qemu(firmware, marker):
    q2 = Qemu(image=firmware, memory="1G", smp="1", qmp_sock=RUN_QMP)
    try:
        print(
            "[run] second qemu pid=%s firmware=%s marker=%r"
            % (q2.proc.pid, firmware, marker),
            flush=True,
        )
        deadline = time.time() + 10
        while time.time() < deadline:
            have_k = "k 10-k" in q2.buf
            have_m = marker in q2.buf
            if have_k and have_m:
                print("[run] UART k 10-k", flush=True)
                print("[run] UART marker %r" % marker, flush=True)
                print(q2.buf[-2000:], flush=True)
                return
            q2.read_more(min(0.5, max(0.05, deadline - time.time())))
        uart_fail(
            q2.buf,
            "no k 10-k+%r in UART within 10s" % marker,
        )
    finally:
        q2.close()


def run_hi():
    if not os.path.isfile(STAGE09):
        raise SystemExit("missing %s" % STAGE09)
    if not os.path.isfile(TCC_PNUT):
        raise SystemExit("missing %s" % TCC_PNUT)
    if not os.path.isfile(PNUT_BIN):
        raise SystemExit("missing %s" % PNUT_BIN)
    os.makedirs(CACHE, exist_ok=True)
    os.makedirs(KOUT, exist_ok=True)

    tcc_bin = open(TCC_PNUT, "rb").read()
    pnut_bin = open(PNUT_BIN, "rb").read()
    print(
        "[hi] cached tcc-pnut.bin %d bytes pnut.bin %d bytes"
        % (len(tcc_bin), len(pnut_bin)),
        flush=True,
    )
    if len(tcc_bin) < 50000:
        raise SystemExit("tcc-pnut.bin too small (%d)" % len(tcc_bin))

    q = Qemu(image=STAGE09, memory="6G", smp="1", qmp_sock=BUILD_QMP)
    try:
        print("[hi] qemu pid=%s image=%s" % (q.proc.pid, STAGE09), flush=True)
        q.wait_prompt(30, "boot")
        q.drain_quiet()
        ram_host = find_ram_host(q.proc.pid)
        blank_ramfs_names(
            q.proc.pid,
            ram_host,
            ("tcc-pnut.bin", "pnut.bin") + KERNEL_FILES + HI_FILES
            + ("kernel.bin", "init.elf"),
        )
        plant_files(
            q.proc.pid,
            ram_host,
            [("pnut.bin", pnut_bin), ("tcc-pnut.bin", tcc_bin)],
            dest_base=0x84000000,
        )
        q.cmd("do tcc-pnut.bin -version", 20, expect="tcc version 0.9.26")

        kitems = load_kernel_sources()
        for name in HI_FILES:
            blob = open(os.path.join(KDIR, "user", name), "rb").read()
            kitems.append((name, blob))
        plant_named(q, ram_host, 0x85000000, kitems)

        compile_rsp(q, "@k.rsp", 120, "k.rsp")
        compile_rsp(q, "@hi.rsp", 60, "hi.rsp")
        ram_host = find_ram_host(q.proc.pid)
        dumped = dump_named(
            q.proc.pid, KOUT, ["kernel.bin", "init.elf"], ram_host=ram_host
        )
        print("[hi] dump %s" % dumped, flush=True)
        kpath = os.path.join(KOUT, "kernel.bin")
        ipath = os.path.join(KOUT, "init.elf")
        kernel = open(kpath, "rb").read()
        init_elf = open(ipath, "rb").read()
        print(
            "[hi] kernel.bin size %d head %s"
            % (len(kernel), kernel[:16].hex()),
            flush=True,
        )
        print(
            "[hi] init.elf size %d head %s"
            % (len(init_elf), init_elf[:16].hex()),
            flush=True,
        )
        if len(kernel) < 256:
            raise SystemExit("kernel.bin too small (%d)" % len(kernel))
        if init_elf[:4] != b"\x7fELF":
            raise SystemExit("init.elf is not ELF: %s" % init_elf[:16].hex())
        ramfs = pack([("/bin/init", init_elf)])
        open(os.path.join(KOUT, "ramfs.bin"), "wb").write(ramfs)
        firmware = kernel.ljust(0x200000, b"\0") + ramfs
        open(FIRMWARE, "wb").write(firmware)
        print("[hi] wrote %s %d bytes" % (FIRMWARE, len(firmware)), flush=True)
    finally:
        q.close()

    run_second_qemu(FIRMWARE, "hi")


def run_readlen():
    if not os.path.isfile(STAGE09):
        raise SystemExit("missing %s" % STAGE09)
    if not os.path.isfile(TCC_PNUT):
        raise SystemExit("missing %s" % TCC_PNUT)
    if not os.path.isfile(PNUT_BIN):
        raise SystemExit("missing %s" % PNUT_BIN)
    if not os.path.isfile(TCC_C):
        raise SystemExit("missing %s" % TCC_C)
    os.makedirs(CACHE, exist_ok=True)
    os.makedirs(KOUT, exist_ok=True)

    tcc_bin = open(TCC_PNUT, "rb").read()
    pnut_bin = open(PNUT_BIN, "rb").read()
    tcc_c = open(TCC_C, "rb").read()
    host_size = os.path.getsize(TCC_C)
    marker = "tcc.c %d" % host_size
    print(
        "[readlen] cached tcc-pnut.bin %d bytes pnut.bin %d bytes tcc.c %d"
        % (len(tcc_bin), len(pnut_bin), host_size),
        flush=True,
    )
    if len(tcc_bin) < 50000:
        raise SystemExit("tcc-pnut.bin too small (%d)" % len(tcc_bin))
    if len(tcc_c) != host_size:
        raise SystemExit("tcc.c size mismatch")

    q = Qemu(image=STAGE09, memory="6G", smp="1", qmp_sock=BUILD_QMP)
    try:
        print("[readlen] qemu pid=%s image=%s" % (q.proc.pid, STAGE09), flush=True)
        q.wait_prompt(30, "boot")
        q.drain_quiet()
        ram_host = find_ram_host(q.proc.pid)
        blank_ramfs_names(
            q.proc.pid,
            ram_host,
            ("tcc-pnut.bin", "pnut.bin") + KERNEL_FILES + READLEN_FILES
            + ("kernel.bin", "init.elf"),
        )
        plant_files(
            q.proc.pid,
            ram_host,
            [("pnut.bin", pnut_bin), ("tcc-pnut.bin", tcc_bin)],
            dest_base=0x84000000,
        )
        q.cmd("do tcc-pnut.bin -version", 20, expect="tcc version 0.9.26")

        kitems = load_kernel_sources()
        for name in READLEN_FILES:
            blob = open(os.path.join(KDIR, "user", name), "rb").read()
            kitems.append((name, blob))
        plant_named(q, ram_host, 0x85000000, kitems)

        compile_rsp(q, "@k.rsp", 120, "k.rsp")
        compile_rsp(q, "@readlen.rsp", 60, "readlen.rsp")
        ram_host = find_ram_host(q.proc.pid)
        dumped = dump_named(
            q.proc.pid, KOUT, ["kernel.bin", "init.elf"], ram_host=ram_host
        )
        print("[readlen] dump %s" % dumped, flush=True)
        kpath = os.path.join(KOUT, "kernel.bin")
        ipath = os.path.join(KOUT, "init.elf")
        kernel = open(kpath, "rb").read()
        init_elf = open(ipath, "rb").read()
        print(
            "[readlen] kernel.bin size %d head %s"
            % (len(kernel), kernel[:16].hex()),
            flush=True,
        )
        print(
            "[readlen] init.elf size %d head %s"
            % (len(init_elf), init_elf[:16].hex()),
            flush=True,
        )
        if len(kernel) < 256:
            raise SystemExit("kernel.bin too small (%d)" % len(kernel))
        if init_elf[:4] != b"\x7fELF":
            raise SystemExit("init.elf is not ELF: %s" % init_elf[:16].hex())
        ramfs = pack([("/bin/init", init_elf), ("/src/tcc/tcc.c", tcc_c)])
        open(os.path.join(KOUT, "ramfs.bin"), "wb").write(ramfs)
        firmware = kernel.ljust(0x200000, b"\0") + ramfs
        open(FIRMWARE, "wb").write(firmware)
        print("[readlen] wrote %s %d bytes" % (FIRMWARE, len(firmware)), flush=True)
        print("[readlen] expect %r" % marker, flush=True)
    finally:
        q.close()

    run_second_qemu(FIRMWARE, marker)


def _git_repo_root():
    return subprocess.check_output(
        ["git", "-C", ROOT, "rev-parse", "--show-toplevel"]
    ).decode("ascii").rstrip("\n")


def _tcc_src_from_head(name):
    repo = _git_repo_root()
    rel = os.path.relpath(os.path.join(TCC_DIR, name), repo).replace("\\", "/")
    return subprocess.check_output(["git", "-C", repo, "show", "HEAD:" + rel])


def collect_kfs_self(wt_names=()):
    ents = []
    inc = os.path.join(USER_DIR, "include")
    for root, _dirs, files in os.walk(inc):
        for fn in files:
            full = os.path.join(root, fn)
            rel = os.path.relpath(full, inc).replace("\\", "/")
            ents.append(("/include/" + rel, open(full, "rb").read()))
    for name in TCC_SRC:
        if name in wt_names:
            data = open(os.path.join(TCC_DIR, name), "rb").read()
            src = "working-tree"
        else:
            data = _tcc_src_from_head(name)
            src = "HEAD"
        ents.append(("/src/tcc/" + name, data))
        print(
            "[tcc-self-k] KFS /src/tcc/%s %s sha256 %s"
            % (name, src, hashlib.sha256(data).hexdigest()),
            flush=True,
        )
    ents.append(
        (
            "/src/tcc/config.h",
            open(os.path.join(USER_DIR, "tcc-config.h"), "rb").read(),
        )
    )
    ents.append(("/lib/crt.s", open(os.path.join(USER_DIR, "crt.s"), "rb").read()))
    ents.append(("/lib/libc.c", open(os.path.join(USER_DIR, "libc.c"), "rb").read()))
    return ents


def run_tcc_self_k():
    if not os.path.isfile(STAGE09):
        raise SystemExit("missing %s" % STAGE09)
    if not os.path.isfile(PNUT_TCC):
        raise SystemExit("missing %s; run pack.sh + extract_files_pl.py" % PNUT_TCC)
    if not os.path.isfile(PNUT_BIN):
        raise SystemExit("missing %s" % PNUT_BIN)
    os.makedirs(CACHE, exist_ok=True)
    os.makedirs(KOUT, exist_ok=True)

    pnut_tcc = open(PNUT_TCC, "rb").read()
    pnut_bin = open(PNUT_BIN, "rb").read()
    print(
        "[tcc-self-k] Task 14 rebuild: pnut_tcc.c %d bytes pnut.bin %d bytes"
        % (len(pnut_tcc), len(pnut_bin)),
        flush=True,
    )

    q = Qemu(image=STAGE09, memory="6G", smp="1", qmp_sock=BUILD_QMP)
    try:
        print("[tcc-self-k] qemu pid=%s image=%s" % (q.proc.pid, STAGE09), flush=True)
        q.wait_prompt(30, "boot")
        q.drain_quiet()
        ram_host = find_ram_host(q.proc.pid)
        blank_ramfs_names(
            q.proc.pid,
            ram_host,
            ("pnut_tcc.c", "pnut.bin", "tcc-pnut.bin")
            + KERNEL_FILES
            + USER_C_FILES
            + ("kernel.bin", "init.elf", "cmp.elf"),
        )
        plant_files(
            q.proc.pid,
            ram_host,
            [("pnut.bin", pnut_bin), ("pnut_tcc.c", pnut_tcc)],
            dest_base=0x84000000,
        )
        print("\n[note] pnut compiling pnut_tcc.c (no float=int)", flush=True)
        q.cmd("do pnut.bin pnut_tcc.c -o tcc-pnut.bin", 600)
        uart = q.last_cmd_buf
        print("--- pnut uart ---", flush=True)
        print(uart[-16000:], flush=True)
        print("--- end pnut uart ---", flush=True)
        if "100%" not in uart:
            uart_fail(
                uart,
                "BLOCKED: pnut cannot compile tcc without #define float int",
            )
        ram_host = find_ram_host(q.proc.pid)
        dumped = dump_named(
            q.proc.pid, CACHE, ["tcc-pnut.bin"], ram_host=ram_host
        )
        print("[tcc-self-k] tcc-pnut.bin dump %s" % dumped, flush=True)
        tcc_bin = open(TCC_PNUT, "rb").read()
        print("[tcc-self-k] tcc-pnut.bin size %d" % len(tcc_bin), flush=True)
        if len(tcc_bin) < 50000:
            raise SystemExit("tcc-pnut.bin too small (%d)" % len(tcc_bin))
        q.cmd("do tcc-pnut.bin -version", 20, expect="tcc version 0.9.26")

        seed_elf = wrap(tcc_bin)
        open(os.path.join(KOUT, "tcc-seed.elf"), "wb").write(seed_elf)
        if seed_elf[:4] != b"\x7fELF":
            raise SystemExit("tcc-seed.elf is not ELF")
        print("[tcc-self-k] tcc-seed.elf %d bytes" % len(seed_elf), flush=True)

        kitems = load_kernel_sources()
        for name in USER_C_FILES:
            blob = open(os.path.join(USER_DIR, name), "rb").read()
            kitems.append((name, blob))
        plant_named(q, ram_host, 0x85000000, kitems)

        compile_rsp(q, "@k.rsp", 120, "k.rsp")
        compile_rsp(
            q,
            "-nostdlib -nostdinc -Wl,-Ttext=0x10000 -Wl,-oformat=elf64-little "
            "-o init.elf crt.s libc.c init.c",
            180,
            "init.elf",
        )
        compile_rsp(
            q,
            "-nostdlib -nostdinc -Wl,-Ttext=0x10000 -Wl,-oformat=elf64-little "
            "-o cmp.elf crt.s libc.c cmp.c",
            180,
            "cmp.elf",
        )
        ram_host = find_ram_host(q.proc.pid)
        dumped = dump_named(
            q.proc.pid,
            KOUT,
            ["kernel.bin", "init.elf", "cmp.elf"],
            ram_host=ram_host,
        )
        print("[tcc-self-k] dump %s" % dumped, flush=True)
        kernel = open(os.path.join(KOUT, "kernel.bin"), "rb").read()
        init_elf = open(os.path.join(KOUT, "init.elf"), "rb").read()
        cmp_elf = open(os.path.join(KOUT, "cmp.elf"), "rb").read()
        print(
            "[tcc-self-k] kernel.bin %d init.elf %d cmp.elf %d"
            % (len(kernel), len(init_elf), len(cmp_elf)),
            flush=True,
        )
        if len(kernel) < 256:
            raise SystemExit("kernel.bin too small (%d)" % len(kernel))
        if init_elf[:4] != b"\x7fELF":
            raise SystemExit("init.elf is not ELF: %s" % init_elf[:16].hex())
        if cmp_elf[:4] != b"\x7fELF":
            raise SystemExit("cmp.elf is not ELF: %s" % cmp_elf[:16].hex())
        _check_seed_elf(init_elf, "init.elf")
        _check_seed_elf(cmp_elf, "cmp.elf")
        ents = [
            ("/bin/init", init_elf),
            ("/bin/tcc", seed_elf),
            ("/bin/cmp", cmp_elf),
        ]
        ents.extend(collect_kfs_self(wt_names=("tccelf.c",)))
        ramfs = pack(ents)
        open(os.path.join(KOUT, "ramfs.bin"), "wb").write(ramfs)
        firmware = kernel.ljust(0x200000, b"\0") + ramfs
        open(FIRMWARE, "wb").write(firmware)
        print(
            "[tcc-self-k] firmware %d bytes ramfs %d"
            % (len(firmware), len(ramfs)),
            flush=True,
        )
    finally:
        q.close()

    q2 = Qemu(image=FIRMWARE, memory="2G", smp="1", qmp_sock=RUN_QMP)
    try:
        print(
            "[tcc-self-k] second qemu pid=%s firmware=%s"
            % (q2.proc.pid, FIRMWARE),
            flush=True,
        )
        deadline = time.time() + 3600
        have_k = False
        have_ver = False
        have_ok = False
        while time.time() < deadline:
            if "k 10-k" in q2.buf:
                have_k = True
            if "tcc version 0.9.26" in q2.buf:
                have_ver = True
            if "cmp ok" in q2.buf:
                have_ok = True
            if "cmp fail" in q2.buf:
                uart_fail(q2.buf, "cmp fail")
            if "panic:" in q2.buf:
                # Wait for the rest of the panic line (path / reason).
                n = 0
                while n < 20 and "\n" not in q2.buf[q2.buf.find("panic:"):]:
                    q2.read_more(0.05)
                    n = n + 1
                try:
                    ram_host = find_ram_host(q2.proc.pid)
                    outp = os.path.join(KOUT, "tcc2-loaded.bin")
                    host = guest_to_host(0x81000000, ram_host)
                    lldb_cmds(q2.proc.pid, [
                        "memory read --outfile %s --binary --force --count %d %s"
                        % (outp, 0x50000, hex(host))
                    ])
                    print("[tcc-self-k] dumped tcc2-loaded.bin %d"
                          % os.path.getsize(outp), flush=True)
                except Exception as exc:
                    print("[tcc-self-k] tcc2 dump failed: %s" % exc, flush=True)
                uart_fail(q2.buf, "panic (k=%s ver=%s ok=%s)" % (have_k, have_ver, have_ok))
            if have_k and have_ver and have_ok:
                print("[tcc-self-k] UART k 10-k", flush=True)
                print("[tcc-self-k] UART tcc version 0.9.26", flush=True)
                print("[tcc-self-k] UART cmp ok", flush=True)
                print(q2.buf[-4000:], flush=True)
                return
            q2.read_more(min(0.5, max(0.05, deadline - time.time())))
        uart_fail(
            q2.buf,
            "no k 10-k + tcc version 0.9.26 + cmp ok (k=%s ver=%s ok=%s)"
            % (have_k, have_ver, have_ok),
        )
    finally:
        q2.close()



if __name__ == "__main__":
    if "--boot" in sys.argv:
        run_boot()
    elif "--hi" in sys.argv:
        run_hi()
    elif "--readlen" in sys.argv:
        run_readlen()
    elif "--tcc-self-k" in sys.argv:
        run_tcc_self_k()
    else:
        raise SystemExit(
            "usage: run_k_qemu_test.py --boot|--hi|--readlen|--tcc-self-k"
        )
