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
from qemu_tcc_cache import CACHE_STEMS, QMP_SOCK, Qmp, cache_ready, dump_tcc_cache

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
            if now - last_beat >= 10:
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
                self.wait_prompt(max(1, deadline - time.time()), tag)
                return
            if self._command_done(line):
                if expect is not None:
                    raise AssertionError(
                        f"prompt before expected {expect!r} ({tag})\n--- tail ---\n{self.buf[-4000:]}"
                    )
                elapsed = time.time() - started
                print(f"[done] {tag}: {elapsed:.2f}s", flush=True)
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


def main():
    tcc_only = "--tcc-only" in sys.argv
    if tcc_only:
        q = Qemu(qmp_sock=os.path.join(ROOT, "build", "qemu-tcc-qmp.sock"))
    else:
        q = Qemu()
    try:
        if tcc_only:
            run_tcc_only(q)
            return
        q.wait_prompt(30, "boot")
        q.drain_quiet()

        q.cmd("hex0 hex1.hex0", 30)
        q.cmd("do hex1.bin hex2.hex1 hex2.bin", 60)
        q.cmd("do hex2.bin M0.hex2 M0.bin", 60)
        q.cmd("do hex2.bin catm.hex2 catm.bin", 30)
        q.cmd("do M0.bin cc.M1 cc.hex2", 180)
        q.cmd("do hex2.bin cc.hex2 cc.bin", 60)

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
        # spawn takes the first free hart as the slot: mes1→1, mes2→2, mes3→3.
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
