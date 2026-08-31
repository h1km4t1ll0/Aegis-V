# 02 — hex2

Linker: resolves labels into machine code. It builds M0 and catm.

**In:** `hex2.bin`.
**Out:** `M0.bin`, `catm.bin`.
**QEMU:** `do hex2.bin M0.hex2 M0.bin`

| On disk | In the image |
|---|---|
| `src/M0_riscv64.hex2` | `M0.hex2` |
| `src/catm_riscv64.hex2` | `catm.hex2` — file concat |
| `tests/hello_hex2.hex2` | `testC.hex2` |

## Sources

**From:** [oriansj/stage0-posix-riscv64](https://github.com/oriansj/stage0-posix-riscv64) `M0_riscv64.hex2` and `catm_riscv64.hex2`. License: GPL-3.0.

**Modified:** ELF headers stripped for Simon raw binaries (see the comment at the top of `catm_riscv64.hex2`). Output is a flat image at `0x82000000`, not an ELF `ET_EXEC`.

**Local:** `tests/hello_hex2.hex2`.

Next: [03-m0](../03-m0/README.md).
