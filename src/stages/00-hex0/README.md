# 00 — hex0

The narrowest language: hex bytes plus comments. A human can read it by eye.

**In:** kernel `hex0` (built into [Simon](../0-simon/README.md)).
**Out in the image:** `hex1.bin`.
**QEMU:** `hex0 hex1.hex0`

| On disk | In the image |
|---|---|
| `src/hex1_riscv64.hex0` | `hex1.hex0` — hex1 source |
| `tests/hello_hex0.hex0` | `testA.hex0` — hello |

## Sources

**From:** [oriansj/stage0-posix-riscv64](https://github.com/oriansj/stage0-posix-riscv64) `hex1_riscv64.hex0` (Jeremiah Orians, Andrius Štikonas, Gabriel Wicki). License: [GPL-3.0](https://github.com/oriansj/stage0-posix-riscv64/blob/master/LICENSE). Same ladder as [stage0-posix](https://github.com/oriansj/stage0-posix).

The hex0 *binary* is not this tree — it is built into [Simon](../0-simon/README.md). This directory is only hex1 source plus a probe.

**Modified:** run as a Simon raw binary (no ELF loader, Linux `ecall`s already in the stage0 port).

**Local:** `tests/hello_hex0.hex0`.

Next: [01-hex1](../01-hex1/README.md).
