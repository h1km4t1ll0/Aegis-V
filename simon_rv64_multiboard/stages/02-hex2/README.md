# 02 — hex2

Линкер: резолвит метки в машинный код. Им собирают M0 и catm.

**Вход:** `hex2.bin`.  
**Выход:** `M0.bin`, `catm.bin`.  
**QEMU:** `do hex2.bin M0.hex2 M0.bin`

| На диске | В образе |
|---|---|
| `M0_riscv64.hex2` | `M0.hex2` |
| `catm_riscv64.hex2` | `catm.hex2` — склейка файлов |
| `hello_hex2.hex2` | `testC.hex2` |

Дальше: [03-m0](../03-m0/README.md).
