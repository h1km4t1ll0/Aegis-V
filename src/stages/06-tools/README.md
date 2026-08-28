# 06 — M1 and hex2

M1 assembler and hex2 linker, written in C. M2-Planet builds them. Needed because pnut is too large for the older M0/hex2.

**In:** `M2.bin`.
**Out:** `m1.bin`, `h2.bin`.
**QEMU:** `do M2.bin -A riscv64 --bootstrap-mode -o m1.M1 M1.c` (same for `H2.c`).

`M1.c` / `H2.c` in the image are concat of bootstrap libc from [05-m2](../05-m2/README.md) plus the files in this directory.

Next: [07-tcc](../07-tcc/README.md) (sources) and [08-pnut](../08-pnut/README.md).
