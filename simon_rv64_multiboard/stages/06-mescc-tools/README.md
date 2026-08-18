# 06 — mescc-tools

blood-elf, M1 и hex2 на C. Их собирает уже M2-Planet (не cc).

**Вход:** `M2.bin`.  
**Выход:** `be.bin`, `m1.bin`, `h2.bin`.  
**QEMU:** `do M2.bin -A riscv64 --bootstrap-mode -o be.M1 BE.c` (то же для `M1.c` / `H2.c`).

`BE.c` / `M1.c` / `H2.c` в образе — concat bootstrap libc из [05-m2](../05-m2/README.md) + файлы этой папки.

Дальше: [07-mes](../07-mes/README.md).
