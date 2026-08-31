#!/usr/bin/env python3
"""PTY QEMU virt session and QMP client."""
import json
import os
import pty
import select
import socket
import subprocess
import sys
import termios
import time

ROOT = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
)
IMAGE = os.path.join(ROOT, "src", "stages", "0-simon", "artifacts", "image.bin")
QEMU = "/opt/homebrew/bin/qemu-system-riscv64"
QMP_SOCK = os.path.join(ROOT, "build", "simon", "qemu-qmp.sock")
PROMPT = "Simon says~: "
BUF_CAP = 262144


class Qmp:
    """Minimal QMP client over a UNIX socket."""

    def __init__(self, path):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        deadline = time.time() + 15
        while True:
            try:
                self.sock.connect(path)
                break
            except OSError as err:
                if time.time() >= deadline:
                    raise RuntimeError(f"qmp connect {path}: {err}") from err
                time.sleep(0.1)
        self._read()
        self.call("qmp_capabilities")

    def close(self):
        self.sock.close()

    def call(self, execute, arguments=None):
        msg = {"execute": execute}
        if arguments is not None:
            msg["arguments"] = arguments
        self.sock.sendall((json.dumps(msg) + "\n").encode())
        while True:
            obj = self._read()
            if "return" in obj:
                return obj["return"]
            if "error" in obj:
                raise RuntimeError(str(obj["error"]))

    def pmemsave(self, addr, size, dest):
        cmd = f"pmemsave {addr} {size} {dest}"
        self.call("human-monitor-command", {"command-line": cmd})

    def _read(self):
        if not hasattr(self, "_buf"):
            self._buf = b""
        while b"\n" not in self._buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise RuntimeError("qmp closed")
            self._buf += chunk
        line, _, self._buf = self._buf.partition(b"\n")
        return json.loads(line.decode())


class Qemu:
    """PTY-backed QEMU virt session with QMP."""

    def __init__(self, memory="6G", smp="4", qmp_sock=None, image=None):
        """Start qemu-system-riscv64 with a raw PTY UART."""
        qmp = qmp_sock or QMP_SOCK
        bios = image or IMAGE
        self.qmp_sock = qmp
        sock_dir = os.path.dirname(qmp)
        if sock_dir:
            os.makedirs(sock_dir, exist_ok=True)
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
                "-gdb",
                "tcp:127.0.0.1:12345,server,nowait",
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
            r, _, _ = select.select(
                [self.master], [], [], min(0.2, max(0, remaining))
            )
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
                    uart = (
                        self.buf[echo_at + len(line) :]
                        if echo_at >= 0
                        else self.buf
                    )
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
