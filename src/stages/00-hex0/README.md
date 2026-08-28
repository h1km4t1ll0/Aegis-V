# 00 — hex0

The narrowest language: hex bytes plus comments. A human can read it by eye.

**In:** kernel `hex0` (built into [Simon](../0-simon/README.md)).
**Out in the image:** `hex1.bin`.
**QEMU:** `hex0 hex1.hex0`

| On disk | In the image |
|---|---|
| `src/hex1_riscv64.hex0` | `hex1.hex0` — hex1 source |
| `tests/hello_hex0.hex0` | `testA.hex0` — hello |

Next: [01-hex1](../01-hex1/README.md).
