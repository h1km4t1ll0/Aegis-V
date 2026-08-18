#!/usr/bin/env python3
"""Drive Simon in QEMU through hex0 → M2 → blood-elf → M1-0 → hex2-1."""
import os
import pty
import select
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMAGE = os.path.join(ROOT, "build", "image_qemu.bin")
QEMU = "/opt/homebrew/bin/qemu-system-riscv64"
PROMPT = "Simon says~:"
# Keep a short tail only. Echoing every UART byte to the IDE PTY stalls QEMU
# (guest blocks on serial TX) so the host looks idle except this Python.
BUF_CAP = 262144


class Qemu:
    def __init__(self):
        self.master, slave = pty.openpty()
        self.proc = subprocess.Popen(
            [
                QEMU,
                "-M",
                "virt",
                "-m",
                "2G",
                "-nographic",
                "-bios",
                IMAGE,
            ],
            stdin=slave,
            stdout=slave,
            stderr=slave,
            close_fds=True,
        )
        os.close(slave)
        self.buf = ""
        self.uart_bytes = 0

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

    def cmd(self, line, timeout, expect=None, label=None):
        tag = label or line
        print(f"\n>>> {line}", flush=True)
        os.write(self.master, (line + "\n").encode())
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
                raise AssertionError(
                    f"trap/panic during {tag!r}\n--- tail ---\n{self.buf[-4000:]}"
                )
            if expect is not None and expect in self.buf:
                print(f"[got] {expect}", flush=True)
                self.wait_prompt(max(1, deadline - time.time()), tag)
                return
            if PROMPT in self.buf:
                if expect is not None:
                    raise AssertionError(
                        f"prompt before expected {expect!r} ({tag})\n--- tail ---\n{self.buf[-4000:]}"
                    )
                elapsed = int(time.time() - started)
                print(f"[done] {tag}: {elapsed}s", flush=True)
                self.wait_prompt(1, tag)
                return
            self.read_more(min(0.5, deadline - time.time()))
        raise TimeoutError(
            f"timeout on {tag!r} expect={expect!r}\n--- tail ---\n{self.buf[-4000:]}"
        )


def main():
    q = Qemu()
    try:
        q.wait_prompt(30, "boot")

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
        q.cmd("do hex2.bin tM.hex2 M2.bin", 120)

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
            180,
        )
        q.cmd("do mes.bin hello.scm", 600, expect="Hello,M2-mes!")
        print("\nmes-m2 OK", flush=True)

        print("\n[note] load nyacc parser/pprint via mes.", flush=True)
        q.cmd("do mes.bin probe.scm", 1800, expect="pprint-ok")
        print("\nnyacc modules OK", flush=True)

        print("\n[note] mescc -S hi.c then tcc.c (tcc 0.9.26).", flush=True)
        q.cmd(
            "do mes.bin -e main mescc.scm -- -S -v -o hi.s hi.c",
            1800,
            expect="dumping: hi.s",
        )
        print("\nmescc hi.c OK", flush=True)
        q.cmd(
            "do mes.bin -e main mescc.scm -- -S -v -o tcc.s tcc.c",
            7200,
            expect="dumping: tcc.s",
        )
        q.cmd("do catm.bin tccin.M1 mes_as.M1 tcc.s", 60)
        q.cmd("do m1.bin -A riscv64 --little-endian -f tccin.M1 -o tcc.hex2", 900)
        q.cmd(
            "do h2.bin -A riscv64 --little-endian -B 0x82000000 -f tcc.hex2 -o tcc.bin",
            180,
        )
        q.cmd("do tcc.bin -version", 60, expect="tcc version 0.9.26")
        print("\nTCC 0.9.26 OK", flush=True)
    finally:
        q.close()


if __name__ == "__main__":
    main()
