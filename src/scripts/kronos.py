#!/usr/bin/env python3
"""Build Kronos with tcc-pnut in Simon, then kexec it in the same QEMU."""
import hashlib
import io
import os
import re
import struct
import subprocess
import sys
import tarfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lib.guest import (
    FILE_NAME_BASE,
    FILE_NAME_COUNT,
    FILE_NAME_SLOT,
    dump_named,
    find_ram_host,
    guest_to_host,
    lldb_cmds,
    plant_files,
)
from lib.image import pack_kfs as pack
from lib.image import wrap_elf as wrap
from lib.phases import artifacts, phase_image, stem_bin
from lib.qemu import IMAGE, Qemu, Qmp, ROOT

KDIR = os.path.join(ROOT, "src", "stages", "09-kronos")
KSRC = os.path.join(KDIR, "src")
KTESTS = os.path.join(KDIR, "tests")
KOUT = artifacts("09-kronos")
PNUT_BIN = stem_bin("pnut")
FIRMWARE = phase_image("09-kronos")
BUILD_QMP = os.path.join(ROOT, "build", "simon", "qemu-k-build-qmp.sock")
BOOT_MARK = "Kronos"
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
TCC_PNUT = stem_bin("tcc-pnut")
HI_FILES = ("hi.s", "hi.rsp")
READLEN_FILES = ("sys.s", "readlen.c", "readlen.rsp")
USER_C_FILES = ("crt.s", "libc.c", "init.c", "cmp.c")
TCC_C = os.path.join(ROOT, "src", "stages", "07-tcc", "src", "tcc.c")
TCC_DIR = os.path.join(ROOT, "src", "stages", "07-tcc", "src")
TCC_VENDOR = os.path.join(TCC_DIR, "vendor", "tcc-0.9.26")
TCC_FULL_CFG = os.path.join(ROOT, "src", "stages", "10-tcc", "src", "config.h")
HELLO_C = os.path.join(ROOT, "src", "stages", "10-tcc", "tests", "hello.c")
ZEXT_C = os.path.join(ROOT, "src", "stages", "10-tcc", "tests", "mob-zext.c")
TCC_OUT = artifacts("10-tcc")
USER_DIR = os.path.join(KSRC, "user")
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
        timeout=300,
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
    try:
        os.makedirs(KOUT, exist_ok=True)
        path = os.path.join(KOUT, "uart-fail.log")
        open(path, "w").write(buf or "")
        print("[uart] wrote %s %d" % (path, len(buf or "")), flush=True)
    except Exception as e:
        print("[uart] write skip %s" % e, flush=True)
    raise SystemExit("%s\n%s" % (msg, tail))


def load_firmware_at_reset(q, firmware):
    """Reset, then overwrite DRAM at 0x80000000, then continue.

    virt reloads -bios into 0x80000000 on system_reset. Write after
    reset so Kronos is what MROM jumps to. Same QEMU process.
    """
    ram_host = find_ram_host(q.proc.pid)
    tmp = os.path.join(KOUT, "_firmware_load.bin")
    os.makedirs(KOUT, exist_ok=True)
    open(tmp, "wb").write(firmware)
    qmp = Qmp(q.qmp_sock)
    qmp.call("stop")
    qmp.call("system_reset")
    qmp.call("stop")
    host = guest_to_host(0x80000000, ram_host)
    print(
        "[handoff] reset then write firmware %d bytes at 0x80000000"
        % len(firmware),
        flush=True,
    )
    lldb_cmds(
        q.proc.pid,
        ["memory write --infile %s %s" % (tmp, hex(host))],
        timeout=300,
    )
    q.buf = ""
    qmp.call("cont")
    qmp.close()


def dump_snap(q, uart, tag, dest, elf=True):
    """Copy a Kronos extra file whose PA/size were printed as `snap TAG ADDR SIZE`."""
    m = re.search(r"snap %s (0x[0-9a-f]+) (0x[0-9a-f]+)" % tag, uart)
    if not m:
        raise SystemExit("no snap %s in UART" % tag)
    addr = int(m.group(1), 16)
    size = int(m.group(2), 16)
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    qmp = Qmp(q.qmp_sock)
    qmp.call("stop")
    try:
        lldb_cmds(
            q.proc.pid,
            [
                "memory read --outfile %s --binary --force --count %d %s"
                % (dest, size, hex(addr))
            ],
            timeout=300,
        )
    finally:
        qmp.call("cont")
        qmp.close()
    blob = open(dest, "rb").read()
    if elf and blob[:4] != b"\x7fELF":
        raise SystemExit("%s is not ELF: %s" % (dest, blob[:16].hex()))
    print("[snap] %s %s @ %s %d bytes" % (tag, dest, hex(addr), len(blob)), flush=True)
    return blob


def wait_uart(q, markers, timeout, label):
    """Block until every marker appears on UART after Kronos boot."""
    if not hasattr(q, "snaps"):
        q.snaps = {}
    deadline = time.time() + timeout
    have = [False] * len(markers)
    got = 0
    pos = 0
    last_gtyok = -1
    last_self = ""
    last_as = ""
    last_ld = ""
    last_tcc = ""
    last_ok = -1
    last_beat = time.time()
    t0 = time.time()
    while time.time() < deadline:
        gtyok = q.buf.count("gtyok")
        if gtyok != last_gtyok:
            last_gtyok = gtyok
            if gtyok:
                print("[%s] gtyok %d" % (label, gtyok), flush=True)
        n_ok = q.buf.count("self ok")
        if n_ok != last_ok:
            last_ok = n_ok
            if n_ok:
                print("[%s] self ok %d" % (label, n_ok), flush=True)
        idx = q.buf.rfind("self /")
        if idx >= 0:
            line = q.buf[idx:].split("\n", 1)[0]
            if line != last_self:
                last_self = line
                print("[%s] %s (ok %d)" % (label, line, n_ok), flush=True)
        idx_as = q.buf.rfind("as /src/as/")
        if idx_as >= 0:
            line = q.buf[idx_as:].split("\n", 1)[0]
            if line != last_as:
                last_as = line
                print("[%s] %s" % (label, line), flush=True)
        idx_tcc = q.buf.rfind("tcc /")
        if idx_tcc >= 0:
            line = q.buf[idx_tcc:].split("\n", 1)[0]
            if line != last_tcc:
                last_tcc = line
                print("[%s] %s" % (label, line), flush=True)
        idx_ld = q.buf.rfind("ld /src/ld/")
        if idx_ld >= 0:
            line = q.buf[idx_ld:].split("\n", 1)[0]
            if line != last_ld:
                last_ld = line
                print("[%s] %s" % (label, line), flush=True)
        now = time.time()
        if now - last_beat >= 60:
            last_beat = now
            snippet = q.buf[-100:].replace("\n", "\\n")
            print(
                "[%s] beat %ds %s"
                % (label, int(now - t0), snippet),
                flush=True,
            )
        while got < len(markers):
            rel = q.buf.find(markers[got], pos)
            if rel < 0:
                break
            pos = rel + len(markers[got])
            have[got] = True
            print("[%s] got %r" % (label, markers[got]), flush=True)
            got = got + 1
        # snap lines may scroll out of the capped UART buffer later
        for sm in re.finditer(r"snap \S+ 0x[0-9a-f]+ 0x[0-9a-f]+\n", q.buf):
            q.snaps[sm.group(0).split()[1]] = sm.group(0)
        # tcc7 tests after "hello from tcc7", the Tilck build after
        # "t7 done" and the musl build after "tilck done" report every
        # failure first; their errors count once "t7 done" / "tilck done" /
        # "musl done" is on the UART.
        gate = q.buf
        t7 = gate.find("hello from tcc7")
        if t7 >= 0 and "t7 done" not in gate:
            gate = gate[:t7]
        tk = gate.find("t7 done")
        if tk >= 0 and "tilck done" not in gate:
            gate = gate[:tk]
        # the musl build is long: "tilck done" may scroll out of q.buf
        if "tilck done" in q.buf:
            q.musl_window = True
        if getattr(q, "musl_window", False) and "musl done" not in q.buf:
            tm = gate.find("tilck done")
            gate = gate[:tm] if tm >= 0 else ""
        # the seed build (stage 14) after "musl done", the same way
        if "musl done" in q.buf:
            q.seed_window = True
        if getattr(q, "seed_window", False) and "seed done" not in q.buf:
            ts = gate.find("musl done")
            gate = gate[:ts] if ts >= 0 else ""
        if ": error:" in gate or "tcc: error" in gate:
            n = 0
            while n < 80:
                q.read_more(0.05)
                n = n + 1
                if "unrecognizable insn" in q.buf or "internal compiler error" in q.buf:
                    break
            uart_fail(q.buf, "%s: tcc error %s" % (label, have))
        if "gty fldloop" in gate or "gty varloop" in gate or "gty optloop" in gate or "gty gcmax" in gate or "gty err" in gate:
            uart_fail(q.buf, "%s: gengtype fail %s" % (label, have))
        if "\nabort\n" in q.buf:
            n = 0
            while n < 40:
                q.read_more(0.05)
                n = n + 1
                if "\nra=" in q.buf:
                    break
            uart_fail(q.buf, "%s: self fail %s" % (label, have))
        if "cmp fail" in gate or "zext fail" in gate:
            q.read_more(0.5)
            uart_fail(q.buf, "%s: cmp/zext fail %s" % (label, have))
        if "self fail" in gate or "as fail" in gate or "ld fail" in gate or "ld miss" in gate or "as obj miss" in gate or "as parse fail" in gate or "xdup fail" in gate or "intl fail" in gate or "gt fail" in gate or "case4 fail" in gate or "case5 fail" in gate or "case6 fail" in gate or "case7 fail" in gate or "case8s fail" in gate or "casen fail" in gate or "gengen fail" in gate or "modes fail" in gate or "flags fail" in gate or "tcc fail" in gate or "tcc6n fail" in gate or "gen464 fail" in gate or "hello.s fail" in gate or "strlen.s fail" in gate or "strlen.o fail" in gate or "compilation terminated" in gate or "fatal error:" in gate or "as464 fail" in gate or "ld464 fail" in gate or "hello-2 fail" in gate:
            uart_fail(q.buf, "%s: self fail %s" % (label, have))
        if "panic:" in q.buf:
            n = 0
            while n < 20 and "\n" not in q.buf[q.buf.find("panic:") :]:
                q.read_more(0.05)
                n = n + 1
            uart_fail(q.buf, "%s: panic %s" % (label, have))
        if q.proc.poll() is not None:
            uart_fail(q.buf, "%s: qemu exited %s" % (label, q.proc.returncode))
        tail4 = q.buf[-4000:]
        if len(tail4) >= 4000 and tail4.strip("0123456789abcdef") == "":
            uart_fail(q.buf, "%s: uart hex flood %s" % (label, have))
        if got == len(markers):
            for m in markers:
                print("[%s] UART %r" % (label, m), flush=True)
            print(q.buf[-4000:], flush=True)
            return
        for term in ("t7 done", "tilck done", "musl done", "seed done"):
            if term in q.buf and term in markers and got <= markers.index(term):
                uart_fail(q.buf, "%s: %s, missing UART %s" % (label, term, have))
            if term in q.buf and term not in markers and got < len(markers):
                uart_fail(q.buf, "%s: %s, missing UART %s" % (label, term, have))
        anchor = q.buf[-48:] if len(q.buf) >= 48 else q.buf
        anchor_at = len(q.buf) - len(anchor)
        q.read_more(min(0.5, max(0.05, deadline - time.time())))
        if anchor:
            idx = q.buf.rfind(anchor)
            if idx >= 0 and anchor_at > idx:
                pos = max(0, pos - (anchor_at - idx))
    uart_fail(q.buf, "%s: missing UART %s have=%s" % (label, markers, have))


def handoff_kronos(q, firmware, markers, timeout, label):
    load_firmware_at_reset(q, firmware)
    wait_uart(q, markers, timeout, label)


def run_boot():
    if not os.path.isfile(IMAGE):
        raise SystemExit("missing %s" % IMAGE)
    if not os.path.isfile(TCC_PNUT):
        raise SystemExit("missing %s; run src/scripts/simon.py first" % TCC_PNUT)
    if not os.path.isfile(PNUT_BIN):
        raise SystemExit("missing %s" % PNUT_BIN)
    os.makedirs(KOUT, exist_ok=True)

    tcc_bin = open(TCC_PNUT, "rb").read()
    pnut_bin = open(PNUT_BIN, "rb").read()
    print(
        "[boot] cached tcc-pnut.bin %d bytes pnut.bin %d bytes"
        % (len(tcc_bin), len(pnut_bin)),
        flush=True,
    )
    if len(tcc_bin) < 50000:
        raise SystemExit("tcc-pnut.bin too small (%d)" % len(tcc_bin))

    q = Qemu(image=IMAGE, memory="6G", smp="1", qmp_sock=BUILD_QMP)
    try:
        print("[boot] qemu pid=%s image=%s" % (q.proc.pid, IMAGE), flush=True)
        q.wait_prompt(30, "boot")
        q.drain_quiet()
        # Simon image is large; STAGE2 at 0x80800000 overwrites trailing
        # packed hex0. Plant cached tcc-pnut.bin (pnut on smp=1 times out).
        ram_host = find_ram_host(q.proc.pid)
        blank_ramfs_names(q.proc.pid, ram_host, ("tcc-pnut.bin", "pnut.bin"))
        plant_files(
            q.proc.pid,
            ram_host,
            [("pnut.bin", pnut_bin), ("tcc-pnut.bin", tcc_bin)],
            dest_base=0x84000000,
        )
        q.cmd("do tcc-pnut.bin -version", 20, expect="tcc version 0.9.26")

        kfiles = []
        for name in KERNEL_FILES:
            blob = open(os.path.join(KSRC, name), "rb").read()
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
        handoff_kronos(q, firmware, (BOOT_MARK,), 10, "boot")
    finally:
        q.close()


def plant_named(q, ram_host, dest_base, items):
    files = []
    for name, blob in items:
        files.append((name, blob))
        print("[plant] %s %d bytes" % (name, len(blob)), flush=True)
    plant_files(q.proc.pid, ram_host, files, dest_base=dest_base)


def load_kernel_sources():
    items = []
    for name in KERNEL_FILES:
        blob = open(os.path.join(KSRC, name), "rb").read()
        items.append((name, blob))
    return items


def compile_rsp(q, rsp, timeout, label):
    print("\n[note] tcc-pnut %s" % rsp, flush=True)
    q.cmd("do tcc-pnut.bin %s" % rsp, timeout)
    uart = q.last_cmd_buf
    print("--- %s uart ---" % label, flush=True)
    print(uart[-8000:], flush=True)
    print("--- end %s uart ---" % label, flush=True)
    if "error:" in uart or "undefined" in uart or "Expected" in uart or "expected (got" in uart:
        uart_fail(uart, "%s compile failed" % label)
    return uart


def run_hi():
    if not os.path.isfile(IMAGE):
        raise SystemExit("missing %s" % IMAGE)
    if not os.path.isfile(TCC_PNUT):
        raise SystemExit("missing %s" % TCC_PNUT)
    if not os.path.isfile(PNUT_BIN):
        raise SystemExit("missing %s" % PNUT_BIN)
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

    q = Qemu(image=IMAGE, memory="6G", smp="1", qmp_sock=BUILD_QMP)
    try:
        print("[hi] qemu pid=%s image=%s" % (q.proc.pid, IMAGE), flush=True)
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
            blob = open(os.path.join(KTESTS, name), "rb").read()
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
        handoff_kronos(q, firmware, (BOOT_MARK, "hi"), 10, "hi")
    finally:
        q.close()


def run_readlen():
    if not os.path.isfile(IMAGE):
        raise SystemExit("missing %s" % IMAGE)
    if not os.path.isfile(TCC_PNUT):
        raise SystemExit("missing %s" % TCC_PNUT)
    if not os.path.isfile(PNUT_BIN):
        raise SystemExit("missing %s" % PNUT_BIN)
    if not os.path.isfile(TCC_C):
        raise SystemExit("missing %s" % TCC_C)
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

    q = Qemu(image=IMAGE, memory="6G", smp="1", qmp_sock=BUILD_QMP)
    try:
        print("[readlen] qemu pid=%s image=%s" % (q.proc.pid, IMAGE), flush=True)
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
            blob = open(os.path.join(KTESTS, name), "rb").read()
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
        handoff_kronos(q, firmware, (BOOT_MARK, marker), 10, "readlen")
    finally:
        q.close()


def _git_repo_root():
    return subprocess.check_output(
        ["git", "-C", ROOT, "rev-parse", "--show-toplevel"]
    ).decode("ascii").rstrip("\n")


def _tcc_src_from_head(name):
    """Bytes of src/stages/.../tcc source at HEAD.

    HEAD may still use older trees until this flatten is committed:
    simon_rv64_multiboard/stages/07-tcc, then 08-tcc, then src/stages/07-tcc.
    """
    repo = _git_repo_root()
    rel = os.path.relpath(os.path.join(TCC_DIR, name), repo).replace("\\", "/")
    candidates = (
        rel,
        "src/stages/07-tcc/src/" + name,
        "src/stages/07-tcc/" + name,
        "simon_rv64_multiboard/stages/07-tcc/" + name,
        "simon_rv64_multiboard/stages/08-tcc/" + name,
    )
    tried = []
    for path in candidates:
        if path in tried:
            continue
        tried.append(path)
        proc = subprocess.run(
            ["git", "-C", repo, "show", "HEAD:" + path],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
        )
        if proc.returncode == 0:
            return proc.stdout
    raise SystemExit("git HEAD missing %s (tried %s)" % (name, tried))


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


def collect_kfs_tcc_full():
    """Plant fuller TCC: vendor tccpp (float) + packed tccelf (Kronos ELF)."""
    ents = []
    for name in TCC_SRC:
        if name == "tccpp.c":
            path = os.path.join(TCC_VENDOR, name)
        else:
            path = os.path.join(TCC_DIR, name)
        data = open(path, "rb").read()
        if name == "tcc.c":
            data = data.replace(b"tcc version 0.9.26", b"tcc version 0.9.27")
        ents.append(("/src/tcc-full/" + name, data))
        print(
            "[tcc-full] KFS /src/tcc-full/%s %d"
            % (name, len(data)),
            flush=True,
        )
    cfg = open(TCC_FULL_CFG, "rb").read()
    ents.append(("/src/tcc-full/config.h", cfg))
    print("[tcc-full] KFS /src/tcc-full/config.h %d" % len(cfg), flush=True)
    return ents


SOURCES = os.path.join(ROOT, "build", "sources")
TCC7_DIR = os.path.join(ROOT, "src", "stages", "11-tcc7")
TCC7_TAR = os.path.join(SOURCES, "tcc-mob-e23dabed.tar")
TCC7_SHA256 = "7d006a2acaeb459970caf8e1d5f56c6cc98391255998a26c819b446ab7a3ec66"
TCC7_OUT = artifacts("11-tcc7")


def _tar_files(path, sha256):
    """Bytes of every regular file in a pinned tar, keyed by member path."""
    data = open(path, "rb").read()
    got = hashlib.sha256(data).hexdigest()
    if got != sha256:
        raise SystemExit("%s sha256 %s, want %s" % (path, got, sha256))
    out = {}
    with tarfile.open(fileobj=io.BytesIO(data)) as tf:
        for m in tf.getmembers():
            if m.isfile():
                out[m.name] = tf.extractfile(m).read()
    return out


def collect_kfs_tcc7():
    """Plant tcc mob: sources under /src/tcc7, headers under /lib/tcc7/include."""
    files = _tar_files(TCC7_TAR, TCC7_SHA256)
    over = os.path.join(TCC7_DIR, "src", "override")
    if os.path.isdir(over):
        for root, _dirs, names in os.walk(over):
            for fn in names:
                full = os.path.join(root, fn)
                files[os.path.relpath(full, over).replace("\\", "/")] = open(full, "rb").read()
    ents = []
    for name in sorted(files):
        if "/" not in name and name.endswith((".c", ".h", ".def")):
            ents.append(("/src/tcc7/" + name, files[name]))
        elif name.startswith("lib/"):
            ents.append(("/src/tcc7/" + name, files[name]))
        elif name.startswith("include/"):
            ents.append(("/lib/tcc7/" + name, files[name]))
    ents.append(("/src/tcc7/config.h", open(os.path.join(TCC7_DIR, "src", "config.h"), "rb").read()))
    ents.append(("/src/tcc7/compat.c", open(os.path.join(TCC7_DIR, "src", "compat.c"), "rb").read()))
    ents.append(("/src/tcc7/compat-crt.s", open(os.path.join(TCC7_DIR, "src", "compat-crt.s"), "rb").read()))
    compat_inc = os.path.join(TCC7_DIR, "src", "compat-include")
    for root, _dirs, names in os.walk(compat_inc):
        for fn in sorted(names):
            full = os.path.join(root, fn)
            rel = os.path.relpath(full, compat_inc).replace("\\", "/")
            ents.append(("/src/tcc7/compat-include/" + rel, open(full, "rb").read()))
    ents.append(("/src/hello7/hello.c", open(os.path.join(TCC7_DIR, "tests", "hello7.c"), "rb").read()))
    tests = os.path.join(TCC7_DIR, "tests")
    for fn in sorted(os.listdir(tests)):
        if fn != "hello7.c":
            ents.append(("/src/t7/" + fn, open(os.path.join(tests, fn), "rb").read()))
    print("[tcc7] KFS %d files" % len(ents), flush=True)
    return ents


TILCK_DIR = os.path.join(ROOT, "src", "stages", "12-tilck")
TILCK_TAR = os.path.join(SOURCES, "tilck-01dacaa5.tar")
TILCK_SHA256 = "d7660faab4f7bd80977666ac592031b8db2ce5a839f5a552205d30942c3333b8"
DTC_TAR = os.path.join(SOURCES, "dtc-v1.7.2.tar")
DTC_SHA256 = "3e28d3c2d3e069331d8e4799892933ab667ffec71164fdefd0b390d25a69e0b8"
UAPI_TAR = os.path.join(SOURCES, "linux-uapi-v6.6.tar")
UAPI_SHA256 = "1df21bbfa9d4715da2439157a558ae30d2320eafd3e7ad9184b3e427c65140bf"
TILCK_OUT = artifacts("12-tilck")
TILCK_MODULES = ("console", "serial", "riscv_irq", "goldfish", "null", "sysfs", "tracing")
KFS_MAX = 0x3E00000


def collect_kfs_tilck():
    """Plant Linux uapi (/src/uapi), Tilck + override + libfdt (/src/tilck)
    and the stage tools. FILES, CONFIG_FILES and MODULES are tar listings:
    Kronos has no getdents."""
    ents = []
    uapi = _tar_files(UAPI_TAR, UAPI_SHA256)
    hdrs = sorted(n for n in uapi if n.endswith(".h"))
    for name in hdrs:
        ents.append(("/src/uapi/" + name, uapi[name]))
    ents.append(("/src/uapi/FILES", ("\n".join(hdrs) + "\n").encode()))
    tilck = _tar_files(TILCK_TAR, TILCK_SHA256)
    keep = ("include/", "kernel/", "common/", "config/") + tuple(
        "modules/%s/" % m for m in TILCK_MODULES
    )
    files = dict((n, d) for n, d in tilck.items() if n.startswith(keep))
    over = os.path.join(TILCK_DIR, "src", "override")
    for root, _dirs, names in os.walk(over):
        for fn in names:
            full = os.path.join(root, fn)
            files[os.path.relpath(full, over).replace("\\", "/")] = open(full, "rb").read()
    dtc = _tar_files(DTC_TAR, DTC_SHA256)
    for name in dtc:
        if name.startswith("libfdt/") and name.endswith((".c", ".h")):
            files[name] = dtc[name]
    for name in sorted(files):
        ents.append(("/src/tilck/" + name, files[name]))
    cfgs = sorted(
        n for n in tilck
        if re.match(r"config/(kernel/)?[^/]+\.h$", n)
    )
    mods = sorted(set(n.split("/")[1] for n in tilck if re.match(r"modules/[^/]+/", n)))
    ents.append(("/src/tilck/CONFIG_FILES", ("\n".join(cfgs) + "\n").encode()))
    ents.append(("/src/tilck/MODULES", ("\n".join(mods) + "\n").encode()))
    src = os.path.join(TILCK_DIR, "src")
    for fn in ("tilck-config.txt", "cflags.rsp", "SOURCES", "link.rsp", "init.c"):
        ents.append(("/src/tilck/" + fn, open(os.path.join(src, fn), "rb").read()))
    for fn in ("headers-install.c", "gen-tilck-config.c"):
        ents.append(("/src/tilck-tools/" + fn, open(os.path.join(src, fn), "rb").read()))
    for fn in ("sbi.rsp", "sbi-entry.s", "sbi.c"):
        ents.append(("/src/tilck/sbi/" + fn, open(os.path.join(src, "sbi", fn), "rb").read()))
    tests = os.path.join(TILCK_DIR, "tests")
    for fn in sorted(os.listdir(tests)):
        ents.append(("/src/tilck-tests/" + fn, open(os.path.join(tests, fn), "rb").read()))
    print(
        "[tilck] KFS %d files (uapi %d, tilck %d, config %d, modules %d)"
        % (len(ents), len(hdrs), len(files), len(cfgs), len(mods)),
        flush=True,
    )
    return ents


MUSL_DIR = os.path.join(ROOT, "src", "stages", "13-musl")
MUSL_TGZ = os.path.join(SOURCES, "musl-1.1.24.tar.gz")
MUSL_SHA256 = "1370c9a812b2cf2a7d92802510cca0058cc37e66a7bedd70051f0a34015022a3"
MUSL_OUT = artifacts("13-musl")
MUSL_HEADERS = re.compile(r"(include/([^/]+/)?[^/]+|arch/(generic|riscv64)/bits/[^/]+)\.h$")


def musl_obj(line):
    """kmain.c build_objs(): the object of a SOURCES line."""
    return "/build/musl/obj/" + line.replace("/", "__") + ".o"


def collect_kfs_musl():
    """Plant musl 1.1.24 (gunzip + untar only) under /src/musl: src/,
    arch/{riscv64,generic}/, include/, crt/*.c, 13-musl/src/override/ on
    top. HEADERS is the install-headers listing (Kronos has no getdents);
    ar.rsp is SOURCES as objects after "-ar rcs libc.a"."""
    tar = _tar_files(MUSL_TGZ, MUSL_SHA256)
    pre = "musl-1.1.24/"
    keep = ("src/", "arch/riscv64/", "arch/generic/", "include/")
    files = {}
    for name, data in tar.items():
        if not name.startswith(pre):
            raise SystemExit("%s: member %s outside %s" % (MUSL_TGZ, name, pre))
        rel = name[len(pre):]
        if rel.startswith(keep) or re.match(r"crt/[^/]+\.c$", rel):
            files[rel] = data
    over = os.path.join(MUSL_DIR, "src", "override")
    for root, _dirs, names in os.walk(over):
        for fn in names:
            full = os.path.join(root, fn)
            rel = os.path.relpath(full, over).replace("\\", "/")
            if rel not in files:
                raise SystemExit("override %s replaces no archive file" % rel)
            files[rel] = open(full, "rb").read()
    ents = [("/src/musl/" + n, files[n]) for n in sorted(files)]
    hdrs = sorted(n for n in files if MUSL_HEADERS.match(n))
    ents.append(("/src/musl/HEADERS", ("\n".join(hdrs) + "\n").encode()))
    src = os.path.join(MUSL_DIR, "src")
    sources = open(os.path.join(src, "SOURCES"), "rb").read()
    lines = sources.decode().split()
    missing = [n for n in lines if n not in files]
    if missing:
        raise SystemExit("SOURCES lines not in the archive: %s" % missing[:5])
    ents.append(("/src/musl/SOURCES", sources))
    ents.append(("/src/musl/cflags.rsp", open(os.path.join(src, "cflags.rsp"), "rb").read()))
    ar = ["-ar rcs /build/musl/lib/libc.a"] + [musl_obj(n) for n in lines]
    ents.append(("/src/musl/ar.rsp", ("\n".join(ar) + "\n").encode()))
    for fn in ("musl-gen.c", "mkustar.c"):
        ents.append(("/src/musl-tools/" + fn, open(os.path.join(src, fn), "rb").read()))
    tests = os.path.join(MUSL_DIR, "tests")
    for fn in sorted(os.listdir(tests)):
        ents.append(("/src/musl-tests/" + fn, open(os.path.join(tests, fn), "rb").read()))
    print(
        "[musl] KFS %d files (archive %d, headers %d, SOURCES %d) %d bytes"
        % (len(ents), len(files), len(hdrs), len(lines), sum(len(d) for _n, d in ents)),
        flush=True,
    )
    return ents


SEED_DIR = os.path.join(ROOT, "src", "stages", "14-seed")
SEED_OUT = artifacts("14-seed")
# /src/seed/<name>/: git archives of stage0-posix 45d90f59's submodules and
# of live-bootstrap b1ceced7 (14-seed README)
SEED_TARS = {
    "mescc-tools": ("mescc-tools-5adfbf33.tar",
                    "d4b110d50198758ec47051e2c462c9b6823ed6c52653ffc84d9d2d790df226d1"),
    "mescc-tools-extra": ("mescc-tools-extra-a151c245.tar",
                          "ebecaeed63a12535fb2e17464f3764dc246671301e64ed8b1e67ac3cda5e56db"),
    "M2libc": ("M2libc-68a23cfd.tar",
               "b091be0f0534fb7bc8104e5f5b52d9e691a9e1f6fc3344d394c0362bddaaf6b1"),
    "live-bootstrap": ("live-bootstrap-b1ceced7.tar",
                       "203292a25e8f1ab86c982eddc5cc5990a8eddbd459252bb9191a5338e60cf30c"),
}
# archive members planted, as /src/seed/<name>/<member> (lb: the src/ file only)
SEED_KEEP = {
    "mescc-tools": ("Kaem/kaem.c", "Kaem/variable.c", "Kaem/kaem_globals.c",
                    "Kaem/kaem_globals.h", "Kaem/kaem.h"),
    "M2libc": ("bootstrappable.c", "bootstrappable.h"),
    "live-bootstrap": ("steps/checksum-transcriber-1.0/src/checksum-transcriber.c",
                       "steps/simple-patch-1.0/src/simple-patch.c"),
}
# what tcc.c includes with ONE_SOURCE for riscv64 (tcc -MD on the host)
TCCM_SRC = (
    "tcc.c", "tcc.h", "libtcc.c", "libtcc.h", "tccpp.c", "tccgen.c", "tccdbg.c",
    "tccasm.c", "tccelf.c", "tccrun.c", "tcctools.c", "riscv64-gen.c",
    "riscv64-link.c", "riscv64-asm.c", "riscv64-tok.h", "tcctok.h", "elf.h",
    "dwarf.h", "stab.h", "stab.def",
)
# a musl program on Kronos (stage 13 template); objects and libraries are
# linked in tcc's own default order: crt1 crti OBJS libc libtcc1 crtn
SEED_CC_HEAD = (
    "-nostdinc -I /build/musl/include -I /lib/tcc7/include -nostdlib -static "
    "-Wl,-Ttext=0x10000 /build/musl/lib/crt1.o /build/musl/lib/crti.o"
)
SEED_CC_TAIL = "/build/musl/lib/libc.a /build/seed/usr/lib/tcc/libtcc1.a /build/musl/lib/crtn.o"


def collect_kfs_seed():
    """Plant stage 14 (the seed for Tilck): tcc mob + stage 11 overrides
    with our config.h under /src/seed/tcc, the kaem/mescc-tools-extra/lb
    sources, M2libc's bootstrappable.[ch], rung-init and seed-tree; one
    tcc7b listfile per PROGS line (/src/seed/rsp/), their list SEED, the
    libtcc1 object list and ar.rsp, and the seed-tree lists TCCINC (tcc's
    headers) and BUILT (files built in place under /build/seed/usr)."""
    src = os.path.join(SEED_DIR, "src")
    ents = []
    tcc = _tar_files(TCC7_TAR, TCC7_SHA256)
    over = os.path.join(TCC7_DIR, "src", "override")
    for root, _dirs, names in os.walk(over):
        for fn in names:
            full = os.path.join(root, fn)
            tcc[os.path.relpath(full, over).replace("\\", "/")] = open(full, "rb").read()
    for name in TCCM_SRC:
        ents.append(("/src/seed/tcc/" + name, tcc[name]))
    ents.append(("/src/seed/tcc/config.h", open(os.path.join(src, "config.h"), "rb").read()))
    tccinc = sorted(n for n in tcc if n.startswith("include/"))
    progs = [ln.split() for ln in open(os.path.join(src, "PROGS")).read().splitlines() if ln]
    want = set(t for p in progs for t in p[1:] if t.startswith("/src/seed/") and t.endswith(".c"))
    files = {}
    for name, (tar, sha) in sorted(SEED_TARS.items()):
        arch = _tar_files(os.path.join(SOURCES, tar), sha)
        for member, data in arch.items():
            path = "/src/seed/%s/%s" % (name, member)
            if name == "live-bootstrap" and member in SEED_KEEP[name]:
                files["/src/seed/%s/%s" % (name, member.rsplit("/", 1)[-1])] = data
            elif member in SEED_KEEP.get(name, ()) or path in want:
                files[path] = data
    over = os.path.join(src, "override")
    for root, _dirs, names in os.walk(over):
        for fn in names:
            full = os.path.join(root, fn)
            path = "/src/seed/" + os.path.relpath(full, over).replace("\\", "/")
            if path not in files:
                raise SystemExit("override %s replaces no planted file" % path)
            files[path] = open(full, "rb").read()
    files["/src/seed/rung-init.c"] = open(os.path.join(src, "rung-init.c"), "rb").read()
    ents.extend(sorted(files.items()))
    missing = sorted(want - set(n for n, _d in ents))
    if missing:
        raise SystemExit("PROGS sources not planted: %s" % missing)
    ents.append(("/src/seed-tools/seed-tree.c", open(os.path.join(src, "seed-tree.c"), "rb").read()))
    rsps = []
    built = ["lib/tcc/libtcc1.a"]
    for p in progs:
        rsp = "/src/seed/rsp/%s.rsp" % p[0].rsplit("/", 1)[-1]
        line = "-o /build/seed/%s %s %s %s\n" % (p[0], SEED_CC_HEAD, " ".join(p[1:]), SEED_CC_TAIL)
        ents.append((rsp, line.encode()))
        rsps.append(rsp)
        if p[0].startswith("usr/"):
            built.append(p[0][len("usr/"):])
    ents.append(("/src/seed/SEED", ("\n".join(rsps) + "\n").encode()))
    ents.append(("/src/seed/BUILT", ("\n".join(built) + "\n").encode()))
    ents.append(("/src/seed/TCCINC", ("\n".join(tccinc) + "\n").encode()))
    lib = open(os.path.join(src, "LIBTCC1"), "rb").read()
    ents.append(("/src/seed/LIBTCC1", lib))
    ents.append(("/src/seed/libtcc1.rsp", open(os.path.join(src, "libtcc1.rsp"), "rb").read()))
    ar = ["-ar rcs /build/seed/usr/lib/tcc/libtcc1.a"] + [
        "/build/seed/obj/%s.o" % n for n in lib.decode().split()
    ]
    ents.append(("/src/seed/ar.rsp", ("\n".join(ar) + "\n").encode()))
    print(
        "[seed] KFS %d files (tcc %d, archives %d, progs %d) %d bytes"
        % (len(ents), len(TCCM_SRC) + 1, len(files) - 1, len(progs),
           sum(len(d) for _n, d in ents)),
        flush=True,
    )
    return ents


def _elf_section_names(blob):
    """Section names of an ELF64 little-endian file."""
    shoff = struct.unpack_from("<Q", blob, 0x28)[0]
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", blob, 0x3A)
    strs = struct.unpack_from("<Q", blob, shoff + shstrndx * shentsize + 0x18)[0]
    names = []
    for i in range(shnum):
        off = struct.unpack_from("<I", blob, shoff + i * shentsize)[0]
        end = blob.index(b"\0", strs + off)
        names.append(blob[strs + off : end].decode())
    return names


def check_no_got(blob, name):
    """-fno-pic check: a static link makes .got only for GOT relocations."""
    names = _elf_section_names(blob)
    if ".got" in names:
        raise SystemExit("%s has a .got section (GOT relocations): %s" % (name, names))
    print("[nopic] %s: no .got (%d sections)" % (name, len(names)), flush=True)


def check_sysroot(blob):
    """musl-sysroot.tar: a ustar with the headers, libc.a and crt*.o."""
    with tarfile.open(fileobj=io.BytesIO(blob)) as tf:
        names = set(m.name for m in tf.getmembers() if m.isfile())
        libc = tf.extractfile("lib/libc.a").read()
    want = {"include/stdio.h", "include/bits/alltypes.h", "include/bits/syscall.h",
            "lib/libc.a", "lib/crt1.o", "lib/crti.o", "lib/crtn.o"}
    if not want <= names or libc[:8] != b"!<arch>\n":
        raise SystemExit("musl-sysroot.tar: missing %s" % sorted(want - names))
    print(
        "[musl] musl-sysroot.tar %d bytes, %d files, libc.a %d bytes"
        % (len(blob), len(names), len(libc)),
        flush=True,
    )


def check_user_elf(blob, name):
    """A Tilck user program (stage 12 README): static riscv64 ET_EXEC,
    every PT_LOAD page aligned in file and memory, below 0x2000000000."""
    if blob[:4] != b"\x7fELF" or blob[4] != 2 or blob[5] != 1:
        raise SystemExit("%s: not ELF64 little-endian" % name)
    e_type, e_machine = struct.unpack_from("<HH", blob, 16)
    phoff = struct.unpack_from("<Q", blob, 0x20)[0]
    phentsize, phnum = struct.unpack_from("<HH", blob, 0x36)
    if e_type != 2 or e_machine != 243:
        raise SystemExit("%s: e_type %d e_machine %d, want 2 (EXEC) 243 (RISC-V)" % (name, e_type, e_machine))
    loads = 0
    for i in range(phnum):
        p_type, _f, p_offset, p_vaddr, _pa, _fs, p_memsz, p_align = struct.unpack_from(
            "<IIQQQQQQ", blob, phoff + i * phentsize
        )
        if p_type in (2, 3):
            raise SystemExit("%s: PT_DYNAMIC/PT_INTERP, not static" % name)
        if p_type == 1:
            loads = loads + 1
            if p_align != 0x1000 or (p_vaddr - p_offset) % 0x1000:
                raise SystemExit("%s: PT_LOAD align %#x off %#x va %#x" % (name, p_align, p_offset, p_vaddr))
            if p_vaddr + p_memsz > 0x2000000000:
                raise SystemExit("%s: PT_LOAD va %#x above BASE_VA" % (name, p_vaddr))
    return "%s %d bytes, %d PT_LOAD" % (name, len(blob), loads)


def check_seed(blob):
    """seed-rootfs.tar: every PROGS program under usr/ (static riscv64
    ELF for Tilck), musl's sysroot, tcc's libtcc1.a and headers."""
    progs = [ln.split()[0] for ln in open(os.path.join(SEED_DIR, "src", "PROGS")).read().splitlines() if ln]
    with tarfile.open(fileobj=io.BytesIO(blob)) as tf:
        files = dict((m.name, tf.extractfile(m).read()) for m in tf.getmembers() if m.isfile())
    want = set(p for p in progs if p.startswith("usr/")) | {
        "usr/lib/libc.a", "usr/lib/crt1.o", "usr/lib/crti.o", "usr/lib/crtn.o",
        "usr/lib/tcc/libtcc1.a", "usr/lib/tcc/include/tccdefs.h",
        "usr/include/stdio.h", "usr/include/bits/alltypes.h",
    }
    if not want <= set(files):
        raise SystemExit("seed-rootfs.tar: missing %s" % sorted(want - set(files)))
    if files["usr/lib/tcc/libtcc1.a"][:8] != b"!<arch>\n":
        raise SystemExit("seed-rootfs.tar: usr/lib/tcc/libtcc1.a is not an archive")
    bins = sorted(n for n in files if n.startswith("usr/bin/"))
    for n in bins:
        print("[seed] %s" % check_user_elf(files[n], n), flush=True)
    print(
        "[seed] seed-rootfs.tar %d bytes, %d files (%d in usr/bin, %d in usr/include)"
        % (len(blob), len(files), len(bins), sum(1 for n in files if n.startswith("usr/include/"))),
        flush=True,
    )


def run_tcc_self_k():
    if not os.path.isfile(IMAGE):
        raise SystemExit("missing %s" % IMAGE)
    if not os.path.isfile(TCC_PNUT):
        raise SystemExit("missing %s; run src/scripts/simon.py first" % TCC_PNUT)
    if not os.path.isfile(PNUT_BIN):
        raise SystemExit("missing %s" % PNUT_BIN)
    tcc6_elf = os.path.join(TCC_OUT, "tcc6.elf")
    if not os.path.isfile(tcc6_elf):
        raise SystemExit("missing %s" % tcc6_elf)
    os.makedirs(KOUT, exist_ok=True)

    tcc_bin = open(TCC_PNUT, "rb").read()
    pnut_bin = open(PNUT_BIN, "rb").read()
    tcc6_bin = open(tcc6_elf, "rb").read()
    if tcc6_bin[:4] != b"\x7fELF":
        raise SystemExit("tcc6.elf is not ELF")
    print(
        "[tcc-self-k] cached tcc-pnut.bin %d bytes pnut.bin %d bytes tcc6.elf %d"
        % (len(tcc_bin), len(pnut_bin), len(tcc6_bin)),
        flush=True,
    )
    if len(tcc_bin) < 50000:
        raise SystemExit("tcc-pnut.bin too small (%d)" % len(tcc_bin))

    q = Qemu(image=IMAGE, memory="6G", smp="1", qmp_sock=BUILD_QMP)
    try:
        print("[tcc-self-k] qemu pid=%s image=%s" % (q.proc.pid, IMAGE), flush=True)
        q.wait_prompt(30, "boot")
        q.drain_quiet()
        qmp = Qmp(q.qmp_sock)
        qmp.call("stop")
        try:
            print("[tcc-self-k] qemu stopped, vmmap", flush=True)
            ram_host = find_ram_host(q.proc.pid)
            print("[tcc-self-k] ram_host=0x%x" % ram_host, flush=True)
            blank_ramfs_names(
                q.proc.pid,
                ram_host,
                ("tcc-pnut.bin", "pnut.bin")
                + KERNEL_FILES
                + USER_C_FILES
                + ("kernel.bin", "init.elf", "cmp.elf"),
            )
            plant_files(
                q.proc.pid,
                ram_host,
                [("pnut.bin", pnut_bin), ("tcc-pnut.bin", tcc_bin)],
                dest_base=0x84000000,
            )
        finally:
            qmp.call("cont")
            qmp.close()
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
        qmp = Qmp(q.qmp_sock)
        qmp.call("stop")
        try:
            plant_named(q, ram_host, 0x85000000, kitems)
        finally:
            qmp.call("cont")
            qmp.close()

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
            ("/bin/tcc", tcc6_bin),
            ("/bin/cmp", cmp_elf),
        ]
        print(
            "[tcc-self-k] /bin/tcc from tcc6.elf %d (0x10000, ramfs is 0x80200000)"
            % len(tcc6_bin),
            flush=True,
        )
        # tccgen.c from the working tree: its riscv64 zero-extension fix must
        # reach tcc2-4 too, or a fixed seed tcc6 makes tcc3 and tcc4 differ.
        ents.extend(collect_kfs_self(wt_names=("tccelf.c", "tccgen.c")))
        ents.extend(collect_kfs_tcc_full())
        hello = open(HELLO_C, "rb").read()
        ents.append(("/src/hello/hello.c", hello))
        print("[hello6] KFS /src/hello/hello.c %d" % len(hello), flush=True)
        ents.append(("/src/hello/mob-zext.c", open(ZEXT_C, "rb").read()))
        ents.extend(collect_kfs_tcc7())
        ents.extend(collect_kfs_tilck())
        ents.extend(collect_kfs_musl())
        ents.extend(collect_kfs_seed())
        ramfs = pack(ents)
        open(os.path.join(KOUT, "ramfs.bin"), "wb").write(ramfs)
        print("[tcc-self-k] ramfs.bin %d bytes (max %d)" % (len(ramfs), KFS_MAX), flush=True)
        if len(ramfs) > KFS_MAX:
            raise SystemExit("ramfs.bin %d > %#x (KFS area in entry.s)" % (len(ramfs), KFS_MAX))
        firmware = kernel.ljust(0x200000, b"\0") + ramfs
        open(FIRMWARE, "wb").write(firmware)
        print(
            "[tcc-self-k] firmware %d bytes ramfs %d"
            % (len(firmware), len(ramfs)),
            flush=True,
        )
        handoff_kronos(
            q,
            firmware,
            (
                BOOT_MARK,
                "tcc version 0.9.27",
                "cmp ok",
                "tcc5 start",
                "tcc5 built",
                "tcc version 0.9.27",
                "tcc6 start",
                "tcc6 built",
                "hello6 start",
                "hello6 built",
                "hello from tcc6",
                "zext ok",
                "floatlit6 ok",
                "tcc66 ok",
                "tcc7 built",
                "tcc7b built",
                "tcc7c built",
                "tcc77 ok",
                "hello from tcc7",
                "floatlit ok",
                "kconst ok",
                "priv ok",
                "macro ok",
                "penum ok",
                "predefs ok",
                "gasdir ok",
                "bssym ok",
                "atomicn ok",
                "nopic ok",
                "branch ok",
                "cstore ok",
                "luiext ok",
                "epilog ok",
                "arlink ok",
                "t7 done",
                "headers-install built",
                "uapi ok",
                "tilck config ok",
                "tilck objs ok",
                "tilck linked",
                "sbi built",
                "tilck progs built",
                "tilck done",
                "musl-gen ok",
                "musl objs ok",
                "musl built",
                "musl progs built",
                "mkustar ok",
                "musl done",
                "libtcc1 objs ok",
                "libtcc1 built",
                "seed progs ok",
                "seed-tree ok",
                "mkustar ok",
                "seed built",
                "seed done",
            ),
            3600,
            "tcc-self-k",
        )
        snaps = "".join(q.snaps.values())
        dump_snap(q, snaps, "tcc6", os.path.join(TCC_OUT, "tcc6.elf"))
        dump_snap(q, snaps, "hello6", os.path.join(TCC_OUT, "hello6.elf"))
        dump_snap(q, snaps, "tcc7b", os.path.join(TCC7_OUT, "tcc7b.elf"))
        nopic = dump_snap(q, snaps, "nopic", os.path.join(TCC7_OUT, "t7-nopic.elf"))
        tilck = dump_snap(q, snaps, "tilck", os.path.join(TILCK_OUT, "tilck.elf"))
        sbi = dump_snap(q, snaps, "sbi", os.path.join(TILCK_OUT, "sbi.bin"), elf=False)
        for tag, name in (("tinit", "init.elf"), ("tproc", "proc.elf"), ("tchild", "child.elf")):
            dump_snap(q, snaps, tag, os.path.join(TILCK_OUT, name))
        dump_snap(q, snaps, "mhello", os.path.join(MUSL_OUT, "hello-musl.elf"))
        dump_snap(q, snaps, "mproc", os.path.join(MUSL_OUT, "musl-proc.elf"))
        dump_snap(q, snaps, "tmem", os.path.join(TILCK_OUT, "mem.elf"))
        dump_snap(q, snaps, "thard", os.path.join(TILCK_OUT, "hard.elf"))
        tar = dump_snap(q, snaps, "msysroot", os.path.join(MUSL_OUT, "musl-sysroot.tar"), elf=False)
        check_sysroot(tar)
        rinit = dump_snap(q, snaps, "rinit", os.path.join(SEED_OUT, "rung-init.elf"))
        seed = dump_snap(q, snaps, "seed", os.path.join(SEED_OUT, "seed-rootfs.tar"), elf=False)
        print("[seed] %s" % check_user_elf(rinit, "rung-init.elf"), flush=True)
        check_seed(seed)
        check_no_got(nopic, "t7-nopic.elf")
        check_no_got(tilck, "tilck.elf")
        if not 0 < len(sbi) < 0x100000:
            raise SystemExit("sbi.bin size %d, want 1..1M (below Tilck's DTB copy)" % len(sbi))
    finally:
        q.close()


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
            "usage: kronos.py --boot|--hi|--readlen|--tcc-self-k"
        )
