# 07 — TCC 0.9.26

Patched TinyCC (janneke). `ONE_SOURCE`: `tcc.c` includes `libtcc.c` and the riscv64 backend.

**In:** this source tree, packed into the image. The first binary is built by [pnut](../08-pnut/README.md), not by this directory itself.
**Out:** seed `tcc-pnut.bin`, then on Kronos — `tcc2` / `tcc3` / `tcc4`.

Files under `src/` (except `vendor/`) are what pack and Kronos KFS consume.
`src/vendor/tcc-0.9.26/` is the full janneke tree and is not packed into the image.

## Sources

**Upstream TinyCC:** Fabrice Bellard, [https://bellard.org/tcc/](https://bellard.org/tcc/), [https://repo.or.cz/tinycc.git](https://repo.or.cz/tinycc.git). Version **0.9.26**. License: [LGPL-2.1](src/vendor/tcc-0.9.26/COPYING).

**Bootstrappable fork:** Jan Nieuwenhuizen (janneke), [https://gitlab.com/janneke/tinycc](https://gitlab.com/janneke/tinycc) — `BOOTSTRAP` / `HAVE_*` patches so a smaller C can compile TCC. `src/vendor/tcc-0.9.26/` is that tree.

**Modified here** (packed `src/*.c`, not the vendor copy):
- `bootstrap.h` / `config.h`: `TCC_TARGET_RISCV64`, `ONE_SOURCE`, static, no `configure`.
- pnut limits: no adjacent-string concat, no float literals, ELF struct layout workarounds (`tcc.c`, `tccpp.c`, `tccelf.c`).
- Simon: default link is raw binary at `0x82000000`; flat ramfs `-I.` (`libtcc.c`, `tccelf.c`).
- Kronos: `-Wl,-Ttext=0x10000` / `elf64-little` produces a real `ET_EXEC` (TCC 0.9.26 `lui` sign-extends bit 31).
