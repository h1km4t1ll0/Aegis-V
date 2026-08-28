#!/bin/bash
# Host-build the Simon seed image. Ladder after that is inside QEMU.
# Usage: image.sh [qemu|lichee|run|clean]

set -e

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

ART=src/stages/0-simon/artifacts
BOOT_SRC=src/stages/0-simon/src/boot.S
PAYLOAD_SRC="src/stages/0-simon/src/simon.S src/stages/0-simon/src/smp.S"
FILES_HEX0=build/simon/src/files.pl
CFLAGS="-march=rv64gc -mabi=lp64d -nostdlib -ffreestanding"
TOOLCHAIN="${TOOLCHAIN:-gcc}"

usage() {
  echo "usage: $0 [qemu|lichee|run|clean]" >&2
  exit 1
}

pick_cross() {
  if [ -n "${CROSS_COMPILE:-}" ]; then
    return
  fi
  if command -v riscv64-elf-gcc >/dev/null 2>&1; then
    CROSS_COMPILE=riscv64-elf-
  elif command -v riscv64-unknown-elf-gcc >/dev/null 2>&1; then
    CROSS_COMPILE=riscv64-unknown-elf-
  else
    echo "no RISC-V gcc (tried riscv64-elf-gcc, riscv64-unknown-elf-gcc)" >&2
    echo "set CROSS_COMPILE, e.g. CROSS_COMPILE=riscv64-elf-" >&2
    exit 1
  fi
}

set_cc() {
  if [ "$TOOLCHAIN" = clang ]; then
    CC="clang --target=riscv64-unknown-elf"
    OBJCOPY=llvm-objcopy
    TEXT_FLAG="-Wl,-Ttext="
  else
    pick_cross
    CC="${CROSS_COMPILE}gcc"
    OBJCOPY="${CROSS_COMPILE}objcopy"
    TEXT_FLAG="-Ttext="
  fi
}

load_board() {
  name="$1"
  case "$name" in
    qemu) name=qemu_virt ;;
    lichee) name=lichee_th1520_fullflash ;;
  esac
  board_sh=
  for f in src/boot-targets/*/"$name".sh; do
    if [ -f "$f" ]; then
      board_sh="$f"
      break
    fi
  done
  if [ -z "$board_sh" ]; then
    echo "unknown board: $1" >&2
    usage
  fi
  # shellcheck disable=SC1090
  . "$board_sh"
}

build_image() {
  load_board "$1"
  set_cc
  if [ ! -f "$FILES_HEX0" ]; then
    echo "missing $FILES_HEX0 — run ./src/scripts/pack.sh first" >&2
    exit 1
  fi
  mkdir -p "$ART" "$(dirname "$IMAGE_OUT")"
  # shellcheck disable=SC2086
  $CC $CFLAGS -include "$BOARD_HEADER" "${TEXT_FLAG}${DDR_ADDR}" $BOOT_SRC -o "$ART/boot.elf"
  "$OBJCOPY" -O binary "$ART/boot.elf" "$ART/boot.bin"
  # shellcheck disable=SC2086
  $CC $CFLAGS -include "$BOARD_HEADER" "${TEXT_FLAG}${DDR_ADDR}" $PAYLOAD_SRC -o "$ART/simon.elf"
  "$OBJCOPY" -O binary "$ART/simon.elf" "$ART/simon.bin"
  python3 src/scripts/lib/image.py "$ART/simon.bin" "$ART/simon.hex0"
  printf '\0' >> "$ART/simon.hex0"
  cat "$ART/boot.bin" "$ART/simon.hex0" "$FILES_HEX0" > "$IMAGE_OUT"
  echo "ready: $IMAGE_OUT"
  stat -c "size: %s bytes" "$IMAGE_OUT" 2>/dev/null || stat -f "size: %z bytes" "$IMAGE_OUT"
  echo "BOARD=$BOARD_NAME"
  if [ "$SUPPORTS_FULLFLASH" = 1 ]; then
    mkdir -p "$(dirname "$FULLFLASH_BIN")" "$(dirname "$WRAPPED_BIN")"
    PAYLOAD="$IMAGE_OUT" WRAPPED="$WRAPPED_BIN" FLASH="$FULLFLASH_BIN" \
      SPL="$UBOOT_SPL_BIN" LOAD_ADDR="$DDR_ADDR" IMAGE_NAME="$UBOOT_IMAGE_NAME" \
      python3 src/scripts/lib/uboot.py
    echo "ready fullflash: $FULLFLASH_BIN"
  fi
}

do_clean() {
  rm -f "$ART/boot.elf" "$ART/boot.bin" "$ART/simon.elf" "$ART/simon.bin" \
    "$ART/simon.hex0" "$ART/image.bin"
  rm -f build/*/image.bin build/*/artifacts/*
  rm -f build/lichee/artifacts/*
  find src/stages -path '*/artifacts/*' ! -name .gitkeep -delete
}

cmd="${1:-qemu}"
case "$cmd" in
  qemu|qemu_virt|lichee|lichee_th1520_fullflash)
    build_image "$cmd"
    ;;
  run)
    build_image qemu
    exec qemu-system-riscv64 -M virt -smp 4 -m 6G -nographic \
      -bios src/stages/0-simon/artifacts/image.bin
    ;;
  clean)
    do_clean
    ;;
  *)
    usage
    ;;
esac
