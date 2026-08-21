#!/usr/bin/env python3
"""Relink guest TCC on a fresh QEMU with live UART: libc3 + m1 + hex2 + -v."""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from guest_ram import find_ram_host, plant_files
from pcrel_addi import patch_pcrel_addiw
from run_m2_qemu_test import Qemu

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DUMP = "/tmp/q34003-dump"
QMP = os.path.join(ROOT, "build", "qemu-relink-qmp.sock")
NEED = ("mes.bin", "m1.bin", "h2.bin", "catm.bin", "tccin4.M1")


def load_dump(name):
    """Read a guest-built blob from the 34003 dump.

    @param name - File name
    @returns File bytes
    @throws RuntimeError If the dump is missing
    """
    path = os.path.join(DUMP, name)
    if not os.path.isfile(path):
        raise RuntimeError("missing dump %s" % path)
    return open(path, "rb").read()


def main():
    """Boot QEMU, plant tools, compile missing libc, link, patch, run -v.

    @returns None
    """
    blobs = [(name, load_dump(name)) for name in NEED]
    q = Qemu(memory="2G", smp="1", qmp_sock=QMP)
    try:
        q.wait_prompt(30, "boot")
        q.drain_quiet()
        pid = q.proc.pid
        ram = find_ram_host(pid)
        print("qemu pid", pid, "ram_host", hex(ram), flush=True)
        plant_files(pid, ram, blobs, 0x84000000)
        q.cmd("do mes.bin -e main libc3.scm", 3600, expect="libc3-ok")
        q.cmd(
            "do catm.bin tccin5.M1 tccin4.M1 isspace.s islower.s isdigit.s "
            "isxdigit.s isnumber.s toupper.s strcat.s assert_msg.s abtod.s",
            60,
        )
        q.cmd(
            "do m1.bin -A riscv64 --little-endian -f tccin5.M1 -o tcc8.hex2",
            900,
        )
        q.cmd(
            "do h2.bin -A riscv64 --little-endian -B 0x82000000 "
            "-f tcc8.hex2 -o tcc8.bin",
            1800,
        )
        from dump_guest_files import dump_named
        dump_named(pid, "/tmp/tcc8-out", ["tcc8.bin"], ram)
        raw = bytearray(open("/tmp/tcc8-out/tcc8.bin", "rb").read())
        print("tcc8.bin", len(raw), "bytes", flush=True)
        if len(raw) <= 866000:
            raise RuntimeError("tcc8.bin still truncated (%d)" % len(raw))
        n = patch_pcrel_addiw(raw)
        print("pcrel patched", n, flush=True)
        plant_files(pid, ram, [("tcc.bin", bytes(raw))], 0x86000000)
        q.cmd("do tcc.bin -version", 60, expect="tcc version 0.9.26")
        print("\nTCC 0.9.26 OK  qemu pid %d (PTY held)" % pid, flush=True)
        while q.proc.poll() is None:
            q.read_more(2)
    except Exception:
        print("leaving qemu pid", q.proc.pid, "up for debug", flush=True)
        raise


if __name__ == "__main__":
    main()
