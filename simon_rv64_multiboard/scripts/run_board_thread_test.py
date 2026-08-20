#!/usr/bin/env python3
"""Run and validate Simon's cooperative multithreading test over UART."""

import argparse
import json
import re
import sys
from pathlib import Path

from run_board_nyacc_test import PROMPT, SimonSerial


RESULT_RE = re.compile(
    r"\[threadtest\] (PASS|FAIL) mode=(normal|stress) threads=(\d+) "
    r"iterations=(\d+) yield_every=(\d+) counter=(\d+) switches=(\d+) hart=(\d+)"
)


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="serial device, for example /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200, help="UART baud rate")
    parser.add_argument(
        "--timeout", type=int, default=300, help="seconds allowed for the test"
    )
    parser.add_argument(
        "--stress",
        action="store_true",
        help="run the high-load profile instead of the quick profile",
    )
    parser.add_argument(
        "--boot-timeout",
        type=int,
        default=300,
        help="seconds to wait for the initial Simon prompt",
    )
    parser.add_argument(
        "--no-kick",
        action="store_true",
        help="do not send an empty line to redraw an existing prompt",
    )
    parser.add_argument(
        "--quiet-uart",
        action="store_true",
        help="hide raw UART and show only the parsed result",
    )
    parser.add_argument(
        "--json-report", type=Path, help="write the parsed result to this JSON file"
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="print the Simon command without opening UART",
    )
    args = parser.parse_args()

    if not args.dry_run and not args.port:
        parser.error("--port is required unless --dry-run is used")
    if args.timeout <= 0 or args.boot_timeout <= 0:
        parser.error("timeouts must be positive")
    return args


def parse_result(output):
    match = RESULT_RE.search(output)
    if not match:
        raise AssertionError(
            "Simon returned without a complete threadtest result marker\n"
            f"--- UART tail ---\n{output[-4000:]}"
        )

    result = {
        "status": match.group(1).lower(),
        "profile": match.group(2),
        "threads": int(match.group(3)),
        "iterations": int(match.group(4)),
        "yield_every": int(match.group(5)),
        "counter": int(match.group(6)),
        "switches": int(match.group(7)),
        "hart": int(match.group(8)),
        "mode": "cooperative-single-hart",
    }
    expected = result["threads"] * result["iterations"]
    expected_switches = result["threads"] * (
        result["iterations"] // result["yield_every"]
    )
    result["expected_counter"] = expected
    result["expected_switches"] = expected_switches
    result["valid"] = (
        result["status"] == "pass"
        and result["counter"] == expected
        and result["switches"] == expected_switches
    )
    return result


def main():
    args = parse_args()
    command = "threadstress" if args.stress else "threadtest"
    if args.dry_run:
        print(command)
        return 0

    serial = SimonSerial(
        args.port,
        args.baud,
        timeout_scale=1.0,
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
        output = serial.command(command, args.timeout)
        result = parse_result(output)
        requested_profile = "stress" if args.stress else "normal"
        if result["profile"] != requested_profile:
            result["valid"] = False
            result["profile_error"] = (
                f"requested {requested_profile}, board reported {result['profile']}"
            )
    finally:
        serial.close()

    print(
        f"\n{result['status'].upper()} ({result['profile']}): "
        f"{result['threads']} cooperative threads, "
        f"{result['counter']}/{result['expected_counter']} atomic increments, "
        f"{result['switches']}/{result['expected_switches']} context switches, "
        f"hart {result['hart']}."
    )
    print("This validates scheduling on one hart; it is not an SMP test.")

    if args.json_report:
        args.json_report.write_text(
            json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        print(f"JSON report: {args.json_report}")

    return 0 if result["valid"] else 1


if __name__ == "__main__":
    sys.exit(main())
