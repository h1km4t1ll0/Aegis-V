# 05 — M2-Planet

A C compiler larger than cc. Sources are concatenated into one `M2.c` at pack time.

**In:** `cc.bin`.
**Out:** `M2.bin` (via M0, later M1+hex2).
**QEMU:** `do cc.bin M2.c M2.M1` then catm / M0 / hex2.

This directory holds M2-Planet `.c`/`.h` plus `linux_bootstrap.c` / `bootstrap_libc.c` — a minimal libc so the compiler can run on Simon without ELF.

## Sources

**From:** [oriansj/M2-Planet](https://github.com/oriansj/M2-Planet) (`cc.c`, `cc_*.c`, `cc.h`, `bootstrappable.c`). Bootstrap libc: same project / [M2libc](https://github.com/oriansj/M2libc) (`linux_bootstrap.c`, `bootstrap_libc.c`; Jeremiah Orians, deesix, Andrius Štikonas). License: GPL-3.0.

**Modified:** all `.c`/`.h` here are concatenated at pack time into one `M2.c` (M2-Planet has no `#include` of its own sources in this mode). Syscalls in `linux_bootstrap.c` are Linux RV64 `ecall`s matching Simon. No host `cc`; first binary is built by [04-cc](../04-cc/README.md).

Next: [06-tools](../06-tools/README.md).
