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

Next: [04-cc](../04-cc/README.md).
