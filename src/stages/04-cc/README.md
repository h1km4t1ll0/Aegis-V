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

Next: [05-m2](../05-m2/README.md).
