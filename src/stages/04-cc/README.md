# 04 — cc_riscv64

First C compiler. Emits M1. It builds M2-Planet.

**In:** `cc.bin` (M0 → hex2).
**Out:** `M2.M1`.
**QEMU:** `do cc.bin M2.c M2.M1`

| On disk | In the image |
|---|---|
| `src/cc_riscv64.M1` + `src/riscv64_defs.M1` | `cc.M1` |
| `src/riscv64_defs.M1` + `src/libc-core.M1` | `rt.M1` — runtime |
| `tests/hello_cc.c` | `testF.c` |

## Sources

**From:** [oriansj/stage0-posix-riscv64](https://github.com/oriansj/stage0-posix-riscv64) `cc_riscv64.M1`, `riscv64_defs.M1`, `libc-core.M1` (Jeremiah Orians, Andrius Štikonas, deesix, Sanne Wouda). `riscv64_defs.M1` is also part of [M2-Planet](https://github.com/oriansj/M2-Planet) / [M2libc](https://github.com/oriansj/M2libc). License: GPL-3.0.

**Modified:** concatenated at pack time into `cc.M1` / `rt.M1`; runtime is the stage0 `libc-core` (syscalls via `ecall`), not glibc. No ELF CRT — Simon `do` loads the raw image.

**Local:** `tests/hello_cc.c`.

Next: [05-m2](../05-m2/README.md).
