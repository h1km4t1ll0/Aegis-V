#!/usr/bin/env python3
"""Watch the live QEMU ladder log and dump tcc.bin before QEMU exits."""
import os
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DUMP = os.path.join(os.path.dirname(os.path.abspath(__file__)), "guest_lldb_dump.py")
OUT_DIR = os.path.join(ROOT, "build", "tcc-extract")
STAMP = os.path.join(OUT_DIR, "done")
TRIGGERS = (
    "tcc version 0.9.26",
    "[got] tcc version 0.9.26",
    "TCC 0.9.26 OK",
)


def qemu_pid():
    """Return the live qemu-system-riscv64 PID or 0.

    @returns PID, or 0 if not running
    """
    proc = subprocess.run(
        ["pgrep", "-n", "qemu-system-riscv64"],
        capture_output=True,
        text=True,
        check=False,
    )
    text = proc.stdout.strip()
    if not text:
        return 0
    return int(text.splitlines()[-1])


def log_paths():
    """Terminal transcripts that belong to the live ladder.

    @returns List of file paths
    """
    term_dir = os.path.expanduser(
        "~/.cursor/projects/Users-hikmatillo-WebstormProjects-Aegis-V/terminals"
    )
    paths = []
    if not os.path.isdir(term_dir):
        return paths
    for name in os.listdir(term_dir):
        if not name.endswith(".txt"):
            continue
        path = os.path.join(term_dir, name)
        try:
            head = open(path, "r", encoding="utf-8", errors="replace").read(4000)
        except OSError:
            continue
        if "run_m2_qemu_test.py" in head:
            paths.append(path)
    return paths


def log_has_trigger(paths):
    """True when any watched log contains a dump trigger.

    @param paths - Log file paths
    @returns True if a trigger string is present
    """
    for path in paths:
        try:
            text = open(path, "r", encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        for marker in TRIGGERS:
            if marker in text:
                return True
    return False


def extract(pid):
    """Run the lldb dump helper.

    @param pid - QEMU PID
    @returns None
    @throws subprocess.CalledProcessError If dump fails
    """
    os.makedirs(OUT_DIR, exist_ok=True)
    subprocess.run(
        [sys.executable, DUMP, str(pid), OUT_DIR],
        check=True,
    )
    open(STAMP, "w", encoding="ascii").write("ok\n")


def main():
    """Poll logs until TCC reports version, then dump ramfs files."""
    print(f"[watch] out={OUT_DIR}", flush=True)
    while True:
        if os.path.isfile(STAMP):
            print("[watch] already dumped", flush=True)
            return
        pid = qemu_pid()
        paths = log_paths()
        if pid == 0:
            print("[watch] qemu gone before trigger", flush=True)
            raise SystemExit(1)
        if log_has_trigger(paths):
            print(f"[watch] trigger; dumping pid={pid}", flush=True)
            extract(pid)
            print("[watch] dump complete", flush=True)
            return
        time.sleep(2)


if __name__ == "__main__":
    main()
