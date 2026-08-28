#!/usr/bin/env python3
"""Drive Simon in QEMU through hex0 → M2 → m1/h2 → pnut → tcc-pnut."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lib.guest import dump_named as dump_guest_named
from lib.guest import find_ram_host
from lib.phases import artifacts, stage_image
from lib.qemu import Qemu, ROOT
from lib.checkpoint import checkpoint, parse_stage, restore_cmds


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
        checkpoint(q, "00")
        q.cmd("do hex1.bin hex2.hex1 hex2.bin", 60)
        checkpoint(q, "01")
        q.cmd("do hex2.bin M0.hex2 M0.bin", 60)
        q.cmd("do hex2.bin catm.hex2 catm.bin", 30)
        checkpoint(q, "02")
        q.cmd("do M0.bin cc.M1 cc.hex2", 180)
        q.cmd("do hex2.bin cc.hex2 cc.bin", 60)
        checkpoint(q, "03")
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
        checkpoint(q, "05")
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
        checkpoint(q, "06")

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
    pnut_out = artifacts("08-pnut")
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
    pnut_out = artifacts("08-pnut")
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
    tcc_out = artifacts("08-pnut")
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
    checkpoint(q, "08")


def parse_from_arg(argv):
    """Read --from 06 / --from=06. None if the flag is absent.

    @param argv - sys.argv
    @returns Two-digit stage id or None
    """
    if "--from" in argv:
        idx = argv.index("--from")
        if idx + 1 >= len(argv):
            raise SystemExit("--from needs a stage id (00..06)")
        return parse_stage(argv[idx + 1])
    for arg in argv:
        if arg.startswith("--from="):
            return parse_stage(arg.split("=", 1)[1])
    return None


def main():
    """hex0→pnut, or --pnut from a checkpoint (default stage 06)."""
    from_stage = parse_from_arg(sys.argv)
    if "--pnut" in sys.argv and from_stage is None:
        from_stage = "06"
    if from_stage is not None:
        image = stage_image(from_stage)
        if not os.path.isfile(image):
            raise SystemExit(
                "no %s; build checkpoints with: python3 src/scripts/lib/checkpoint.py --build"
                % image
            )
        q = Qemu(
            image=image,
            qmp_sock=os.path.join(ROOT, "build", "simon", "qemu-pnut-qmp.sock"),
        )
    else:
        q = Qemu()
    try:
        run_pnut_tcc(q, from_stage=from_stage)
    finally:
        q.close()


if __name__ == "__main__":
    main()
