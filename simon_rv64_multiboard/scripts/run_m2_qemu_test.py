#!/usr/bin/env python3
"""Drive Simon in QEMU through hex0 → M2 → blood-elf → M1-0 → hex2-1."""
import os
import pty
import select
import subprocess
import sys
import termios
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dump_guest_files import dump_named as dump_guest_named
from guest_ram import find_ram_host, plant_files
from qemu_tcc_cache import CACHE_STEMS, QMP_SOCK, Qmp, cache_ready, dump_tcc_cache
from stage_images import CACHE, IMAGES, parse_stage, restore_cmds

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMAGE = os.path.join(ROOT, "build", "image_qemu.bin")
QEMU = "/opt/homebrew/bin/qemu-system-riscv64"
PROMPT = "Simon says~: "
# Keep a short tail only. Echoing every UART byte to the IDE PTY stalls QEMU
# (guest blocks on serial TX) so the host looks idle except this Python.
BUF_CAP = 262144
TCC_WORKER_OK = ("tcc-p0-ok", "tcc-p1-ok", "tcc-p2-ok", "tcc-p3-ok")


class Qemu:
    """PTY-backed QEMU virt session with QMP."""

    def __init__(self, memory="6G", smp="4", qmp_sock=None, image=None):
        """Start qemu-system-riscv64 with a raw PTY UART.

        @param memory - QEMU -m value
        @param smp - QEMU -smp value
        @param qmp_sock - UNIX socket path for QMP
        @param image - BIOS/image path
        @returns None
        """
        qmp = qmp_sock or QMP_SOCK
        bios = image or IMAGE
        self.qmp_sock = qmp
        self.master, slave = pty.openpty()
        attrs = termios.tcgetattr(slave)
        attrs[0] &= ~(
            termios.ICRNL | termios.INLCR | termios.IGNCR | termios.IXON
        )
        attrs[1] &= ~(termios.OPOST | termios.ONLCR | termios.OCRNL)
        attrs[3] &= ~(
            termios.ECHO
            | termios.ECHOE
            | termios.ECHOK
            | termios.ICANON
            | termios.ISIG
            | termios.IEXTEN
        )
        termios.tcsetattr(slave, termios.TCSANOW, attrs)
        try:
            os.unlink(qmp)
        except OSError:
            pass
        self.proc = subprocess.Popen(
            [
                QEMU,
                "-M",
                "virt",
                "-accel",
                "tcg,tb-size=64",
                "-m",
                memory,
                "-smp",
                smp,
                "-nographic",
                "-bios",
                bios,
                "-qmp",
                f"unix:{qmp},server,nowait",
            ],
            stdin=slave,
            stdout=slave,
            stderr=slave,
            close_fds=True,
        )
        os.close(slave)
        self.buf = ""
        self.uart_bytes = 0
        self.seen_ok = set()

    def close(self):
        if self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.proc.kill()
        try:
            os.close(self.master)
        except OSError:
            pass

    def read_more(self, timeout):
        end = time.time() + timeout
        while time.time() < end:
            remaining = end - time.time()
            r, _, _ = select.select([self.master], [], [], min(0.2, max(0, remaining)))
            if not r:
                continue
            try:
                chunk = os.read(self.master, 65536)
            except OSError:
                return False
            if not chunk:
                return False
            self.uart_bytes += len(chunk)
            self.buf += chunk.decode("utf-8", "replace")
            for marker in TCC_WORKER_OK:
                if marker in self.buf:
                    self.seen_ok.add(marker)
            if len(self.buf) > BUF_CAP:
                self.buf = self.buf[-BUF_CAP:]
            return True
        return False

    def wait_prompt(self, timeout, label=""):
        deadline = time.time() + timeout
        while time.time() < deadline:
            if PROMPT in self.buf:
                idx = self.buf.rfind(PROMPT)
                self.buf = self.buf[idx + len(PROMPT) :]
                return
            if not self.read_more(min(0.5, deadline - time.time())):
                continue
        raise TimeoutError(
            f"timeout waiting for prompt ({label})\n--- tail ---\n{self.buf[-4000:]}"
        )

    def drain_quiet(self, quiet_for=0.5):
        """Swallow extra boot prompts/hart-up lines until UART is idle."""
        deadline = time.time() + 5
        last = time.time()
        while time.time() < deadline:
            if self.read_more(0.2):
                last = time.time()
                while PROMPT in self.buf:
                    idx = self.buf.rfind(PROMPT)
                    self.buf = self.buf[idx + len(PROMPT) :]
            elif time.time() - last >= quiet_for:
                return

    def _command_done(self, line):
        """True after SiSH reprints an idle prompt, not the typed echo line."""
        start = 0
        saw_echo = line in self.buf
        if not saw_echo:
            return False
        echo_at = self.buf.find(line)
        while True:
            idx = self.buf.find(PROMPT, start)
            if idx < 0:
                return False
            if idx < echo_at:
                start = idx + 1
                continue
            after = self.buf[idx + len(PROMPT) :]
            if after.startswith(line):
                start = idx + 1
                continue
            return True

    def cmd(self, line, timeout, expect=None, label=None):
        tag = label or line
        print(f"\n>>> {line}", flush=True)
        for marker in TCC_WORKER_OK:
            if marker in self.buf:
                self.seen_ok.add(marker)
        self.buf = ""
        os.write(self.master, (line + "\r").encode())
        deadline = time.time() + timeout
        started = time.time()
        last_beat = started
        last_uart = self.uart_bytes
        while time.time() < deadline:
            now = time.time()
            if now - last_beat >= 20:
                produced = self.uart_bytes - last_uart
                last_uart = self.uart_bytes
                alive = "up" if self.proc.poll() is None else "dead"
                print(
                    f"\n[wait] {tag}: {int(now - started)}s / {timeout}s, "
                    f"+{produced} uart bytes, qemu={alive}",
                    file=sys.stderr,
                    flush=True,
                )
                last_beat = now
            if "Parse error" in self.buf:
                raise AssertionError(
                    f"pnut parse error during {tag!r}\n--- tail ---\n{self.buf[-4000:]}"
                )
            if "code buffer overflow" in self.buf:
                raise AssertionError(
                    f"pnut code buffer overflow during {tag!r}\n--- tail ---\n{self.buf[-2000:]}"
                )
            if "identifier not found" in self.buf:
                raise AssertionError(
                    f"pnut identifier not found during {tag!r}\n--- tail ---\n{self.buf[-2000:]}"
                )
            if "[KERNEL PANIC]" in self.buf or "[TRAP]" in self.buf:
                drain = time.time() + 2
                while time.time() < drain:
                    if "Unhandled exception" in self.buf:
                        break
                    if not self.read_more(min(0.2, drain - time.time())):
                        break
                raise AssertionError(
                    f"trap/panic during {tag!r}\n--- tail ---\n{self.buf[-4000:]}"
                )
            echo_at = self.buf.find(line)
            tail = self.buf[echo_at + len(line) :] if echo_at >= 0 else ""
            if expect is not None and expect in tail:
                print(f"[got] {expect}", flush=True)
                self.last_cmd_buf = self.buf
                self.wait_prompt(max(1, deadline - time.time()), tag)
                return
            if self._command_done(line):
                if expect is not None:
                    raise AssertionError(
                        f"prompt before expected {expect!r} ({tag})\n--- tail ---\n{self.buf[-4000:]}"
                    )
                elapsed = time.time() - started
                print(f"[done] {tag}: {elapsed:.2f}s", flush=True)
                self.last_cmd_buf = self.buf
                if (
                    "PNUT.c" in line
                    or "pnut.bin" in line
                    or "ck.bin" in line
                    or "TCC.c" in line
                    or "tcc-pnut.bin" in line
                    or "simon_crt" in line
                ):
                    echo_at = self.buf.find(line)
                    uart = self.buf[echo_at + len(line) :] if echo_at >= 0 else self.buf
                    print("--- M2 uart ---", flush=True)
                    print(uart[-16000:], flush=True)
                    print("--- end M2 uart ---", flush=True)
                    if (
                        "Unknown type" in uart
                        or "you probably don't want to allocate" in uart
                        or "is not a defined symbol" in uart
                    ):
                        raise AssertionError(
                            "M2 failed to compile PNUT.c\n--- uart ---\n"
                            + uart[-4000:]
                        )
                self.wait_prompt(max(1, deadline - time.time()), tag)
                return
            self.read_more(min(0.5, deadline - time.time()))
        raise TimeoutError(
            f"timeout on {tag!r} expect={expect!r}\n--- tail ---\n{self.buf[-4000:]}"
        )


def run_tcc_only(q):
    """Skip hex0→mes: restore qemu-built tools, then sequential mescc.

    mes1/mes2/mes3 need a different hex2 base; the cache dump only has
    mes.bin at 0x82000000, so 4-way spawn is not used here.
    """
    if not cache_ready():
        raise SystemExit(
            "no TCC cache; run the full ladder once, then: "
            "python3 scripts/run_m2_qemu_test.py --tcc-only"
        )
    q.wait_prompt(30, "boot")
    q.drain_quiet()
    for stem in CACHE_STEMS:
        q.cmd(f"hex0 {stem}.hex0", 120)
    q.cmd("do mes.bin hello.scm", 600, expect="Hello,M2-mes!")
    print("\n[note] sequential mescc (cached mes.bin, new as.scm).", flush=True)
    q.cmd("do mes.bin -e main tcc-all.scm", 43200, expect="tcc-mescc-all-ok")
    q.cmd("do mes.bin -e main tcc-libc.scm", 43200, expect="libc-ok")
    q.cmd(
        "do catm.bin tccin.M1 mes_as.M1 crtfix.M1 libc-tcc.s "
        "tccpp.s tccgen.s tccelf.s riscv64-gen.s riscv64-link.s "
        "riscv64-asm.s tccasm.s libtcc.s tcc.s",
        60,
    )
    q.cmd("do m1.bin -A riscv64 --little-endian -f tccin.M1 -o tcc.hex2", 900)
    q.cmd(
        "do h2.bin -A riscv64 --little-endian -B 0x82000000 -f tcc.hex2 -o tcc.bin",
        1800,
    )
    q.cmd("do tcc.bin -version", 60, expect="tcc version 0.9.26")
    print("\nTCC 0.9.26 OK", flush=True)


def run_pnut_tcc(q, from_stage="06"):
    """M2-compile pnut-exe (RV64 backend) and compile tcc.c to a Simon binary.

    Success: `do tcc-pnut.bin -version` prints tcc version 0.9.26.
    """
    q.wait_prompt(30, "boot")
    q.drain_quiet()
    if from_stage is not None:
        print("[from] stage %s image, restoring bins" % from_stage, flush=True)
        for line in restore_cmds(from_stage):
            q.cmd(line, 120)
    else:
        q.cmd("hex0 hex1.hex0", 30)
        q.cmd("do hex1.bin hex2.hex1 hex2.bin", 60)
        q.cmd("do hex2.bin M0.hex2 M0.bin", 60)
        q.cmd("do hex2.bin catm.hex2 catm.bin", 30)
        q.cmd("do M0.bin cc.M1 cc.hex2", 180)
        q.cmd("do hex2.bin cc.hex2 cc.bin", 60)
        print("\n[note] compiling M2.c", flush=True)
        q.cmd("do cc.bin M2.c M2.M1", 900)
        q.cmd("do catm.bin tM.M1 rt.M1 M2.M1", 60)
        q.cmd("do M0.bin tM.M1 tM.hex2", 600)
        q.cmd("do hex2.bin tM.hex2 M2.bin", 300)
        q.cmd("do M2.bin -A riscv64 --bootstrap-mode -o tG.M1 testF.c", 180)
        q.cmd("do catm.bin tG2.M1 rt.M1 tG.M1", 30)
        q.cmd("do M0.bin tG2.M1 tG.hex2", 60)
        q.cmd("do hex2.bin tG.hex2 tG.bin", 30)
        q.cmd("do tG.bin", 30, expect="hello from C")
        print("\n[note] compiling M1.c / H2.c with M2", flush=True)
        q.cmd("do M2.bin -A riscv64 --bootstrap-mode -o m1.M1 M1.c", 300)
        q.cmd("do catm.bin m12.M1 rt.M1 m1.M1", 30)
        q.cmd("do M0.bin m12.M1 m1.hex2", 180)
        q.cmd("do hex2.bin m1.hex2 m1.bin", 60)
        q.cmd("do m1.bin -V", 30, expect="M1 1.7.0")
        q.cmd("do M2.bin -A riscv64 --bootstrap-mode -o h2.M1 H2.c", 300)
        q.cmd("do catm.bin h22.M1 rt.M1 h2.M1", 30)
        q.cmd("do M0.bin h22.M1 h2.hex2", 180)
        q.cmd("do hex2.bin h2.hex2 h2.bin", 60)
        q.cmd("do h2.bin -V", 30, expect="hex2 1.7.0")

    print("\n[note] compiling CHECK.c (ELF magic probe)", flush=True)
    q.cmd("do M2.bin -A riscv64 -o ck.M1 CHECK.c", 60)
    q.cmd("do catm.bin ck2.M1 rt.M1 ck.M1", 30)
    q.cmd("do m1.bin -A riscv64 --little-endian -f ck2.M1 -o ck.hex2", 180)
    q.cmd(
        "do h2.bin -A riscv64 --little-endian -B 0x82000000 -f ck.hex2 -o ck.bin",
        60,
    )

    print("\n[note] compiling pnut-exe with M2 (riscv64 backend)", flush=True)
    q.cmd("do M2.bin -A riscv64 --max-string 8192 -o pnut.M1 PNUT.c", 120)
    q.cmd("do catm.bin pnut2.M1 rt.M1 pnut.M1", 30)
    q.cmd("do m1.bin -A riscv64 --little-endian -f pnut2.M1 -o pnut.hex2", 90)
    pnut_out = os.path.join(ROOT, "build", "pnut-cache")
    ram_host = find_ram_host(q.proc.pid)
    try:
        dumped = dump_guest_named(
            q.proc.pid, pnut_out, ["pnut.hex2"], ram_host=ram_host
        )
        print("[pnut] pnut.hex2 dump %s" % dumped, flush=True)
    except Exception as err:
        print("[pnut] pnut.hex2 dump failed: %s" % err, flush=True)
    q.cmd(
        "do h2.bin -A riscv64 --little-endian -B 0x82000000 -f pnut.hex2 -o pnut.bin",
        120,
    )
    if "not valid" in q.last_cmd_buf:
        raise AssertionError("hex2 failed linking pnut.bin:\n%s" % q.last_cmd_buf[-2000:])
    print("\npnut-exe linked", flush=True)
    pnut_out = os.path.join(ROOT, "build", "pnut-cache")
    ram_host = find_ram_host(q.proc.pid)
    dumped = dump_guest_named(
        q.proc.pid, pnut_out, ["pnut.bin"], ram_host=ram_host
    )
    print("[pnut] host dump %s" % dumped, flush=True)

    def pnut_uart(marker):
        echo_at = q.last_cmd_buf.find(marker)
        uart = q.last_cmd_buf[echo_at:] if echo_at >= 0 else q.last_cmd_buf
        if "Syntax error" in uart:
            raise AssertionError("pnut syntax error:\n%s" % uart[-2000:])
        if "Parse error" in uart:
            raise AssertionError("pnut parse error:\n%s" % uart[-2000:])
        if "identifier not found" in uart:
            raise AssertionError("pnut missing identifier:\n%s" % uart[-2000:])
        if "unexpected operator" in uart:
            raise AssertionError("pnut codegen error:\n%s" % uart[-2000:])
        if "type is not defined" in uart:
            raise AssertionError("pnut undefined type:\n%s" % uart[-2000:])
        if "pnut_tcc.c" in marker and "100%" not in uart:
            raise AssertionError(
                "pnut did not finish pnut_tcc.c:\n%s" % uart[-2000:]
            )
        return uart

    print("\n[note] pnut compiling pnut_tcc.c ONE_SOURCE amalgamation", flush=True)
    q.cmd("do pnut.bin pnut_tcc.c -o tcc-pnut.bin", 300)
    uart = pnut_uart("do pnut.bin pnut_tcc.c")
    print("--- pnut pnut_tcc.c uart ---", flush=True)
    print(uart[-16000:], flush=True)
    print("--- end pnut pnut_tcc.c uart ---", flush=True)
    tcc_out = os.path.join(ROOT, "build", "pnut-cache")
    dumped = dump_guest_named(
        q.proc.pid, tcc_out, ["tcc-pnut.bin"], ram_host=ram_host
    )
    print("[pnut] tcc-pnut.bin dump %s" % dumped, flush=True)
    tcc_path = os.path.join(tcc_out, "tcc-pnut.bin")
    tcc_size = os.path.getsize(tcc_path)
    with open(tcc_path, "rb") as tf:
        raw = tf.read(32)
    print("[pnut] tcc-pnut.bin size %d head %s" % (tcc_size, raw.hex()), flush=True)
    if tcc_size < 50000:
        raise AssertionError(
            "tcc-pnut.bin too small (%d bytes); ONE_SOURCE amalgamation did not compile"
            % tcc_size
        )
    q.cmd("do ck.bin tcc-pnut.bin", 30, expect="pnut-tcc-ok")
    print("\n[note] running tcc-pnut.bin -version", flush=True)
    q.cmd("do tcc-pnut.bin -version", 20, expect="tcc version 0.9.26")
    print("\npnut TCC compile and run OK", flush=True)


def run_tcc_selfhost(q, from_stage="09"):
    """tcc-pnut.bin compiles stages/08-tcc (ONE_SOURCE tcc.c) to tcc-tcc.bin.

    Success: `do tcc-tcc.bin -version` prints tcc version 0.9.26.
    """
    q.wait_prompt(30, "boot")
    q.drain_quiet()
    ram_host = find_ram_host(q.proc.pid)
    blob = open(os.path.join(CACHE, "tcc-pnut.bin"), "rb").read()
    print("[plant] tcc-pnut.bin %d bytes" % len(blob), flush=True)
    nxt = plant_files(
        q.proc.pid,
        ram_host,
        [("tcc-pnut.bin", blob)],
        dest_base=0x84000000,
        update_file_ptr=False,
    )
    q.cmd("do tcc-pnut.bin -version", 20, expect="tcc version 0.9.26")
    out = os.path.join(ROOT, "build", "pnut-cache")

    pnut_dir = os.path.join(ROOT, "stages", "09-pnut")
    crt = open(os.path.join(pnut_dir, "tcc_crt.s"), "rb").read()
    rsp = open(os.path.join(pnut_dir, "tcc_self.rsp"), "rb").read()
    plant_files(
        q.proc.pid,
        ram_host,
        [("tcc_crt.s", crt), ("self.rsp", rsp)],
        dest_base=nxt,
        update_file_ptr=False,
    )

    print("\n[note] smoke: @tcc_hi.rsp", flush=True)
    q.cmd("do tcc-pnut.bin @tcc_hi.rsp", 15)
    uart = q.last_cmd_buf
    print("--- tcc smoke uart ---", flush=True)
    print(uart[-4000:], flush=True)
    print("--- end tcc smoke uart ---", flush=True)
    if "%s" in uart.split("tcc-main", 1)[-1]:
        raise AssertionError("tcc-pnut failed to link smoke")
    ram_host = find_ram_host(q.proc.pid)
    dumped = dump_guest_named(
        q.proc.pid, out, ["tcc-hi.bin"], ram_host=ram_host
    )
    print("[tcc-self] tcc-hi.bin dump %s" % dumped, flush=True)
    hi = os.path.join(out, "tcc-hi.bin")
    if os.path.isfile(hi):
        raw = open(hi, "rb").read(16)
        print(
            "[tcc-self] tcc-hi.bin size %d head %s"
            % (os.path.getsize(hi), raw.hex()),
            flush=True,
        )
    q.cmd("do tcc-hi.bin", 15, expect="tcc-crt")

    print("\n[note] tcc-pnut compiling 08-tcc tcc.c", flush=True)
    q.cmd("do tcc-pnut.bin @self.rsp", 900)
    uart = q.last_cmd_buf
    print("--- tcc selfhost uart ---", flush=True)
    print(uart[-16000:], flush=True)
    print("--- end tcc selfhost uart ---", flush=True)
    tail = uart.split("tcc-main", 1)[-1]
    if (
        "too many arguments" in tail
        or "include file" in tail
        or "duplicate case" in tail
        or "error:" in tail
    ):
        raise AssertionError("tcc-pnut failed to compile 08-tcc")
    ram_host = find_ram_host(q.proc.pid)
    dumped = dump_guest_named(
        q.proc.pid, out, ["tcc-tcc.bin"], ram_host=ram_host
    )
    print("[tcc-self] dump %s" % dumped, flush=True)
    path = os.path.join(out, "tcc-tcc.bin")
    size = os.path.getsize(path)
    print("[tcc-self] tcc-tcc.bin size %d" % size, flush=True)
    if size < 50000:
        raise AssertionError("tcc-tcc.bin too small (%d)" % size)
    print("\n[note] running tcc-tcc.bin -version", flush=True)
    q.cmd("do tcc-tcc.bin -version", 60, expect="tcc version 0.9.26")
    print("\nTCC self-host OK", flush=True)


def parse_from_arg(argv):
    """Read --from 07 / --from=07. None if the flag is absent.

    @param argv - sys.argv
    @returns Two-digit stage id or None
    """
    if "--from" in argv:
        idx = argv.index("--from")
        if idx + 1 >= len(argv):
            raise SystemExit("--from needs a stage id (00..07)")
        return parse_stage(argv[idx + 1])
    for arg in argv:
        if arg.startswith("--from="):
            return parse_stage(arg.split("=", 1)[1])
    return None


def main():
    tcc_only = "--tcc-only" in sys.argv
    pnut_tcc = "--pnut" in sys.argv
    tcc_self = "--tcc-self" in sys.argv
    from_stage = parse_from_arg(sys.argv)
    if tcc_only:
        q = Qemu(qmp_sock=os.path.join(ROOT, "build", "qemu-tcc-qmp.sock"))
    elif tcc_self:
        if from_stage is None:
            from_stage = "09"
        image = os.path.join(IMAGES, "stage%s.bin" % from_stage)
        if not os.path.isfile(image):
            raise SystemExit(
                "no %s; build checkpoints with: python3 scripts/stage_images.py --build"
                % image
            )
        q = Qemu(
            image=image,
            qmp_sock=os.path.join(ROOT, "build", "qemu-tcc-self-qmp.sock"),
        )
    elif pnut_tcc:
        if from_stage is None:
            from_stage = "06"
        image = os.path.join(IMAGES, "stage%s.bin" % from_stage)
        if not os.path.isfile(image):
            raise SystemExit(
                "no %s; build checkpoints with: python3 scripts/stage_images.py --build"
                % image
            )
        q = Qemu(
            image=image,
            qmp_sock=os.path.join(ROOT, "build", "qemu-pnut-qmp.sock"),
        )
    elif from_stage is not None:
        image = os.path.join(IMAGES, "stage%s.bin" % from_stage)
        if not os.path.isfile(image):
            raise SystemExit(
                "no %s; build checkpoints with: python3 scripts/stage_images.py --build"
                % image
            )
        q = Qemu(
            image=image,
            qmp_sock=os.path.join(ROOT, "build", "qemu-from%s.sock" % from_stage),
        )
    else:
        q = Qemu()
    try:
        if tcc_only:
            run_tcc_only(q)
            return
        if tcc_self:
            run_tcc_selfhost(q, from_stage=from_stage)
            return
        if pnut_tcc:
            run_pnut_tcc(q, from_stage=from_stage)
            return
        have = -1 if from_stage is None else int(from_stage)
        q.wait_prompt(30, "boot")
        q.drain_quiet()
        if from_stage is not None:
            print("[from] stage %s image, restoring bins" % from_stage, flush=True)
            for line in restore_cmds(from_stage):
                q.cmd(line, 120)

        if have < 0:
            q.cmd("hex0 hex1.hex0", 30)
        if have < 1:
            q.cmd("do hex1.bin hex2.hex1 hex2.bin", 60)
        if have < 2:
            q.cmd("do hex2.bin M0.hex2 M0.bin", 60)
            q.cmd("do hex2.bin catm.hex2 catm.bin", 30)
        if have < 3:
            q.cmd("do M0.bin cc.M1 cc.hex2", 180)
            q.cmd("do hex2.bin cc.hex2 cc.bin", 60)

        if have < 5:
            print(
                "\n[note] compiling M2.c: kernel prints [bar] tokenize/compile/write on UART.",
                flush=True,
            )
            q.cmd("do cc.bin M2.c M2.M1", 900)
            q.cmd("do catm.bin tM.M1 rt.M1 M2.M1", 60)
            q.cmd("do M0.bin tM.M1 tM.hex2", 600)
            q.cmd("do hex2.bin tM.hex2 M2.bin", 300)

            q.cmd("do M2.bin -A riscv64 --bootstrap-mode -o tG.M1 testF.c", 180)
            q.cmd("do catm.bin tG2.M1 rt.M1 tG.M1", 30)
            q.cmd("do M0.bin tG2.M1 tG.hex2", 60)
            q.cmd("do hex2.bin tG.hex2 tG.bin", 30)
            q.cmd("do tG.bin", 30, expect="hello from C")
            print("\nM2 bootstrap OK", flush=True)

        if have < 6:
            print(
                "\n[note] compiling BE.c with M2 (blood-elf-0).",
                flush=True,
            )
            q.cmd("do M2.bin -A riscv64 --bootstrap-mode -o be.M1 BE.c", 300)
            q.cmd("do catm.bin be2.M1 rt.M1 be.M1", 30)
            q.cmd("do M0.bin be2.M1 be.hex2", 180)
            q.cmd("do hex2.bin be.hex2 be.bin", 60)
            q.cmd("do be.bin -V", 30, expect="blood-elf 2.0.1")
            q.cmd("do be.bin --64 --little-endian -f tG.M1 -o ft.M1", 60)
            print("\nblood-elf-0 OK", flush=True)

            print("\n[note] compiling M1.c with M2 (M1-0).", flush=True)
            q.cmd("do M2.bin -A riscv64 --bootstrap-mode -o m1.M1 M1.c", 300)
            q.cmd("do catm.bin m12.M1 rt.M1 m1.M1", 30)
            q.cmd("do M0.bin m12.M1 m1.hex2", 180)
            q.cmd("do hex2.bin m1.hex2 m1.bin", 60)
            q.cmd("do m1.bin -V", 30, expect="M1 1.7.0")
            q.cmd("do catm.bin in.M1 rt.M1 tG.M1", 30)
            q.cmd("do m1.bin -A riscv64 --little-endian -f in.M1 -o n.hex2", 60)
            q.cmd("do hex2.bin n.hex2 n.bin", 30)
            q.cmd("do n.bin", 30, expect="hello from C")
            print("\nM1-0 OK", flush=True)

            print("\n[note] compiling H2.c with M2 (hex2-1).", flush=True)
            q.cmd("do M2.bin -A riscv64 --bootstrap-mode -o h2.M1 H2.c", 300)
            q.cmd("do catm.bin h22.M1 rt.M1 h2.M1", 30)
            q.cmd("do M0.bin h22.M1 h2.hex2", 180)
            q.cmd("do hex2.bin h2.hex2 h2.bin", 60)
            q.cmd("do h2.bin -V", 30, expect="hex2 1.7.0")
            q.cmd(
                "do h2.bin -A riscv64 --little-endian -B 0x82000000 -f n.hex2 -o p.bin",
                60,
            )
            q.cmd("do p.bin", 30, expect="hello from C")
            print("\nhex2-1 OK", flush=True)

        if have < 7:
            print("\n[note] compiling MES.c with M2 (mes-m2).", flush=True)
            q.cmd("do M2.bin -A riscv64 -o mes.M1 MES.c", 1800)
            q.cmd("do catm.bin mes2.M1 mes_rt.M1 mes.M1", 60)
            q.cmd("do m1.bin -A riscv64 --little-endian -f mes2.M1 -o mes.hex2", 900)
            q.cmd(
                "do h2.bin -A riscv64 --little-endian -B 0x82000000 -f mes.hex2 -o mes.bin",
                1800,
            )
            q.cmd("do mes.bin hello.scm", 600, expect="Hello,M2-mes!")
            print("\nmes-m2 OK", flush=True)

            print("\n[note] named-let+cond TCO (must not STACK FULL).", flush=True)
            q.cmd("do mes.bin tco-probe.scm", 1800, expect="tco-iter-ok")
            print("\nmes TCO OK", flush=True)

            print("\n[note] load nyacc parser/pprint via mes.", flush=True)
            q.cmd("do mes.bin probe.scm", 1800, expect="pprint-ok")
            print("\nnyacc modules OK", flush=True)

            print("\n[note] 4-way mescc: hi+tccpp / tccgen / tccelf+libtcc / rest.", flush=True)
            q.cmd(
                "do h2.bin -A riscv64 --little-endian -B 0x82800000 -f mes.hex2 -o mes1.bin",
                1800,
            )
            q.cmd(
                "do h2.bin -A riscv64 --little-endian -B 0x83000000 -f mes.hex2 -o mes2.bin",
                1800,
            )
            q.cmd(
                "do h2.bin -A riscv64 --little-endian -B 0x83800000 -f mes.hex2 -o mes3.bin",
                1800,
            )
            try:
                dump_tcc_cache(Qmp(QMP_SOCK))
            except Exception as err:
                print(f"[cache] dump skipped: {err}", flush=True)
            q.cmd("spawn mes1.bin -e main tcc-p1.scm", 180)
            q.cmd("spawn mes2.bin -e main tcc-p2.scm", 180)
            q.cmd("spawn mes3.bin -e main tcc-p3.scm", 180)
            q.cmd(
                "do mes.bin -e main tcc-p0.scm",
                43200,
                expect="tcc-p0-ok",
            )
            q.cmd("wait", 43200)
            missing = [m for m in TCC_WORKER_OK if m not in q.seen_ok]
            if missing:
                raise AssertionError(
                    f"mescc workers missing {missing}\n--- tail ---\n{q.buf[-4000:]}"
                )
            print("\nmescc TCC units OK (4-way)", flush=True)
        else:
            q.cmd("do mes.bin hello.scm", 600, expect="Hello,M2-mes!")
            print("\n[note] sequential mescc from stage-07 image.", flush=True)
            q.cmd("do mes.bin -e main tcc-all.scm", 43200, expect="tcc-mescc-all-ok")

        if have < 8:
            q.cmd("do mes.bin -e main tcc-libc.scm", 43200, expect="libc-ok")
            q.cmd(
                "do catm.bin tccin.M1 mes_as.M1 crtfix.M1 libc-tcc.s "
                "tccpp.s tccgen.s tccelf.s riscv64-gen.s riscv64-link.s "
                "riscv64-asm.s tccasm.s libtcc.s tcc.s",
                60,
            )
            q.cmd("do m1.bin -A riscv64 --little-endian -f tccin.M1 -o tcc.hex2", 900)
            q.cmd(
                "do h2.bin -A riscv64 --little-endian -B 0x82000000 -f tcc.hex2 -o tcc.bin",
                1800,
            )
            q.cmd("do tcc.bin -version", 60, expect="tcc version 0.9.26")
            print("\nTCC 0.9.26 OK", flush=True)
    finally:
        q.close()


if __name__ == "__main__":
    main()
