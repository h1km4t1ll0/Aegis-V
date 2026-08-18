#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
BOARD_NAME="${1:-lichee_th1520_fullflash}"
TARGET="${2:-raw}"
make clean
if [[ "$TARGET" == "fullflash" ]]; then
  make BOARD="$BOARD_NAME" TOOLCHAIN="${TOOLCHAIN:-clang}" fullflash
else
  make BOARD="$BOARD_NAME" TOOLCHAIN="${TOOLCHAIN:-clang}"
fi
