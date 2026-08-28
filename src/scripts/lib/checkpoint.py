#!/usr/bin/env python3
"""Cumulative Simon images: stage N includes artifacts of stages 0..N.

  python3 src/scripts/lib/checkpoint.py --build
  python3 -u src/scripts/simon.py --from 06
"""
import os
import struct
import sys

_SCRIPTS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _SCRIPTS not in sys.path:
    sys.path.insert(0, _SCRIPTS)
from lib.guest import (
    FD_BASE,
    FD_BYTES,
    FILE_NAME_BASE,
    FILE_NAME_BYTES,
    FILE_NAME_SLOT,
    FILE_NAME_COUNT,
    guest_to_host,
    lldb_cmds,
    find_ram_host,
)
from lib.image import bytes_to_hex0, parse_pl, serialize_pl
from lib.phases import (
    STAGE_DIR,
    artifacts,
    ensure_phase_dirs,
    simon_artifacts,
    simon_files_pl,
    stage_image,
    stem_bin,
    stem_hex0,
)
from lib.qemu import Qemu, Qmp, ROOT

FILES_PL = simon_files_pl()
BOOT = os.path.join(simon_artifacts(), "boot.bin")
SIMON_HEX0 = os.path.join(simon_artifacts(), "simon.hex0")
DUMP_META = os.path.join(ROOT, "build", "simon", "dump")

# Stage k is done when these stems exist as .bin in ramfs.
# --from k restores all stems with index <= k, then continues with k+1.
STAGE_STEMS = (
    ("00", ("hex1",)),
    ("01", ("hex2",)),
    ("02", ("M0", "catm")),
    ("03", ("cc",)),
    ("04", ()),  # M2.M1 is text; checkpoint that matters is 05/M2.bin
    ("05", ("M2",)),
    ("06", ("m1", "h2")),
    ("07", ()),  # TCC sources only; image matches stage 06
    ("08", ("pnut", "tcc-pnut")),
)


def stems_through(stage):
    """Stems that belong on a checkpoint image for this stage id.

    @param stage - Two-digit stage id, e.g. '07'
    @returns Tuple of stem names
    """
    out = []
    for sid, stems in STAGE_STEMS:
        out.extend(stems)
        if sid == stage:
            return tuple(out)
    raise SystemExit("unknown stage %s" % stage)


def parse_stage(raw):
    """Normalize '6' / '06' / '6-tools' to '06'.

    @param raw - User token
    @returns Two-digit stage id
    """
    token = raw.strip().split("-", 1)[0]
    return "%02d" % int(token)



def upsert_hex0(recs, stem, blob):
    """Replace or append stem.hex0 with a hex0 encoding of blob.

    @param recs - Mutable record list
    @param stem - File stem (hex1, m1, …)
    @param blob - Raw binary
    @returns None
    """
    name = stem + ".hex0"
    encoded = bytes_to_hex0(blob).encode("ascii")
    for i, (old, _) in enumerate(recs):
        if old == name:
            recs[i] = (name, encoded)
            return
    recs.append((name, encoded))


def dump_qmp(qmp_sock):
    """Copy known stage binaries out of a live QEMU into src/stages/<phase>/artifacts/.

    @param qmp_sock - UNIX QMP path of THIS qemu (never the default shared one)
    @returns Dict of stem -> byte length
    """
    ensure_phase_dirs()
    os.makedirs(DUMP_META, exist_ok=True)
    tmp = "/tmp/simon-stage-dump"
    os.makedirs(tmp, exist_ok=True)
    qmp = Qmp(qmp_sock)
    found = {}
    try:
        names_path = os.path.join(tmp, "_names.bin")
        fds_path = os.path.join(tmp, "_fds.bin")
        for path in (names_path, fds_path):
            try:
                os.unlink(path)
            except OSError:
                pass
        qmp.pmemsave(FILE_NAME_BASE, FILE_NAME_BYTES, names_path)
        qmp.pmemsave(FD_BASE, FD_BYTES, fds_path)
        if not os.path.isfile(names_path):
            raise RuntimeError("pmemsave wrote no %s" % names_path)
        names = open(names_path, "rb").read()
        fds = open(fds_path, "rb").read()
        open(os.path.join(DUMP_META, "_names.bin"), "wb").write(names)
        open(os.path.join(DUMP_META, "_fds.bin"), "wb").write(fds)
        wanted = {}
        for _, stems in STAGE_STEMS:
            for stem in stems:
                wanted[stem + ".bin"] = stem
        present = []
        for index in range(FILE_NAME_BYTES // FILE_NAME_SLOT):
            slot = names[index * FILE_NAME_SLOT : (index + 1) * FILE_NAME_SLOT]
            name = slot.split(b"\0", 1)[0].decode("ascii", "replace")
            if not name:
                continue
            present.append(name)
            if name not in wanted:
                continue
            addr, length = struct.unpack_from("<QQ", fds, index * 16)
            if length == 0 or length > 0x4000000:
                print("[stage] skip %s len=%d" % (name, length), flush=True)
                continue
            stem = wanted[name]
            raw_path = os.path.join(tmp, stem + ".bin")
            try:
                os.unlink(raw_path)
            except OSError:
                pass
            qmp.pmemsave(addr, length, raw_path)
            data = open(raw_path, "rb").read()
            dest = stem_bin(stem)
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            open(dest, "wb").write(data)
            open(stem_hex0(stem), "w", encoding="ascii").write(
                bytes_to_hex0(data)
            )
            found[stem] = length
            print("[stage] %s: %d bytes" % (name, length), flush=True)
        if not found:
            print("[stage] ramfs names: %s" % ",".join(present[:24]), flush=True)
    finally:
        qmp.close()
    open(os.path.join(DUMP_META, "MANIFEST.txt"), "w", encoding="ascii").write(
        "\n".join("%s %d" % item for item in sorted(found.items())) + "\n"
    )
    return found


def assemble():
    """Write build/<phase>/image.bin for every stage whose stems are cached.

    @returns List of stage ids written
    """
    if not os.path.isfile(FILES_PL) or not os.path.isfile(BOOT):
        raise SystemExit(
            "need build/simon/src/files.pl and src/stages/0-simon/artifacts/boot.bin (pack + image.sh)"
        )
    ensure_phase_dirs()
    base = parse_pl(open(FILES_PL, "rb").read())
    boot = open(BOOT, "rb").read()
    hex0 = open(SIMON_HEX0, "rb").read()
    written = []
    for sid, _stems in STAGE_STEMS:
        stems = stems_through(sid)
        if not stems:
            continue
        missing = [s for s in stems if not os.path.isfile(stem_bin(s))]
        if missing:
            continue
        recs = list(base)
        for stem in stems:
            blob = open(stem_bin(stem), "rb").read()
            upsert_hex0(recs, stem, blob)
        pl = serialize_pl(recs)
        phase = STAGE_DIR[sid]
        pl_path = os.path.join(artifacts(phase), "files.pl")
        img_path = stage_image(sid)
        open(pl_path, "wb").write(pl)
        open(img_path, "wb").write(boot + hex0 + pl)
        print(
            "[stage] %s  %d bytes  stems=%s" % (img_path, os.path.getsize(img_path), ",".join(stems)),
            flush=True,
        )
        written.append(sid)
    return written


def dump_pid(pid):
    """Copy known stage binaries from THIS qemu pid via lldb.

    @param pid - qemu-system-riscv64 pid we started
    @returns Dict of stem -> byte length
    """
    ensure_phase_dirs()
    os.makedirs(DUMP_META, exist_ok=True)
    ram_host = find_ram_host(pid)
    names_path = os.path.join(DUMP_META, "_names.bin")
    fds_path = os.path.join(DUMP_META, "_fds.bin")
    lldb_cmds(pid, [
        "memory read --outfile %s --binary --force --count %d %s" % (
            names_path, FILE_NAME_COUNT * FILE_NAME_SLOT,
            hex(guest_to_host(FILE_NAME_BASE, ram_host))),
        "memory read --outfile %s --binary --force --count %d %s" % (
            fds_path, FILE_NAME_COUNT * 16,
            hex(guest_to_host(FD_BASE, ram_host))),
    ])
    names = open(names_path, "rb").read()
    fds = open(fds_path, "rb").read()
    wanted = {}
    for _, stems in STAGE_STEMS:
        for stem in stems:
            wanted[stem + ".bin"] = stem
    by_name = {}
    present = []
    for i in range(FILE_NAME_COUNT):
        raw = names[i * FILE_NAME_SLOT:(i + 1) * FILE_NAME_SLOT].split(b"\0", 1)[0]
        if not raw:
            continue
        name = raw.decode("ascii", "replace")
        present.append(name)
        addr, length = struct.unpack_from("<QQ", fds, i * 16)
        by_name[name] = (addr, length)
    found = {}
    cmds = []
    pending = []
    for name, stem in wanted.items():
        if name not in by_name:
            continue
        addr, length = by_name[name]
        if length == 0 or length > 0x4000000:
            print("[stage] skip %s len=%d" % (name, length), flush=True)
            continue
        dest = stem_bin(stem)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        cmds.append(
            "memory read --outfile %s --binary --force --count %d %s" % (
                dest, length, hex(guest_to_host(addr, ram_host)))
        )
        pending.append((stem, name, dest, length))
    if cmds:
        lldb_cmds(pid, cmds, timeout=300)
    for stem, name, dest, length in pending:
        data = open(dest, "rb").read()
        open(stem_hex0(stem), "w", encoding="ascii").write(
            bytes_to_hex0(data)
        )
        found[stem] = length
        print("[stage] %s: %d bytes" % (name, length), flush=True)
    if not found:
        print("[stage] ramfs names: %s" % ",".join(present[:24]), flush=True)
    open(os.path.join(DUMP_META, "MANIFEST.txt"), "w", encoding="ascii").write(
        "\n".join("%s %d" % item for item in sorted(found.items())) + "\n"
    )
    return found


def checkpoint(q, sid):
    """Dump ramfs bins and write every stage image whose stems are complete.

    @param q - Live Qemu session we started
    @param sid - Stage id just finished
    @returns None
    """
    found = dump_pid(q.proc.pid)
    written = assemble()
    print("[stage] checkpoint %s dumped=%s images=%s" % (
        sid,
        ",".join(sorted(found)),
        ",".join(written) or "-",
    ), flush=True)


def build():
    """Run hex0→m1/h2 in a private QEMU and snapshot after every stage."""
    sock = os.path.join(ROOT, "build", "simon", "qemu-stage-build.sock")
    q = Qemu(memory="2G", smp="1", qmp_sock=sock)
    print("[stage] qemu pid=%s qmp=%s" % (q.proc.pid, sock), flush=True)
    try:
        q.wait_prompt(30, "boot")
        q.drain_quiet()
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
        q.cmd("do M2.bin -A riscv64 --bootstrap-mode -o m1.M1 M1.c", 300)
        q.cmd("do catm.bin m12.M1 rt.M1 m1.M1", 30)
        q.cmd("do M0.bin m12.M1 m1.hex2", 180)
        q.cmd("do hex2.bin m1.hex2 m1.bin", 60)
        q.cmd("do M2.bin -A riscv64 --bootstrap-mode -o h2.M1 H2.c", 300)
        q.cmd("do catm.bin h22.M1 rt.M1 h2.M1", 30)
        q.cmd("do M0.bin h22.M1 h2.hex2", 180)
        q.cmd("do hex2.bin h2.hex2 h2.bin", 60)
        checkpoint(q, "06")
        print("[stage] images ready under src/stages/<phase>/artifacts/", flush=True)
    finally:
        q.close()
        try:
            os.unlink(sock)
        except OSError:
            pass


def restore_cmds(stage):
    """Guest commands that materialize cached .bin files.

    @param stage - Two-digit stage id
    @returns List of 'hex0 stem.hex0' strings
    """
    return ["hex0 %s.hex0" % stem for stem in stems_through(stage)]


def main():
    args = sys.argv[1:]
    if not args or args[0] in ("-h", "--help"):
        print(__doc__.strip())
        return
    if args[0] == "--dump-qmp":
        if len(args) < 2:
            raise SystemExit("usage: checkpoint.py --dump-qmp SOCK")
        found = dump_qmp(args[1])
        print("[stage] dumped %d binaries" % len(found), flush=True)
        assemble()
        return
    if args[0] == "--assemble":
        assemble()
        return
    if args[0] == "--build":
        build()
        return
    raise SystemExit(
        "usage: checkpoint.py --build | --dump-qmp SOCK | --assemble"
    )


if __name__ == "__main__":
    main()
