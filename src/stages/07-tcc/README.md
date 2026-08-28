# 07 — TCC 0.9.26

Patched TinyCC (janneke). `ONE_SOURCE`: `tcc.c` includes `libtcc.c` and the riscv64 backend.

**In:** this source tree, packed into the image. The first binary is built by [pnut](../08-pnut/README.md), not by this directory itself.
**Out:** seed `tcc-pnut.bin`, then on Kronos — `tcc2` / `tcc3` / `tcc4`.

Files at the root of this directory are what pack and Kronos KFS consume.
`vendor/tcc-0.9.26/` is full upstream and is not packed into the image.
