# 08 — pnut

pnut-exe compiles the TinyCC amalgam into the first seed compiler (`tcc-pnut`).

**In:** `m1.bin` / `h2.bin` from [06-tools](../06-tools/README.md) plus this tree.
**Out:** `pnut.bin`, then `tcc-pnut.bin`.

`vendor/pnut/` is [pnut](https://github.com/udem-dlteam/pnut) (see [SOURCE](SOURCE)). Native pnut backends are i386/x86_64 ELF only — RISC-V codegen is not used; pnut here emits the TCC binary that *does* target RV64.

`pnut_tcc.c` is the amalgam of [07-tcc](../07-tcc/README.md) plus `pnut_tcc_libc.c`, produced at pack time.

## Sources

**From:** [udem-dlteam/pnut](https://github.com/udem-dlteam/pnut) (`pnut.c`, `exe.c`, `elf.c`, `env.c`, `x86.c`, `arm.c`). Snapshot noted in [SOURCE](SOURCE) (main, 2026-08-27). Authors: Marc Feeley, Laurent Huberdeau, Cassandre Hamel, Stefan Monnier. License: [BSD-2-Clause](https://github.com/udem-dlteam/pnut/blob/master/LICENSE).

Native pnut backends are i386/x86_64 ELF only. Those files are vendored unmodified and **not** used for RV64 codegen.

**Written here:**
- `vendor/pnut/riscv64.c` — RV64 codegen, raw binary, syscalls matching Simon.
- `pnut_tcc_libc.c`, `tcc_host_libc.c`, `pnut_host.c`, `pnut_bss.c` — libc / BSS malloc so M2-Planet can compile pnut and pnut can compile TCC.
- `tcc-inc/`, `simon_crt.s`, `tcc_crt.s` — headers and CRT for the amalgam / self-host.

**Modified:** pnut frontend comments already mention M2-Planet (ternary, `&&`). `pnut_bss.c` allocates globals at runtime because M2-Planet RV64 `%0` is not relocated.

Next: [09-kronos](../09-kronos/README.md).
