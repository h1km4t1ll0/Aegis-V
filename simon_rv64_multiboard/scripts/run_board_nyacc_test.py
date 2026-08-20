#!/usr/bin/env python3
"""Run the Simon bootstrap through GNU Mes/NYACC over a physical UART.

This runner is intended to be started on a Linux host after Simon has reached
its command prompt on a board.  It has no third-party Python dependencies and
never starts the experimental TCC stage.
"""

import argparse
import os
import select
import sys
import termios
import time


PROMPT = "Simon says~:"
BUFFER_LIMIT = 262144


# (command, timeout in seconds, required UART marker)
COMMANDS = [
    ("stat mes_rt.M1", 10, "size: 13513"),
    ("hex0 hex1.hex0", 60, None),
    ("do hex1.bin hex2.hex1 hex2.bin", 120, None),
    ("do hex2.bin M0.hex2 M0.bin", 120, None),
    ("do hex2.bin catm.hex2 catm.bin", 60, None),
    ("do M0.bin cc.M1 cc.hex2", 600, None),
    ("do hex2.bin cc.hex2 cc.bin", 180, None),
    ("do cc.bin M2.c M2.M1", 3600, None),
    ("do catm.bin tM.M1 rt.M1 M2.M1", 300, None),
    ("do M0.bin tM.M1 tM.hex2", 1800, None),
    ("do hex2.bin tM.hex2 M2.bin", 600, None),
    ("do M2.bin -A riscv64 --bootstrap-mode -o m1.M1 M1.c", 1200, None),
    ("do catm.bin m12.M1 rt.M1 m1.M1", 300, None),
    ("do M0.bin m12.M1 m1.hex2", 900, None),
    ("do hex2.bin m1.hex2 m1.bin", 600, None),
    ("do m1.bin -V", 60, "M1 1.7.0"),
    ("do M2.bin -A riscv64 --bootstrap-mode -o h2.M1 H2.c", 1200, None),
    ("do catm.bin h22.M1 rt.M1 h2.M1", 300, None),
    ("do M0.bin h22.M1 h2.hex2", 900, None),
    ("do hex2.bin h2.hex2 h2.bin", 600, None),
    ("do h2.bin -V", 60, "hex2 1.7.0"),
    ("stat mes_rt.M1", 10, "size: 13513"),
    ("do M2.bin -A riscv64 -o mes.M1 MES.c", 7200, None),
    ("stat mes_rt.M1", 10, "size: 13513"),
    ("do catm.bin mes2.M1 mes_rt.M1 mes.M1", 600, None),
    ("do m1.bin -A riscv64 --little-endian -f mes2.M1 -o mes.hex2", 3600, None),
    (
        "do h2.bin -A riscv64 --little-endian -B {user_code_base} "
        "-f mes.hex2 -o mes.bin",
        1200,
        None,
    ),
    ("do mes.bin hello.scm", 1800, "Hello,M2-mes!"),
    ("do mes.bin probe.scm", 3600, "pprint-ok"),
]

MESCC_SMOKE = (
    "do mes.bin -e main mescc.scm -- -S -v -o hi.s hi.c",
    7200,
    "dumping: hi.s",
)


def baud_constant(baud):
    value = getattr(termios, f"B{baud}", None)
    if value is None:
        supported = [
            str(rate)
            for rate in (9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600)
            if hasattr(termios, f"B{rate}")
        ]
        raise ValueError(
            f"unsupported baud rate {baud}; supported here: {', '.join(supported)}"
        )
    return value


def configure_serial(fd, baud):
    speed = baud_constant(baud)
    attrs = termios.tcgetattr(fd)

    # Raw 8N1, no flow control, no input/output translations.
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = termios.CLOCAL | termios.CREAD | termios.CS8
    attrs[3] = 0
    attrs[4] = speed
    attrs[5] = speed
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 0

    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    termios.tcflush(fd, termios.TCIOFLUSH)


class SimonSerial:
    def __init__(self, port, baud, timeout_scale, show_uart):
        try:
            self.fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        except OSError as error:
            raise SystemExit(f"cannot open serial port {port}: {error}") from error

        try:
            configure_serial(self.fd, baud)
        except Exception:
            os.close(self.fd)
            raise

        self.port = port
        self.timeout_scale = timeout_scale
        self.show_uart = show_uart
        self.buffer = ""
        self.uart_bytes = 0

    def close(self):
        if self.fd is not None:
            os.close(self.fd)
            self.fd = None

    def write(self, data):
        view = memoryview(data)
        while view:
            try:
                written = os.write(self.fd, view)
                view = view[written:]
            except BlockingIOError:
                select.select([], [self.fd], [], 1)

    def read_more(self, timeout):
        readable, _, _ = select.select([self.fd], [], [], max(0, timeout))
        if not readable:
            return False

        try:
            chunk = os.read(self.fd, 65536)
        except BlockingIOError:
            return False
        except OSError as error:
            raise ConnectionError(
                f"serial read failed on {self.port}: {error}"
            ) from error

        if not chunk:
            return False

        self.uart_bytes += len(chunk)
        text = chunk.decode("utf-8", "replace")
        self.buffer += text
        if len(self.buffer) > BUFFER_LIMIT:
            self.buffer = self.buffer[-BUFFER_LIMIT:]

        if self.show_uart:
            sys.stdout.write(text)
            sys.stdout.flush()
        return True

    def wait_prompt(self, timeout, label):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if PROMPT in self.buffer:
                index = self.buffer.rfind(PROMPT)
                output = self.buffer[:index]
                self.buffer = self.buffer[index + len(PROMPT) :]
                return output
            self.read_more(min(0.5, deadline - time.monotonic()))

        raise TimeoutError(
            f"timeout waiting for prompt ({label})\n--- UART tail ---\n"
            f"{self.buffer[-4000:]}"
        )

    def command(self, line, timeout, expected=None):
        print(f"\n>>> {line}", flush=True)
        self.write((line + "\r").encode())

        scaled_timeout = timeout * self.timeout_scale
        deadline = time.monotonic() + scaled_timeout
        started = time.monotonic()
        last_heartbeat = started
        last_uart = self.uart_bytes

        while time.monotonic() < deadline:
            now = time.monotonic()
            if now - last_heartbeat >= 10:
                produced = self.uart_bytes - last_uart
                last_uart = self.uart_bytes
                print(
                    f"\n[wait] {line}: {int(now - started)}s / "
                    f"{int(scaled_timeout)}s, +{produced} UART bytes",
                    file=sys.stderr,
                    flush=True,
                )
                last_heartbeat = now

            if "[KERNEL PANIC]" in self.buffer or "[TRAP]" in self.buffer:
                raise AssertionError(
                    f"trap/panic during {line!r}\n--- UART tail ---\n"
                    f"{self.buffer[-4000:]}"
                )

            if expected is not None and expected in self.buffer:
                print(f"\n[got] {expected}", flush=True)
                return self.wait_prompt(
                    max(1, deadline - time.monotonic()), line
                )

            if PROMPT in self.buffer:
                if expected is not None:
                    raise AssertionError(
                        f"prompt returned before {expected!r} during {line!r}\n"
                        f"--- UART tail ---\n{self.buffer[-4000:]}"
                    )
                elapsed = int(time.monotonic() - started)
                print(f"\n[done] {line}: {elapsed}s", flush=True)
                return self.wait_prompt(1, line)

            self.read_more(min(0.5, deadline - time.monotonic()))

        raise TimeoutError(
            f"timeout during {line!r}\n--- UART tail ---\n{self.buffer[-4000:]}"
        )


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="serial device, for example /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200, help="UART baud rate")
    parser.add_argument(
        "--user-code-base",
        type=lambda value: int(value, 0),
        default=0x82000000,
        help="hex2 load base; must match USER_CODE_BASE in the board config",
    )
    parser.add_argument(
        "--boot-timeout",
        type=int,
        default=300,
        help="seconds to wait for the initial Simon prompt",
    )
    parser.add_argument(
        "--timeout-scale",
        type=float,
        default=1.0,
        help="multiply every command timeout by this value",
    )
    parser.add_argument(
        "--mescc-smoke",
        action="store_true",
        help="also compile hi.c with mescc/NYACC; still does not run TCC",
    )
    parser.add_argument(
        "--no-kick",
        action="store_true",
        help="do not send an empty line to redraw an already running Simon prompt",
    )
    parser.add_argument(
        "--quiet-uart",
        action="store_true",
        help="hide raw UART output and show only runner status",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="print commands without opening a serial port",
    )
    args = parser.parse_args()

    if not args.dry_run and not args.port:
        parser.error("--port is required unless --dry-run is used")
    if args.timeout_scale <= 0:
        parser.error("--timeout-scale must be positive")
    return args


def selected_commands(include_mescc_smoke, user_code_base):
    base = f"0x{user_code_base:x}"
    commands = [
        (line.format(user_code_base=base), timeout, expected)
        for line, timeout, expected in COMMANDS
    ]
    if include_mescc_smoke:
        commands.append(MESCC_SMOKE)
    return commands


def main():
    args = parse_args()
    commands = selected_commands(args.mescc_smoke, args.user_code_base)

    if args.dry_run:
        for line, timeout, expected in commands:
            suffix = f"  # expect: {expected}" if expected else ""
            print(f"{line}{suffix}  [timeout {timeout}s]")
        print(f"\n{len(commands)} commands; TCC is not included.")
        return

    serial = SimonSerial(
        args.port,
        args.baud,
        args.timeout_scale,
        show_uart=not args.quiet_uart,
    )
    try:
        print(
            f"Connected to {args.port} at {args.baud} baud. "
            f"Waiting for {PROMPT!r}...",
            flush=True,
        )
        if not args.no_kick:
            serial.write(b"\r")
        serial.wait_prompt(args.boot_timeout, "boot")

        for line, timeout, expected in commands:
            serial.command(line, timeout, expected)

        if args.mescc_smoke:
            print("\nNYACC + mescc smoke test OK; TCC skipped", flush=True)
        else:
            print("\nGNU Mes/NYACC module test OK; TCC skipped", flush=True)
    finally:
        serial.close()


if __name__ == "__main__":
    main()
