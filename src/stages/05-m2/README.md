# 05 — M2-Planet

A C compiler larger than cc. Sources are concatenated into one `M2.c` at pack time.

**In:** `cc.bin`.
**Out:** `M2.bin` (via M0, later M1+hex2).
**QEMU:** `do cc.bin M2.c M2.M1` then catm / M0 / hex2.

This directory holds M2-Planet `.c`/`.h` plus `linux_bootstrap.c` / `bootstrap_libc.c` — a minimal libc so the compiler can run on Simon without ELF.

Next: [06-tools](../06-tools/README.md).
