# 12 — gcc 4.0.4 C (RV64 integer)

Kronos `tcc6` compiles gcc 4.0.4 **C frontend only**, plus the smallest RV64 integer backend that can compile `hello.c` (`puts` + `return 0`). Asm must be TCC-legal (`ld rd, rs, imm`, no `.cfi_*`).

**In:** `/bin/tcc6` from [10-tcc](../10-tcc/README.md) (UART `hello from tcc6` first).
**Out:** `/bin/cc1` 4.0.4. UART: version containing `4.0.4`, then `hello from gcc`.

Upstream 4.0.4 has no RISC-V. live-bootstrap builds this with GNU make/`configure` for i386 — we cannot: no make, no configure, no fork. Sources: `gcc-core-4.0.4` (C only). Not this rung: gcc 4.7, libstdc++, GNU as/ld.

Subrungs (do not skip): **12a** tcc6 hello (`hello from tcc6`), **12b** gcc `version.c` (`gcc404` / `4.0.4`), **12c** libiberty `xstrdup`, **12d** `gengenrtl -h`, then genmodes / `.md` / cc1. See [docs/2026-08-29-compiler-ladder.md](../../../docs/2026-08-29-compiler-ladder.md).

Tarball (gitignored under `artifacts/`): `gcc-core-4.0.4.tar.bz2`, sha256 `e9bf58c761a4f988311aef6b41f12fd5c7e51d09477468fb73826aecc1be32e7`.

Green so far: `hello from tcc6`, `gcc404`+`4.0.4`, `xstrdup-ok`, `gengenrtl from rtl.def`, `insn-modes VOIDmode`. Snapshots in `artifacts/*.elf` and `insn-modes.h`.
