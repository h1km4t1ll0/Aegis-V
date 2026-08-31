# Kronos — Linux ABI kernel + TCC self-host

Mini kernel for QEMU `virt`: M-mode firmware at `0x80000000`, user U-mode, `ecall` = Linux RV64. Built by `tcc-pnut` inside Simon. Self-host is the **same** QEMU after reset, not a second machine and not `do`.

Details: [README.md](../../../README.md). Notes: [docs/kronos.md](../../../docs/kronos.md).

```bash
python3 -u src/scripts/kronos.py --tcc-self-k
```

Success: UART `Kronos`, `tcc version 0.9.26`, `cmp ok` (`/bin/tcc3` ≡ `/bin/tcc4`), then `tcc version 0.9.27` (`/bin/tcc5`), `tcc56 ok`, `hello from tcc6`.

## Sources

**Original to Aegis-V.** Not Linux, not xv6, not OpenSBI. Written so TinyCC 0.9.26 can compile itself as a Unix program (paths, headers, ELF).

**ABI borrowed (not source):** Linux RV64 syscall numbers and calling convention (`ecall` from U-mode). User ELF linked at `0x10000` because TCC 0.9.26 `lui` sign-extends bit 31.

**Local:** `src/` kernel (M-mode trap, ramfs, ELF loader, process), `src/user/libc.c` + `include/` (freestanding POSIX subset, not musl/glibc), `src/user/crt.s`, `src/user/init.c`, `tests/`.
