# 06 — M1 and hex2

M1 assembler and hex2 linker, written in C. M2-Planet builds them. Needed because pnut is too large for the older M0/hex2.

**In:** `M2.bin`.
**Out:** `m1.bin`, `h2.bin`.
**QEMU:** `do M2.bin -A riscv64 --bootstrap-mode -o m1.M1 M1.c` (same for `H2.c`).

`M1.c` / `H2.c` in the image are concat of bootstrap libc from [05-m2](../05-m2/README.md) plus the files in this directory.

## Sources

**From:** [oriansj/mescc-tools](https://github.com/oriansj/mescc-tools) `M1-macro.c`, `stringify.c`, `hex2.c`, `hex2_linker.c`, `hex2_word.c`, `hex2.h`, `hex2_globals.h` (Jeremiah Orians, Jan Nieuwenhuizen). License: GPL-3.0.

**Modified:** packed as two amalgams (`M1.c`, `H2.c`) with M2-Planet `linux_bootstrap.c` / `bootstrap_libc.c` / `bootstrappable.c` prepended — no POSIX headers on Simon.

**Local glue:** `hex2_stubs.c` (`chmod`/`fflush` no-ops), `hex2_defs.c` (arch enums).

Next: [07-tcc](../07-tcc/README.md) (sources) and [08-pnut](../08-pnut/README.md).
