#!/usr/bin/env python3
"""Run the destructive Simon DDR accessibility probe over a physical UART."""

import argparse
import json
import re
import sys
from pathlib import Path

from run_board_nyacc_test import PROMPT, SimonSerial


# TH1520 maps its DDR address window at 0x0..0x3ffffffff.  Simon itself is
# loaded at 0x40000000, which is not the DDR base.
DDR_BASE = 0x0
DEFAULT_START = 0x44000000
DEFAULT_STEP = 0x100000
ADDRESS_PATTERN = 0x9E3779B97F4A7C15

FAULT_RE = re.compile(
    r"\[memtest\] FAULT address=(0x[0-9a-f]+) "
    r"cause=(0x[0-9a-f]+) phase=([12])"
)
MISMATCH_RE = re.compile(
    r"\[memtest\] MISMATCH address=(0x[0-9a-f]+) "
    r"expected=(0x[0-9a-f]+) actual=(0x[0-9a-f]+)"
)


def address(value):
    try:
        return int(value, 0)
    except ValueError as error:
        raise argparse.ArgumentTypeError(f"invalid address {value!r}") from error


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="serial device, for example /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200, help="UART baud rate")
    parser.add_argument(
        "--start",
        type=address,
        default=DEFAULT_START,
        help=f"first address to probe (default: {DEFAULT_START:#x})",
    )
    parser.add_argument(
        "--end",
        type=address,
        required=True,
        help="exclusive end address; required to make the destructive range explicit",
    )
    parser.add_argument(
        "--step",
        type=address,
        default=DEFAULT_STEP,
        help=f"bytes between sampled 64-bit words (default: {DEFAULT_STEP:#x})",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=1800,
        help="seconds allowed for both probe passes",
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
        help="multiply the test timeout by this value",
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
        "--json-report",
        type=Path,
        help="write the parsed result to this JSON file",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="validate and print the Simon command without opening UART",
    )
    args = parser.parse_args()

    if not args.dry_run and not args.port:
        parser.error("--port is required unless --dry-run is used")
    if args.timeout <= 0 or args.boot_timeout <= 0 or args.timeout_scale <= 0:
        parser.error("timeouts and --timeout-scale must be positive")
    if args.start < DEFAULT_START:
        parser.error(
            f"--start must be >= {DEFAULT_START:#x}; lower addresses contain Simon data"
        )
    if args.end <= args.start:
        parser.error("--end must be greater than --start")
    if args.step < 8:
        parser.error("--step must be at least 8 bytes")
    if (args.start | args.end | args.step) & 7:
        parser.error("--start, --end and --step must be 8-byte aligned")
    return args


def command_for(args):
    return f"memtest {args.start:x} {args.end:x} {args.step:x}"


def parse_result(output, args):
    sample_count = (args.end - args.start + args.step - 1) // args.step
    contiguous_scan = args.start == DEFAULT_START
    base = {
        "ddr_base": DDR_BASE,
        "start": args.start,
        "end": args.end,
        "step": args.step,
        "sample_count": sample_count,
        "contiguous_scan": contiguous_scan,
    }

    fault = FAULT_RE.search(output)
    if fault:
        fault_address = int(fault.group(1), 16)
        lower_end = max(args.start, fault_address - args.step + 8)
        return {
            **base,
            "status": "fault",
            "address": fault_address,
            "cause": int(fault.group(2), 16),
            "phase": "write/read-back" if fault.group(3) == "1" else "verify",
            "ddr_boundary_bytes_min": max(0, lower_end - DDR_BASE),
            "ddr_boundary_bytes_max": max(0, fault_address - DDR_BASE),
            "tested_span_bytes": max(0, fault_address - args.start),
        }

    mismatch = MISMATCH_RE.search(output)
    if mismatch:
        mismatch_address = int(mismatch.group(1), 16)
        actual = int(mismatch.group(3), 16)
        phase = "verify" if "[memtest] write_pass_ok" in output else "write/read-back"
        alias_source = actual ^ ADDRESS_PATTERN
        alias_is_sampled = (
            args.start <= alias_source < args.end
            and (alias_source - args.start) % args.step == 0
        )
        return {
            **base,
            "status": "mismatch",
            "address": mismatch_address,
            "expected": int(mismatch.group(2), 16),
            "actual": actual,
            "phase": phase,
            "alias_source": alias_source if alias_is_sampled else None,
            "alias_period_bytes": (
                abs(alias_source - mismatch_address) if alias_is_sampled else None
            ),
            "ddr_boundary_bytes": (
                mismatch_address - DDR_BASE
                if contiguous_scan and phase == "write/read-back"
                else None
            ),
        }

    if "[memtest] PASS" in output:
        last_address = args.start + (sample_count - 1) * args.step
        return {
            **base,
            "status": "pass",
            "ddr_bytes_min": (
                last_address + 8 - DDR_BASE if contiguous_scan else None
            ),
            "tested_span_bytes": last_address + 8 - args.start,
        }

    raise AssertionError(
        "Simon returned to the prompt without a memtest result marker\n"
        f"--- UART tail ---\n{output[-4000:]}"
    )


def human_size(size):
    gib = size / (1024**3)
    return f"{size} bytes ({gib:.3f} GiB)"


def print_result(result):
    status = result["status"]
    if status == "pass":
        print(
            f"\nPASS: {result['sample_count']} sampled words were writable and "
            "readable in both passes."
        )
        print(f"Verified selected window: {human_size(result['tested_span_bytes'])}.")
        if result["contiguous_scan"]:
            print(f"DDR available: at least {human_size(result['ddr_bytes_min'])}.")
            print("The selected range ended before an inaccessible address was found.")
        else:
            print(
                "Total DDR size is unknown: this window did not start at the "
                f"safe DDR scan base {DEFAULT_START:#x}."
            )
        return

    if status == "fault":
        print(
            f"\nFAULT: inaccessible sampled address {result['address']:#x}, "
            f"cause {result['cause']:#x}, phase {result['phase']}."
        )
        minimum = result["ddr_boundary_bytes_min"]
        maximum = result["ddr_boundary_bytes_max"]
        if minimum == maximum:
            boundary = human_size(minimum)
        else:
            boundary = f"between {human_size(minimum)} and {human_size(maximum)}"

        if result["contiguous_scan"]:
            print(f"DDR available from {DDR_BASE:#x}: {boundary}.")
        else:
            print(f"Candidate DDR boundary from {DDR_BASE:#x}: {boundary}.")
            print(
                "Total DDR size is not proven by this isolated window; run a "
                f"continuous sweep from {DEFAULT_START:#x} to detect holes or aliases."
            )

        if minimum != maximum:
            print(
                f"Boundary uncertainty is {maximum - minimum} bytes; "
                "rerun around the fault with --step 0x8 for an exact boundary."
            )
        return

    print(
        f"\nMISMATCH: address {result['address']:#x}, "
        f"expected {result['expected']:#x}, actual {result['actual']:#x}, "
        f"phase {result['phase']}."
    )
    if result["alias_source"] is not None:
        print(
            f"The value came from sampled address {result['alias_source']:#x}; "
            f"probable alias period is {human_size(result['alias_period_bytes'])}."
        )
    elif result["ddr_boundary_bytes"] is not None:
        print(
            f"Candidate DDR window boundary: {result['address']:#x}, "
            f"corresponding to {human_size(result['ddr_boundary_bytes'])} from "
            f"DDR base {DDR_BASE:#x}."
        )
        print(
            "Rerun with this address as exclusive --end so the second pass can "
            "check the preceding range for aliases."
        )


def main():
    args = parse_args()
    command = command_for(args)
    print("WARNING: the selected range will be overwritten; reboot after the test.")

    if args.dry_run:
        print(command)
        return 0

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
        output = serial.command(command, args.timeout)
        result = parse_result(output, args)
    finally:
        serial.close()

    print_result(result)
    if args.json_report:
        args.json_report.write_text(
            json.dumps(result, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        print(f"JSON report: {args.json_report}")

    return 0 if result["status"] == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())
