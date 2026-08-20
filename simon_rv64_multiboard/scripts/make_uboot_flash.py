#!/usr/bin/env python3
import os
import struct
import time
import zlib
from pathlib import Path


payload = Path(os.environ.get("PAYLOAD", "image_simon_lichee.bin"))
wrapped = Path(os.environ.get("WRAPPED", "simon_wrapped.img"))
flash = Path(os.environ.get("FLASH", "flash_me.bin"))
spl = Path(os.environ.get("SPL", "firmware/u-boot-with-spl.bin"))
load = int(os.environ.get("LOAD_ADDR", "0x40000000"), 0)
flash_size = int(os.environ.get("FLASH_SIZE", str(4 * 1024 * 1024)), 0)
payload_offset = int(os.environ.get("PAYLOAD_OFFSET", str(1024 * 1024)), 0)
read_addr = int(os.environ.get("READ_ADDR", "0x30000000"), 0)
name = os.environ.get("IMAGE_NAME", "SimonOS").encode("ascii", errors="ignore")

if not payload.exists():
    raise SystemExit(f"No payload: {payload}")
if not spl.exists():
    raise SystemExit(f"No SPL/U-Boot image: {spl}")

data = payload.read_bytes()
magic = 0x27051956
hcrc = 0
ts = int(time.time())
size = len(data)
ep = load
dcrc = zlib.crc32(data) & 0xffffffff
header = struct.pack(
    ">7I4B32s",
    magic,
    hcrc,
    ts,
    size,
    load,
    ep,
    dcrc,
    0x11,
    0x1A,
    0x01,
    0x00,
    name[:32].ljust(32, b"\0"),
)
hcrc = zlib.crc32(header) & 0xffffffff
header = struct.pack(
    ">7I4B32s",
    magic,
    hcrc,
    ts,
    size,
    load,
    ep,
    dcrc,
    0x11,
    0x1A,
    0x01,
    0x00,
    name[:32].ljust(32, b"\0"),
)
wrapped.write_bytes(header + data)

if flash_size <= 0 or payload_offset <= 0 or payload_offset >= flash_size:
    raise SystemExit("Invalid FLASH_SIZE/PAYLOAD_OFFSET layout")
if flash_size % 512 or payload_offset % 512:
    raise SystemExit(
        "FLASH_SIZE and PAYLOAD_OFFSET must be aligned to a 512-byte MMC block"
    )

img = bytearray(b"\0" * flash_size)
splb = spl.read_bytes()
if len(splb) > payload_offset:
    raise SystemExit(
        f"SPL/U-Boot image is too large for offset layout: {len(splb)} bytes"
    )
img[:len(splb)] = splb
wr = wrapped.read_bytes()
if len(wr) > len(img) - payload_offset:
    raise SystemExit(
        f"Wrapped payload is too large for flash image: {len(wr)} bytes"
    )
img[payload_offset : payload_offset + len(wr)] = wr
flash.write_bytes(img)
print(f"Built {flash} size {flash.stat().st_size}")
start_block = payload_offset // 512
block_count = (len(wr) + 511) // 512
print(
    f"U-Boot raw flash: mmc read 0x{read_addr:x} 0x{start_block:x} "
    f"0x{block_count:x}; bootm 0x{read_addr:x}"
)
