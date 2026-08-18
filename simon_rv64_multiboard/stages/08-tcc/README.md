# 08 — TCC 0.9.26

Patched TinyCC (janneke). `ONE_SOURCE`: `tcc.c` включает `libtcc.c` и бэкенд riscv64.

**Вход:** `mes.bin` + mescc.  
**Выход:** `tcc.s` → `tcc.bin`.  
**QEMU:** `do mes.bin -e main mescc.scm -- -S -v -o tcc.s tcc.c` затем m1/hex2.

Файлы в корне этой папки — то, что пакуется в образ.  
`vendor/tcc-0.9.26/` — полный апстрим (win32 и прочее), в образ не идёт.

На этом лестница останавливается: `tcc.bin -version` → `tcc version 0.9.26`.
