# 05 — M2-Planet

C-компилятор посерьёзнее cc. Исходники склеиваются в один `M2.c` при паке.

**Вход:** `cc.bin`.  
**Выход:** `M2.bin` (через M0 или позже M1+hex2).  
**QEMU:** `do cc.bin M2.c M2.M1` затем catm / M0 / hex2.

Здесь только `.c`/`.h` M2-Planet плюс `linux_bootstrap.c` / `bootstrap_libc.c` — минимальный libc, чтобы крутиться на simon без ELF.

Дальше: [06-mescc-tools](../06-mescc-tools/README.md).
