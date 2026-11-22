#!/usr/bin/env python3
"""Run a stage 21 gate on a hash-verified guest checkpoint.

The checkpoint must be an unmodified /dev/outbox dump, with its SHA256
independently matched to the guest UART before use. No host archive is made.
Only this invocation's input is overridden; normal checkpoint inputs stay unchanged.
"""
import argparse
import hashlib
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "scripts"))
import tilck


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("checkpoint", type=Path)
    parser.add_argument("sha256", help="guest UART hash, already matched to the dump")
    parser.add_argument("--rung", choices=("linux-tools", "linux-base", "linux-build", "linux-ready"),
                        default="linux-tools")
    args = parser.parse_args()
    checkpoint = args.checkpoint.resolve()
    sha = hashlib.sha256()
    with checkpoint.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            sha.update(chunk)
    if sha.hexdigest() != args.sha256:
        parser.error("checkpoint SHA256 mismatch: " + sha.hexdigest())
    print("[%s resume] guest checkpoint SHA256 match %s" % (args.rung, args.sha256), flush=True)
    tilck.RUNGS[args.rung]["rootfs"] = str(checkpoint)
    tilck.run_rung(args.rung)


if __name__ == "__main__":
    main()
