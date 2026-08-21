#!/usr/bin/env python3
"""Dump Simon ramfs binaries from QEMU and pack them as hex0 for --tcc-only."""
import json
import os
import socket
import struct
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE_DIR = os.path.join(ROOT, "build", "tcc-cache")
QMP_SOCK = os.path.join(ROOT, "build", "qemu-qmp.sock")
READY = os.path.join(CACHE_DIR, "ready")

# boards/qemu_virt.h
FILE_NAME_BASE = 0x80920000
FILE_NAME_BYTES = 0x80000
FILE_NAME_SLOT = 0x400
FD_BASE = 0x80820000
FD_BYTES = 1024 * 16

# Guest-built hex0 cache. mes1/mes2/mes3 are optional (4-way relink).
CACHE_STEMS = ("mes", "m1", "h2", "catm")
QUAD_STEMS = ("mes1", "mes2", "mes3")


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


def cache_ready():
    """True when a previous dump wrote all hex0 stems."""
    if not os.path.isfile(READY):
        return False
    return all(
        os.path.isfile(os.path.join(CACHE_DIR, f"{stem}.hex0"))
        for stem in CACHE_STEMS
    )


def bytes_to_hex0(data):
    """Encode a binary as Simon hex0 text."""
    lines = []
    for i in range(0, len(data), 4):
        chunk = data[i : i + 4]
        lines.append(" ".join(f"{byte:02X}" for byte in chunk))
    return "\n".join(lines) + ("\n" if lines else "")


def dump_tcc_cache(qmp):
    """Copy mes/m1/h2/catm binaries out of guest RAM into build/tcc-cache/."""
    os.makedirs(CACHE_DIR, exist_ok=True)
    names_path = os.path.join(CACHE_DIR, "_names.bin")
    fds_path = os.path.join(CACHE_DIR, "_fds.bin")
    qmp.pmemsave(FILE_NAME_BASE, FILE_NAME_BYTES, names_path)
    qmp.pmemsave(FD_BASE, FD_BYTES, fds_path)
    names = open(names_path, "rb").read()
    fds = open(fds_path, "rb").read()
    wanted = {f"{stem}.bin": stem for stem in CACHE_STEMS + QUAD_STEMS}
    found = {}
    for index in range(FILE_NAME_BYTES // FILE_NAME_SLOT):
        slot = names[index * FILE_NAME_SLOT : (index + 1) * FILE_NAME_SLOT]
        name = slot.split(b"\0", 1)[0].decode("ascii", "replace")
        if name not in wanted:
            continue
        addr, length = struct.unpack_from("<QQ", fds, index * 16)
        if length == 0 or length > 0x4000000:
            raise RuntimeError(f"{name}: bad length {length}")
        raw_path = os.path.join(CACHE_DIR, f"{wanted[name]}.bin")
        qmp.pmemsave(addr, length, raw_path)
        data = open(raw_path, "rb").read()
        hex_path = os.path.join(CACHE_DIR, f"{wanted[name]}.hex0")
        open(hex_path, "w", encoding="ascii").write(bytes_to_hex0(data))
        found[name] = length
        print(f"[cache] {name}: {length} bytes", flush=True)
    missing = [f"{stem}.bin" for stem in CACHE_STEMS if f"{stem}.bin" not in found]
    if missing:
        raise RuntimeError(f"cache missing {missing}")
    open(READY, "w", encoding="ascii").write("ok\n")
    print("[cache] ready for --tcc-only", flush=True)
