# 07 — GNU Mes + mescc

Scheme-интерпретатор (mes-m2) и компилятор C на Scheme (mescc + NYACC). Им собирают TCC.

**Вход:** `M2.bin` без `--bootstrap-mode`.  
**Выход:** `mes.bin`, затем `hi.s` / `tcc.s`.  
**QEMU:** `do mes.bin hello.scm` → `Hello,M2-mes!`

| На диске | В образе |
|---|---|
| `vendor/mes/` | `MES.c`, `mes_rt.M1`, `mes_as.M1`, `mes/module/`, `include/` |
| `vendor/nyacc/` | `mes/module/nyacc/` (только `.scm`/`.mes`) |
| `mescc.scm`, `hello.scm`, `probe.scm`, `hi.c`, `config.h` | те же имена |

`vendor/` — полные деревья в репо, ничего не качается.

Дальше: [08-tcc](../08-tcc/README.md).
