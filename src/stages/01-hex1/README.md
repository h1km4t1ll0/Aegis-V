# 01 — hex1

hex1 adds labels. It builds hex2.

**In:** `hex1.bin` from stage 00.
**Out:** `hex2.bin`.
**QEMU:** `do hex1.bin hex2.hex1 hex2.bin`

| On disk | In the image |
|---|---|
| `src/hex2_riscv64.hex1` | `hex2.hex1` |
| `tests/hello_hex1.hex1` | `testB.hex1` |

Next: [02-hex2](../02-hex2/README.md).
