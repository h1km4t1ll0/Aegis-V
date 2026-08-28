# 08 — pnut

pnut-exe compiles the TinyCC amalgam into the first seed compiler (`tcc-pnut`).

**In:** `m1.bin` / `h2.bin` from [06-tools](../06-tools/README.md) plus this tree.
**Out:** `pnut.bin`, then `tcc-pnut.bin`.

`vendor/pnut/` is [pnut](https://github.com/udem-dlteam/pnut) (see [SOURCE](SOURCE)). Native pnut backends are i386/x86_64 ELF only — RISC-V codegen is not used; pnut here emits the TCC binary that *does* target RV64.

`pnut_tcc.c` is the amalgam of [07-tcc](../07-tcc/README.md) plus `pnut_tcc_libc.c`, produced at pack time.

Next: [09-kronos](../09-kronos/README.md).
