#!/usr/bin/env python3
"""Boot the guest-built Tilck in a fresh QEMU.

-bios is the guest-built SBI shim (12-tilck/artifacts/sbi.bin). The host only
places bytes: tilck.elf, the optional initrd.img and a bootinfo block with
their addresses (layout in 12-tilck/src/sbi/README.md) go in with
`-device loader`. The shim loads the ELF, fills /chosen and enters Tilck.

--test packs the guest-built programs (artifacts/{init,proc,child,mem,hard}.elf
and 13-musl/artifacts/{hello-musl,musl-proc}.elf) into artifacts/initrd.img
(FAT32, lib/fat32.py) as bin/init, bin/proc, bin/child, bin/mem, bin/hard,
bin/hello-musl, bin/musl-proc; Tilck runs /initrd/bin/init.

/dev/outbox: bootinfo gives the SBI shim an outbox region (OUTBOX_ADDR,
OUTBOX_SIZE), which it reserves in the DTB. Each close() of /dev/outbox makes
Tilck print `outbox ADDR SIZE`; for each such line this script copies the
region's first SIZE bytes to artifacts/outbox.bin (QMP pmemsave in 64 MB
chunks, from a worker thread while the UART keeps draining).

--rung NAME runs one rung of the ladder on Tilck (RUNGS below; 14-seed
README "Rungs"): the initrd holds bin/init (14-seed's rung-init), rootfs.tar
(the previous rung's /usr), steps/ (the rung's directory plus extra files)
and distfiles/ (pinned archives from build/sources, sha256 checked); the
guest's `rootfs sha256 HEX` must equal the sha256 of the outbox dump, which
is then copied to OUT/rootfs-NAME.tar.

Usage: tilck.py --boot|--test|--rung NAME
"""
import hashlib
import os
import queue
import re
import select
import shutil
import struct
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from lib.fat32 import pack_fat32
from lib.phases import artifacts
from lib.qemu import QEMU, ROOT, Qmp

TILCK_OUT = artifacts("12-tilck")
MUSL_OUT = artifacts("13-musl")
SEED_DIR = os.path.join(ROOT, "src", "stages", "14-seed")
SEED_OUT = artifacts("14-seed")
TOOLS_DIR = os.path.join(ROOT, "src", "stages", "15-early-tools")
TOOLS_OUT = artifacts("15-early-tools")
BUILD_DIR = os.path.join(ROOT, "src", "stages", "16-buildtools")
BUILD_OUT = artifacts("16-buildtools")
BINUTILS_DIR = os.path.join(ROOT, "src", "stages", "17-binutils")
BINUTILS_OUT = artifacts("17-binutils")
GMP_DIR = os.path.join(ROOT, "src", "stages", "18-gmp-mpfr-mpc")
GMP_OUT = artifacts("18-gmp-mpfr-mpc")
GCC464_DIR = os.path.join(ROOT, "src", "stages", "19-gcc464")
GCC464_OUT = artifacts("19-gcc464")
SOURCES = os.path.join(ROOT, "build", "sources")
BOOTINFO_ADDR = 0x87F00000
ELF_ADDR = 0x88000000
INITRD_ADDR = 0x90000000
# Tilck keeps the initrd range in u32 (early_fdt.c fdt_parse_chosen(),
# multiboot mod_start/mod_end): the image must end below 4 GB
INITRD_END_MAX = 1 << 32
RAM_END = 0x80000000 + (8 << 30)  # QEMU virt -m 8G
OUTBOX_SIZE = 1 << 30  # 2 MB aligned; the kernel loses this much RAM
OUTBOX_ADDR = RAM_END - OUTBOX_SIZE
OUTBOX_RE = re.compile(r"outbox (0x[0-9a-f]+) ([0-9]+)\r?\n")
DUMP_CHUNK = 64 << 20
QMP_SOCK = os.path.join(TILCK_OUT, "qmp.sock")
BOOTINFO_MAGIC = 0x32544F4F424B4C54  # "TLKBOOT2"
BOOTARGS_MAX = 200
FAIL_MARKS = ("sbi fail", "PANIC", "panic:", "init fail", "proc fail", "mem fail", "hard fail")
INITRD_PROGS = (
    ("bin/init", "init.elf"),
    ("bin/proc", "proc.elf"),
    ("bin/child", "child.elf"),
    ("bin/mem", "mem.elf"),
    ("bin/hard", "hard.elf"),
    ("bin/hello-musl", os.path.join(MUSL_OUT, "hello-musl.elf")),
    ("bin/musl-proc", os.path.join(MUSL_OUT, "musl-proc.elf")),
)
TEST_MARKS = (
    "Hello from Tilck",
    "init ok",
    "fork ok",
    "pipe ok",
    "exec ok",
    "dirs ok",
    "writev ok",
    "hello from musl",
    "musl proc ok",
    "mem 3072M ok",
    "fdmax ok",
    "chown ok",
    "rootperm ok",
    "xbit ok",
    "fatseek ok",
    "deepstack ok",
    "brkzero ok",
    "prio ok",
    "scriptlock ok",
    "sharedoff ok",
    "abort ok",
    "killcwd ok",
    "pipewrite ok",
    "ttywrite ok",
    "rlimit ok",
    "munmapspan ok",
    "munmappart ok",
    "mremap ok",
    "scriptargs ok",
    "outbox sha256 ",
    "tests done",
)


def bootinfo(elf_size, initrd_size, bootargs=""):
    args = bootargs.encode()
    if len(args) >= BOOTARGS_MAX:
        raise SystemExit("bootargs too long (%d)" % len(args))
    return struct.pack(
        "<7Q%ds" % BOOTARGS_MAX,
        BOOTINFO_MAGIC,
        ELF_ADDR,
        elf_size,
        INITRD_ADDR if initrd_size else 0,
        initrd_size,
        OUTBOX_ADDR,
        OUTBOX_SIZE,
        args,
    )


def pack_initrd(progs):
    """artifacts/initrd.img from (initrd path, artifact name or path, or
    bytes) pairs; files under bin/ must be ELF."""
    files = {}
    for path, name in progs:
        if isinstance(name, bytes):
            src, blob = path, name
        else:
            src = os.path.join(TILCK_OUT, name)  # an absolute name stays as it is
            if not os.path.isfile(src):
                raise SystemExit("missing %s; run kronos.py --tcc-self-k" % src)
            blob = open(src, "rb").read()
        if path.startswith("bin/") and blob[:4] != b"\x7fELF":
            raise SystemExit("%s is not ELF" % src)
        files[path] = blob
    img = pack_fat32(files)
    end_max = min(INITRD_END_MAX, OUTBOX_ADDR)
    if INITRD_ADDR + len(img) >= end_max:
        raise SystemExit("initrd.img %d bytes does not fit below %#x" % (len(img), end_max))
    open(os.path.join(TILCK_OUT, "initrd.img"), "wb").write(img)
    print(
        "[tilck] initrd.img %d bytes: %s"
        % (len(img), " ".join("%s=%d" % (p, len(files[p])) for p in sorted(files))),
        flush=True,
    )


def start_qemu():
    sbi = os.path.join(TILCK_OUT, "sbi.bin")
    elf = os.path.join(TILCK_OUT, "tilck.elf")
    initrd = os.path.join(TILCK_OUT, "initrd.img")
    for path in (sbi, elf):
        if not os.path.isfile(path):
            raise SystemExit("missing %s; run kronos.py --tcc-self-k" % path)
    elf_size = os.path.getsize(elf)
    if ELF_ADDR + elf_size > INITRD_ADDR:
        raise SystemExit("tilck.elf %d bytes overlaps the initrd" % elf_size)
    initrd_size = os.path.getsize(initrd) if os.path.isfile(initrd) else 0
    bi = os.path.join(TILCK_OUT, "bootinfo.bin")
    open(bi, "wb").write(bootinfo(elf_size, initrd_size))
    loaders = [(elf, ELF_ADDR), (bi, BOOTINFO_ADDR)]
    if initrd_size:
        loaders.append((initrd, INITRD_ADDR))
    if os.path.exists(QMP_SOCK):
        os.unlink(QMP_SOCK)
    cmd = [QEMU, "-M", "virt", "-m", "8G", "-smp", "1", "-nographic", "-bios", sbi]
    cmd += ["-qmp", "unix:%s,server,nowait" % QMP_SOCK]
    for path, addr in loaders:
        cmd += ["-device", "loader,file=%s,addr=%#x,force-raw=on" % (path, addr)]
    print(
        "[tilck] sbi.bin %d tilck.elf %d initrd %d"
        % (os.path.getsize(sbi), elf_size, initrd_size),
        flush=True,
    )
    print("[tilck] %s" % " ".join(cmd), flush=True)
    return subprocess.Popen(
        cmd, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT
    )


class OutboxDumper:
    """artifacts/outbox.bin for each `outbox ADDR SIZE` line. A worker thread
    does the QMP pmemsave, so the UART pipe keeps draining meanwhile."""

    def __init__(self):
        self.jobs = queue.Queue()
        self.done = []  # (addr, size, sha256 hex) per dump
        self.errors = []
        self.thread = threading.Thread(target=self._work, daemon=True)
        self.thread.start()

    def request(self, addr, size):
        self.jobs.put((addr, size))

    def finish(self, timeout):
        self.jobs.put(None)
        self.thread.join(timeout)
        if self.thread.is_alive():
            self.errors.append("dump timeout")

    def _work(self):
        qmp = None
        while True:
            job = self.jobs.get()
            if job is None:
                break
            try:
                if qmp is None:
                    qmp = Qmp(QMP_SOCK)
                self.done.append(dump_outbox(qmp, *job))
            except (OSError, RuntimeError, ValueError) as err:
                self.errors.append("dump %#x %d: %s" % (job[0], job[1], err))
        if qmp is not None:
            qmp.close()


def dump_outbox(qmp, addr, size):
    """Copy guest RAM [addr, addr + size) to artifacts/outbox.bin."""
    if addr != OUTBOX_ADDR or size > OUTBOX_SIZE:
        raise ValueError("not the outbox region (%#x, %d bytes)" % (OUTBOX_ADDR, OUTBOX_SIZE))
    dest = os.path.join(TILCK_OUT, "outbox.bin")
    chunk = dest + ".chunk"
    sha = hashlib.sha256()
    t0 = time.time()
    with open(dest + ".part", "wb") as out:
        for off in range(0, size, DUMP_CHUNK):
            n = min(DUMP_CHUNK, size - off)
            qmp.call("pmemsave", {"val": addr + off, "size": n, "filename": chunk})
            data = open(chunk, "rb").read()
            if len(data) != n:
                raise ValueError("pmemsave gave %d of %d bytes" % (len(data), n))
            out.write(data)
            sha.update(data)
    os.replace(dest + ".part", dest)
    if os.path.exists(chunk):
        os.unlink(chunk)
    print(
        "[outbox] %s %d bytes from %#x in %.1fs sha256 %s"
        % (dest, size, addr, time.time() - t0, sha.hexdigest()),
        flush=True,
    )
    return addr, size, sha.hexdigest()


def wait_uart(proc, markers, timeout, label, dumper, fail_marks=FAIL_MARKS):
    """Read UART until every marker appeared in order; (ok, UART text).
    Each new `outbox ADDR SIZE` line goes to the dumper."""
    buf = ""
    got = 0
    pos = 0
    scan = 0
    t0 = time.time()
    deadline = t0 + timeout
    ok = False
    why = "timeout"
    # the UART also goes to the log as it arrives (a rung runs for hours;
    # the text must outlive a stopped or crashed host script)
    log = os.path.join(TILCK_OUT, "uart-tilck.log")
    live = open(log, "wb")
    while time.time() < deadline:
        r, _, _ = select.select([proc.stdout], [], [], 0.2)
        if r:
            chunk = os.read(proc.stdout.fileno(), 65536)
            if not chunk:
                why = "qemu exited %s" % proc.poll()
                break
            live.write(chunk)
            live.flush()
            buf += chunk.decode("utf-8", "replace")
            for m in OUTBOX_RE.finditer(buf, scan):
                dumper.request(int(m.group(1), 16), int(m.group(2)))
                scan = m.end()
        while got < len(markers):
            i = buf.find(markers[got], pos)
            if i < 0:
                break
            pos = i + len(markers[got])
            print("[%s] got %r at %.1fs" % (label, markers[got], time.time() - t0), flush=True)
            got += 1
        if got == len(markers):
            ok = True
            break
        # complete lines only: a read can end inside "rung exit 0"
        lines = buf[: buf.rfind("\n") + 1]
        bad = [m for m in fail_marks if m in lines]
        if bad:
            why = "fail mark %r" % bad[0]
            break
    # a little more output after the last marker (or the failure) for the log
    end = time.time() + 1.0
    while time.time() < end and proc.poll() is None:
        r, _, _ = select.select([proc.stdout], [], [], 0.2)
        if r:
            chunk = os.read(proc.stdout.fileno(), 65536)
            if not chunk:
                break
            buf += chunk.decode("utf-8", "replace")
    for m in OUTBOX_RE.finditer(buf, scan):
        dumper.request(int(m.group(1), 16), int(m.group(2)))
    live.close()
    open(log, "w").write(buf)
    print("--- UART (%s) ---" % log, flush=True)
    print(buf[-6000:], flush=True)
    print("--- end UART ---", flush=True)
    if not ok:
        print("[%s] FAIL %s; missing %r" % (label, why, markers[got:]), flush=True)
    return ok, buf


def check_outbox_sha(buf, dumper, key, label):
    """The guest's `KEY HEX` (KEY e.g. "outbox sha256") against the last dump."""
    m = re.search(re.escape(key) + r" ([0-9a-f]{64})", buf)
    if not m:
        return "no guest %s" % key
    if not dumper.done:
        return "no outbox dump"
    if dumper.done[-1][2] != m.group(1):
        return "%s mismatch: guest %s host %s" % (key, m.group(1), dumper.done[-1][2])
    print("[%s] %s match %s (%d bytes)" % (label, key, m.group(1), dumper.done[-1][1]), flush=True)
    return None


def run(markers, timeout, label, outbox_check=None, fail_marks=FAIL_MARKS):
    """outbox_check: the key of the guest's sha256 line to check, or None."""
    proc = start_qemu()
    dumper = OutboxDumper()
    try:
        ok, buf = wait_uart(proc, markers, timeout, label, dumper, fail_marks)
        dumper.finish(600)  # QEMU must still run for pmemsave
        for err in dumper.errors:
            print("[%s] FAIL outbox %s" % (label, err), flush=True)
            ok = False
        if ok and outbox_check:
            err = check_outbox_sha(buf, dumper, outbox_check, label)
            if err:
                print("[%s] FAIL %s" % (label, err), flush=True)
                ok = False
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
    if not ok:
        raise SystemExit(1)
    for m in markers:
        print("[%s] UART %r" % (label, m), flush=True)


def tree_files(prefix, top):
    """(prefix + path relative to top, bytes) for every file under top."""
    ents = []
    for root, _dirs, names in os.walk(top):
        for fn in names:
            if fn == ".DS_Store":
                continue
            full = os.path.join(root, fn)
            rel = os.path.relpath(full, top).replace("\\", "/")
            ents.append((prefix + rel, open(full, "rb").read()))
    return sorted(ents)


def tcc_mob_files():
    """steps/tcc-mob/ of rung tcc: the bytes the seed's tcc-m was built from
    (kronos.py collect_kfs_seed()): TCCM_SRC of the pinned tcc mob archive
    with stage 11's overrides, 14-seed's config.h, and lib/ with the
    sources of 14-seed's LIBTCC1."""
    import kronos

    tcc = kronos._tar_files(kronos.TCC7_TAR, kronos.TCC7_SHA256)
    tcc.update(tree_files("", os.path.join(kronos.TCC7_DIR, "src", "override")))
    ents = [("steps/tcc-mob/" + n, tcc[n]) for n in kronos.TCCM_SRC]
    ents.append(("steps/tcc-mob/config.h", open(os.path.join(SEED_DIR, "src", "config.h"), "rb").read()))
    for n in open(os.path.join(SEED_DIR, "src", "LIBTCC1")).read().split():
        ents.append(("steps/tcc-mob/lib/" + n, tcc["lib/" + n]))
    return ents


def musl_test_files():
    """steps/tests/{hello-musl,musl-proc}.c of rung musl2: stage 13's tests."""
    tests = os.path.join(ROOT, "src", "stages", "13-musl", "tests")
    return [("steps/tests/" + n, open(os.path.join(tests, n), "rb").read())
            for n in ("hello-musl.c", "musl-proc.c")]


def gcc1_step_files():
    """steps/{sources,excludes,patches/*,tests/sanity.c} of rungs gcc3 and
    gcc4: rung gcc1's pinned source, excludes, patches and sanity test."""
    gcc1 = os.path.join(GCC464_DIR, "gcc1")
    return [(p, b) for p, b in tree_files("steps/", gcc1)
            if p in ("steps/sources", "steps/excludes", "steps/tests/sanity.c")
            or p.startswith("steps/patches/")]


def gcc4_step_files():
    """gcc1_step_files(), rung gcc3's patches/ and steps/src/mkustar.c of
    rung gcc4: stage 13's mkustar, which the rung builds with
    gcc4/src/mkustar-long.patch."""
    src = os.path.join(ROOT, "src", "stages", "13-musl", "src", "mkustar.c")
    gcc3 = [(p, b) for p, b in tree_files("steps/", os.path.join(GCC464_DIR, "gcc3"))
            if p.startswith("steps/patches/")]
    return (gcc1_step_files() + gcc3
            + [("steps/src/mkustar.c", open(src, "rb").read())])


# --rung NAME. rootfs: the tar unpacked into / (the seed, or the previous
# rung's rootfs-NAME.tar); steps: its files go to /initrd/steps/ (run.kaem
# is the script); extra: a function giving more (initrd path, bytes);
# distfiles: (file in build/sources, sha256) -> /initrd/distfiles/FILE;
# marks: in order, then "rung exit 0" (kaem's status, printed by rung-init);
# the guest prints `rootfs sha256 HEX` of the tar it wrote to /dev/outbox,
# which becomes OUT/rootfs-NAME.tar.
RUNGS = {
    "tcc": {
        "rootfs": os.path.join(SEED_OUT, "seed-rootfs.tar"),
        "steps": os.path.join(SEED_DIR, "steps", "tcc"),
        "extra": tcc_mob_files,
        "distfiles": (),
        "marks": ("tilck hello ok", "tccm fixed ok", "rootfs sha256 ", "rung done tcc"),
        "timeout": 300,  # the gate run took 16 s
        "out": SEED_OUT,
    },
    # 15-early-tools README: live-bootstrap's make, patch, gzip, tar, sed,
    # bzip2 (sha256 from its steps/<pkg>/sources)
    "tools1": {
        "rootfs": os.path.join(SEED_OUT, "rootfs-tcc.tar"),
        "steps": os.path.join(TOOLS_DIR, "steps"),
        "extra": None,
        "distfiles": (
            ("make-3.82.tar.bz2", "e2c1a73f179c40c71e2fe8abf8a8a0688b8499538512984da4a76958d0402966"),
            ("patch-2.5.9.tar.gz", "ecb5c6469d732bcf01d6ec1afe9e64f1668caba5bfdb103c28d7f537ba3cdb8a"),
            ("gzip-1.2.4.tar.gz", "1ca41818a23c9c59ef1d5e1d00c0d5eaa2285d931c0fb059637d7c0cc02ad967"),
            ("tar-1.12.tar.gz", "c6c37e888b136ccefab903c51149f4b7bd659d69d4aea21245f61053a57aa60a"),
            ("sed-4.0.9.tar.gz", "c365874794187f8444e5d22998cd5888ffa47f36def4b77517a808dec27c0600"),
            ("bzip2-1.0.8.tar.gz", "ab5a03176ee106d3f0fa90e381da478ddae405918153cca248e682cd0c4a2269"),
        ),
        "marks": (
            "GNU Make 3.82",
            "patch 2.5.9",
            "gzip 1.2.4",
            "tar (GNU tar) 1.12",
            "GNU sed version 4.0.9",
            "Version 1.0.8",
            "tar gzip ok",
            "make ok",
            "rootfs sha256 ",
            "rung done tools1",
        ),
        "timeout": 1800,
        "out": TOOLS_OUT,
    },
    # 15-early-tools README: live-bootstrap's coreutils, oyacc, bash on top
    # of rung tools1
    "tools2": {
        "rootfs": os.path.join(TOOLS_OUT, "rootfs-tools1.tar"),
        "steps": os.path.join(TOOLS_DIR, "steps-tools2"),
        "extra": None,
        "distfiles": (
            ("coreutils-5.0.tar.bz2", "c25b36b8af6e0ad2a875daf4d6196bd0df28a62be7dd252e5f99a4d5d7288d95"),
            ("oyacc-6.6.tar.gz", "eb0866e740b79bd3a23e0ca47885eb3148aab18d77a4bedba96e979d8b4ebfe1"),
            ("bash-2.05b.tar.gz", "ba03d412998cc54bd0b0f2d6c32100967d3137098affdc2d32e6e7c11b163fe4"),
        ),
        "marks": (
            "ls (coreutils) 5.0",
            "GNU bash, version 2.05b.0(",
            "bash ok",
            "coreutils ok",
            "rootfs sha256 ",
            "rung done tools2",
        ),
        "timeout": 1800,  # the gate run took 82 s
        "out": TOOLS_OUT,
    },
    # 16-buildtools README: grep, gawk, diffutils, m4, flex (live-bootstrap's
    # mk/), the missing coreutils 5.0 programs, findutils (its configure),
    # then a real configure of grep-2.4; steps/run.kaem runs steps/run.sh
    "build1": {
        "rootfs": os.path.join(TOOLS_OUT, "rootfs-tools2.tar"),
        "steps": os.path.join(BUILD_DIR, "steps"),
        "extra": None,
        "distfiles": (
            ("grep-2.4.tar.gz", "a32032bab36208509466654df12f507600dfe0313feebbcd218c32a70bf72a16"),
            ("gawk-3.0.4.tar.gz", "5cc35def1ff4375a8b9a98c2ff79e95e80987d24f0d42fdbb7b7039b3ddb3fb0"),
            ("diffutils-2.7.tar.gz", "d5f2489c4056a31528e3ada4adacc23d498532b0af1a980f2f76158162b139d6"),
            ("m4-1.4.7.tar.bz2", "a88f3ddaa7c89cf4c34284385be41ca85e9135369c333fdfa232f3bf48223213"),
            ("flex-2.6.4.tar.gz", "e87aae032bf07c26f85ac0ed3250998c37621d95f8bd748b31f15b33c45ee995"),
            ("coreutils-5.0.tar.bz2", "c25b36b8af6e0ad2a875daf4d6196bd0df28a62be7dd252e5f99a4d5d7288d95"),
            ("findutils-4.2.33.tar.gz", "813cd9405aceec5cfecbe96400d01e90ddad7b512d3034487176ce5258ab0f78"),
        ),
        "marks": (
            "grep (GNU grep) 2.4",
            "GNU Awk 3.0.4",
            "diff - GNU diffutils version 2.7",
            "GNU M4 1.4.7",
            "flex 2.6.4",
            "GNU find version 4.2.33",
            "uname (coreutils) 5.0",
            "build1 tools ok",
            "configure ok",
            "rootfs sha256 ",
            "rung done build1",
        ),
        "timeout": 7200,
        "out": BUILD_OUT,
    },
    # 17-binutils README: binutils 2.30's own top-level configure and make
    # with tcc, then as/ld/ar/nm/ranlib tests; steps/run.kaem runs
    # steps/run.sh
    "binutils": {
        "rootfs": os.path.join(BUILD_OUT, "rootfs-build1.tar"),
        "steps": os.path.join(BINUTILS_DIR, "steps"),
        "extra": None,
        "distfiles": (
            ("binutils-2.30.tar.bz2", "efeade848067e9a03f1918b1da0d37aaffa0b0127a06b5e9236229851d9d0c09"),
        ),
        "marks": (
            "binutils configure ok",
            "binutils libiberty ok",
            "binutils zlib ok",
            "binutils bfd ok",
            "binutils opcodes ok",
            "binutils binutils ok",
            "binutils gas ok",
            "binutils ld ok",
            "binutils all ok",
            "binutils install ok",
            "GNU assembler (GNU Binutils) 2.30",
            "GNU ld (GNU Binutils) 2.30",
            "GNU ar (GNU Binutils) 2.30",
            "GNU nm (GNU Binutils) 2.30",
            "GNU ranlib (GNU Binutils) 2.30",
            "GNU objdump (GNU Binutils) 2.30",
            "GNU strip (GNU Binutils) 2.30",
            "GNU readelf (GNU Binutils) 2.30",
            "binutils asm ok",
            "binutils hello ok",
            "binutils ar ok",
            "rootfs sha256 ",
            "rung done binutils",
        ),
        "timeout": 6 * 3600,
        "out": BINUTILS_OUT,
    },
    # 18-gmp-mpfr-mpc README: gmp 4.3.2, mpfr 2.4.2, mpc 0.8.1 (GCC 4.6.4's
    # contrib/download_prerequisites), each with its own configure, make,
    # make check, make install; config.sub/config.guess from binutils 2.30
    "gmp": {
        "rootfs": os.path.join(BINUTILS_OUT, "rootfs-binutils.tar"),
        "steps": os.path.join(GMP_DIR, "steps"),
        "extra": None,
        "distfiles": (
            ("binutils-2.30.tar.bz2", "efeade848067e9a03f1918b1da0d37aaffa0b0127a06b5e9236229851d9d0c09"),
            ("gmp-4.3.2.tar.bz2", "936162c0312886c21581002b79932829aa048cfaf9937c6265aeaa14f1cd1775"),
            ("mpfr-2.4.2.tar.bz2", "c7e75a08a8d49d2082e4caee1591a05d11b9d5627514e678f02d66a124bcf2ba"),
            ("mpc-0.8.1.tar.gz", "e664603757251fd8a352848276497a4c79b7f8b21fd8aedd5cc0598a38fee3e4"),
        ),
        "marks": (
            "gmp configure ok",
            "gmp make ok",
            "gmp check ok",
            "gmp install ok",
            "mpfr configure ok",
            "mpfr make ok",
            "mpfr check ok",
            "mpfr install ok",
            "mpc configure ok",
            "mpc make ok",
            "mpc check ok",
            "mpc install ok",
            "gmp mpfr mpc link ok",
            "rootfs sha256 ",
            "rung done gmp",
        ),
        "timeout": 10 * 3600,
        "out": GMP_OUT,
    },
    # 19-gcc464 README: tcc builds GCC 4.6.4 (Ekaitz's RISC-V fork), C only,
    # --disable-bootstrap, into /usr/gcc1; gcc1/run.kaem runs gcc1/run.sh
    "gcc1": {
        "rootfs": os.path.join(GMP_OUT, "rootfs-gmp.tar"),
        "steps": os.path.join(GCC464_DIR, "gcc1"),
        "extra": None,
        "distfiles": (
            ("gcc-4.6.4-c0d20242.tar", "fb8689273a59812638f7d38a6187ebd32fd0487c0a90100d12a86e6251bbbac0"),
        ),
        "marks": (
            "gcc1 source ok",
            "gcc1 configured",
            "libiberty built",
            "libcpp built",
            "libdecnumber built",
            "gcc1 support built",
            "gen* done",
            "cc1 built",
            "xgcc built",
            "libgcc built",
            "gcc1 installed",
            "gcc version 4.6.4",
            "hello from gcc-1",
            "gcc1 sanity ok",
            "rootfs sha256 ",
            "rung done gcc1",
        ),
        "timeout": 24 * 3600,
        "out": GCC464_OUT,
    },
    # 19-gcc464 README: gcc-1 builds musl 1.1.24 with musl's configure and
    # Makefile (musl-2) into /usr; stage 13's test programs come along
    "musl2": {
        "rootfs": os.path.join(GCC464_OUT, "rootfs-gcc1.tar"),
        "steps": os.path.join(GCC464_DIR, "musl2"),
        "extra": musl_test_files,
        "distfiles": (
            ("musl-1.1.24.tar.gz", "1370c9a812b2cf2a7d92802510cca0058cc37e66a7bedd70051f0a34015022a3"),
        ),
        "marks": (
            "musl2 source ok",
            "musl2 configured",
            "musl2 built",
            "musl2 installed",
            "hello from musl",
            "musl proc ok",
            "musl2 complex ok",
            "musl2 ok",
            "musl2 tar pipe ok",
            "rootfs sha256 ",
            "rung done musl2",
        ),
        "timeout": 8 * 3600,
        "out": GCC464_OUT,
    },
    # 19-gcc464 README: gcc-1 and musl-2 run GCC 4.6.4's `make bootstrap`
    # (C) into /usr; gcc1's source list, excludes, patches and sanity test,
    # its own patches/ (riscv-rtx-costs.patch)
    "gcc3": {
        "rootfs": os.path.join(GCC464_OUT, "rootfs-musl2.tar"),
        "steps": os.path.join(GCC464_DIR, "gcc3"),
        "extra": gcc1_step_files,
        "distfiles": (
            ("gcc-4.6.4-c0d20242.tar", "fb8689273a59812638f7d38a6187ebd32fd0487c0a90100d12a86e6251bbbac0"),
        ),
        "marks": (
            "gcc3 source ok",
            "gcc3 configured",
            "stage1 cc1 built",
            "stage1 libgcc built",
            "stage2 cc1 built",
            "stage2 libgcc built",
            "stage3 cc1 built",
            "stage3 libgcc built",
            "Comparison successful",
            "bootstrap compare ok",
            "gcc3 installed",
            "gcc version 4.6.4",
            "hello from gcc 4.6.4",
            "gcc3 sanity ok",
            "rootfs sha256 ",
            "rung done gcc3",
        ),
        "timeout": 48 * 3600,
        "out": GCC464_OUT,
    },
    # 19-gcc464 README: gcc-3 builds GCC 4.6.4 for C and C++ into /usr
    # (--disable-bootstrap), g++ tests, the fixed-point check gcc-3 = gcc-4;
    # gcc1's source list, excludes (less gcc/cp, libstdc++-v3), patches
    # (with gcc3's and gcc4's own), stage 13's mkustar.c
    "gcc4": {
        "rootfs": os.path.join(GCC464_OUT, "rootfs-gcc3.tar"),
        "steps": os.path.join(GCC464_DIR, "gcc4"),
        "extra": gcc4_step_files,
        "distfiles": (
            ("gcc-4.6.4-c0d20242.tar", "fb8689273a59812638f7d38a6187ebd32fd0487c0a90100d12a86e6251bbbac0"),
        ),
        "marks": (
            "gcc4 source ok",
            "gcc4 configured",
            "gcc4 support built",
            "cc1 built",
            "cc1plus built",
            "xgcc built",
            "libgcc built",
            "libstdc++ built",
            "gcc4 installed",
            "g++ -v: gcc version 4.6.4",
            "hello from g++ 4.6.4",
            "gcc4 sanity ok",
            "cc1 layout ok",
            "fixedpoint ok",
            "rootfs sha256 ",
            "rung done gcc4",
        ),
        "timeout": 48 * 3600,
        "out": GCC464_OUT,
    },
}
RUNG_FAIL_MARKS = FAIL_MARKS + ("rung exit ",)


def run_rung(name):
    if name not in RUNGS:
        raise SystemExit("unknown rung %r (have %s)" % (name, ", ".join(sorted(RUNGS))))
    r = RUNGS[name]
    ents = [("bin/init", os.path.join(SEED_OUT, "rung-init.elf")), ("rootfs.tar", r["rootfs"])]
    ents += tree_files("steps/", r["steps"])
    if r["extra"]:
        ents += r["extra"]()
    for fn, sha in r["distfiles"]:
        blob = open(os.path.join(SOURCES, fn), "rb").read()
        got = hashlib.sha256(blob).hexdigest()
        if got != sha:
            raise SystemExit("%s sha256 %s, want %s" % (fn, got, sha))
        ents.append(("distfiles/" + fn, blob))
    pack_initrd(ents)
    t0 = time.time()
    run(r["marks"] + ("rung exit 0",), r["timeout"], name, "rootfs sha256", RUNG_FAIL_MARKS)
    dest = os.path.join(r["out"], "rootfs-%s.tar" % name)
    os.makedirs(r["out"], exist_ok=True)  # artifacts/ is not in git
    shutil.copyfile(os.path.join(TILCK_OUT, "outbox.bin"), dest)
    print("[%s] %s %d bytes; rung took %.0fs" % (name, dest, os.path.getsize(dest), time.time() - t0),
          flush=True)


if __name__ == "__main__":
    if "--boot" in sys.argv:
        run(("Hello from Tilck",), 120, "boot")
    elif "--test" in sys.argv:
        pack_initrd(INITRD_PROGS)
        run(TEST_MARKS, 600, "test", outbox_check="outbox sha256")
    elif "--rung" in sys.argv and sys.argv.index("--rung") + 1 < len(sys.argv):
        run_rung(sys.argv[sys.argv.index("--rung") + 1])
    else:
        raise SystemExit("usage: tilck.py --boot|--test|--rung NAME")
