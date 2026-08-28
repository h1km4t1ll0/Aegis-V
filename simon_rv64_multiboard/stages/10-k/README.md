# 10-k — Linux ABI kernel + TCC self-host

Мини-ядро для QEMU `virt`: M-mode firmware @ `0x80000000`, user U-mode, `ecall` = Linux RV64. Производитель — `tcc-pnut` в Simon. Self-host — **второй** QEMU, не `do`.

Подробности и команды: [../../README.md](../../README.md). Спека: [../../docs/superpowers/specs/2026-08-28-linux-abi-kernel-tcc-selfhost-design.md](../../docs/superpowers/specs/2026-08-28-linux-abi-kernel-tcc-selfhost-design.md).

```bash
cd simon_rv64_multiboard
python3 -u scripts/run_k_qemu_test.py --tcc-self-k
```

Успех: UART `k 10-k`, `tcc version 0.9.26`, `cmp ok` (`/bin/tcc3` ≡ `/bin/tcc4`).
