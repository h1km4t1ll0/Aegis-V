# 03 — M0

Macro assembler. Builds `cc.M1` (the C compiler) into hex2.

**In:** `M0.bin`.
**Out:** `cc.hex2` → `cc.bin`.
**QEMU:** `do M0.bin cc.M1 cc.hex2`

| On disk | In the image |
|---|---|
| `src/M1_riscv64.M0` | `M1.M0` |
| `tests/hello_m0.M0` | `testD.M0` |
| `tests/hello_m1.M1` | `testE.M1` — hello in M1 |

## Sources

**M0 (used on the ladder):** [oriansj/stage0-posix-riscv64](https://github.com/oriansj/stage0-posix-riscv64) `M0_riscv64.hex2`. License: GPL-3.0. Same ELF-strip / raw-`0x82000000` adaptation as [02-hex2](../02-hex2/README.md).

**`M1_riscv64.M0`:** written here in M0. Opcode `DEFINE`s follow stage0-posix-riscv64 / [M2libc](https://github.com/oriansj/M2libc) `riscv64_defs.M1`. The C M1 that actually assembles pnut is [06-tools](../06-tools/README.md).

**Local:** `tests/hello_m0.M0`, `tests/hello_m1.M1`.

Next: [04-cc](../04-cc/README.md).
