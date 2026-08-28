#!/usr/bin/env python3
"""Per-phase layout.

  src/stages/<phase>/
    README.md
    src/                       canonical sources
    tests/                     hello / probes
    artifacts/                 bins + image.bin (gitignored)

  build/simon/src/files.pl     packed ramfs (seed)
  build/<phase>/{src,tests}/   packed guest names (unpack of files.pl)
"""
import os
import shutil
import sys

_SCRIPTS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _SCRIPTS not in sys.path:
    sys.path.insert(0, _SCRIPTS)
from lib.image import parse_pl
from lib.qemu import ROOT

BUILD = os.path.join(ROOT, "build")
STAGES = os.path.join(ROOT, "src", "stages")

STAGE_DIR = {
    "00": "00-hex0",
    "01": "01-hex1",
    "02": "02-hex2",
    "03": "03-m0",
    "04": "04-cc",
    "05": "05-m2",
    "06": "06-tools",
    "07": "07-tcc",
    "08": "08-pnut",
    "09": "09-kronos",
}

ARTIFACT_STEM = {
    "hex1": "00-hex0",
    "hex2": "01-hex1",
    "M0": "02-hex2",
    "catm": "02-hex2",
    "cc": "03-m0",
    "M2": "05-m2",
    "m1": "06-tools",
    "h2": "06-tools",
    "pnut": "08-pnut",
    "tcc-pnut": "08-pnut",
}

PACKED_TESTS = {
    "testA.hex0": "00-hex0",
    "testB.hex1": "01-hex1",
    "testC.hex2": "02-hex2",
    "testD.M0": "03-m0",
    "testE.M1": "03-m0",
    "testF.c": "04-cc",
}

PACKED_SRC = {
    "hex1.hex0": "00-hex0",
    "hex2.hex1": "01-hex1",
    "M0.hex2": "02-hex2",
    "catm.hex2": "02-hex2",
    "M1.M0": "03-m0",
    "cc.M1": "04-cc",
    "rt.M1": "04-cc",
    "M2.c": "05-m2",
    "M1.c": "06-tools",
    "H2.c": "06-tools",
}

TCC_SRC = {
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
    "tcclib.h",
    "config.h",
    "bootstrap.h",
    "tcctok.h",
    "riscv64-tok.h",
    "elf.h",
    "stab.h",
    "stab.def",
    "stddef.h",
    "stdarg.h",
    "stdio.h",
    "stdlib.h",
    "string.h",
    "errno.h",
    "math.h",
    "fcntl.h",
    "setjmp.h",
    "time.h",
    "unistd.h",
    "assert.h",
    "inttypes.h",
    "stdint.h",
    "limits.h",
    "ctype.h",
    "sys/time.h",
}


def phase_path(*parts):
    return os.path.join(BUILD, *parts)


def simon_dir():
    return phase_path("simon")


def simon_image():
    return os.path.join(STAGES, "0-simon", "artifacts", "image.bin")


def simon_files_pl():
    return phase_path("simon", "src", "files.pl")


def simon_artifacts():
    return os.path.join(STAGES, "0-simon", "artifacts")


def stage_dir(sid):
    return STAGE_DIR[sid]


def phase_image(name):
    return os.path.join(STAGES, name, "artifacts", "image.bin")


def stage_image(sid):
    return phase_image(STAGE_DIR[sid])


def artifacts(name):
    return os.path.join(STAGES, name, "artifacts")


def stem_bin(stem):
    return os.path.join(artifacts(ARTIFACT_STEM[stem]), stem + ".bin")


def stem_hex0(stem):
    return os.path.join(artifacts(ARTIFACT_STEM[stem]), stem + ".hex0")


def classify_packed(name):
    """Return (phase, 'src'|'tests') for a files.pl record."""
    if name in PACKED_TESTS:
        return PACKED_TESTS[name], "tests"
    if name in PACKED_SRC:
        return PACKED_SRC[name], "src"
    if name in TCC_SRC:
        return "07-tcc", "src"
    return "08-pnut", "src"


def unpack_files_pl(pl_path=None):
    """Copy files.pl records into build/<phase>/{src,tests}/."""
    pl_path = pl_path or simon_files_pl()
    recs = parse_pl(open(pl_path, "rb").read())
    for name, blob in recs:
        phase, kind = classify_packed(name)
        dest = os.path.join(phase_path(phase, kind), name)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        open(dest, "wb").write(blob)
    _seed_kronos()
    return len(recs)


def _seed_kronos():
    """Copy Kronos sources into build/09-kronos/{src,tests}/ (not in files.pl)."""
    kdir = os.path.join(STAGES, "09-kronos")
    dest_src = phase_path("09-kronos", "src")
    dest_tests = phase_path("09-kronos", "tests")
    src = os.path.join(kdir, "src")
    tests = os.path.join(kdir, "tests")
    if not os.path.isdir(src):
        return
    if os.path.isdir(dest_src):
        shutil.rmtree(dest_src)
    shutil.copytree(src, dest_src)
    if os.path.isdir(tests):
        if os.path.isdir(dest_tests):
            shutil.rmtree(dest_tests)
        shutil.copytree(tests, dest_tests)


def ensure_phase_dirs():
    os.makedirs(os.path.join(simon_dir(), "src"), exist_ok=True)
    os.makedirs(simon_artifacts(), exist_ok=True)
    os.makedirs(phase_path("lichee", "artifacts"), exist_ok=True)
    for name in list(STAGE_DIR.values()):
        os.makedirs(os.path.join(STAGES, name, "artifacts"), exist_ok=True)
    for name in list(STAGE_DIR.values()):
        for kind in ("src", "tests"):
            os.makedirs(phase_path(name, kind), exist_ok=True)


def main():
    args = sys.argv[1:]
    if args and args[0] == "unpack":
        ensure_phase_dirs()
        n = unpack_files_pl()
        print("unpacked %d files.pl records into build/<phase>/{src,tests}" % n)
        return
    raise SystemExit("usage: phases.py unpack")


if __name__ == "__main__":
    main()
