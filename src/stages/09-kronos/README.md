# Kronos — Linux ABI kernel + TCC self-host

Mini kernel for QEMU `virt`: M-mode firmware at `0x80000000`, user U-mode, `ecall` = Linux RV64. Built by `tcc-pnut` inside Simon. Self-host is the **same** QEMU after reset, not a second machine and not `do`.

Details: [README.md](../../../README.md). Notes: [docs/kronos.md](../../../docs/kronos.md).

```bash
python3 -u src/scripts/kronos.py --tcc-self-k
```

Success: UART `Kronos`, `tcc version 0.9.26`, `cmp ok` (`/bin/tcc3` ≡ `/bin/tcc4`).
