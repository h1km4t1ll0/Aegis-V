# 04 — cc_riscv64

Первый компилятор C. Пишет M1. Им собирают M2-Planet.

**Вход:** `cc.bin` (M0 → hex2).  
**Выход:** `M2.M1`.  
**QEMU:** `do cc.bin M2.c M2.M1`

| На диске | В образе |
|---|---|
| `cc_riscv64.M1` + `riscv64_defs.M1` | `cc.M1` |
| `riscv64_defs.M1` + `libc-core.M1` | `rt.M1` — рантайм |
| `hello_cc.c` | `testF.c` |

Дальше: [05-m2](../05-m2/README.md).
