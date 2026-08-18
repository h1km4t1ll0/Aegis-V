#!/bin/bash
set -e
cd "$(dirname "$0")/.."
mkdir -p build

echo "--- Compiling ELF files ---"
riscv64-unknown-elf-gcc -mabi=lp64d -march=rv64gc -nostdlib -ffreestanding -Ttext=0x80000000 kernel/boot.S -o build/boot.elf
riscv64-unknown-elf-gcc -mabi=lp64d -march=rv64gc -nostdlib -ffreestanding -Ttext=0x80000000 kernel/simon.S -o build/simon.elf

echo "--- Converting to BIN ---"
riscv64-unknown-elf-objcopy -O binary build/boot.elf build/boot.bin
riscv64-unknown-elf-objcopy -O binary build/simon.elf build/simon.bin

echo "--- Generating hex0 ---"
python3 scripts/bin-to-hex0.sh build/simon.bin build/simon.hex0
scripts/add_null.sh build/simon.hex0

echo "--- Creating final image ---"
cat build/boot.bin build/simon.hex0 build/files.pl > build/image.bin

echo "--- Starting QEMU ---"
qemu-system-riscv64 -M virt -bios build/image.bin -monitor stdio
