# 03 — M0

Макроассемблер. Собирает `cc.M1` (компилятор C) в hex2.

**Вход:** `M0.bin`.  
**Выход:** `cc.hex2` → `cc.bin`.  
**QEMU:** `do M0.bin cc.M1 cc.hex2`

| На диске | В образе |
|---|---|
| `M1_riscv64.M0` | `M1.M0` |
| `hello_m0.M0` | `testD.M0` |
| `hello_m1.M1` | `testE.M1` — hello на M1 |

Дальше: [04-cc](../04-cc/README.md).
