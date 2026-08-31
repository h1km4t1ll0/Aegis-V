# 10 — TCC 0.9.27 (float)

Kronos `tcc4` (pnut-stripped 0.9.26) compiles a fuller TinyCC: vendor `tccpp.c` with `HAVE_FLOAT`, Kronos `tccelf.c` kept so ELF still loads at `0x10000`.

**In:** `tcc4` from [09-kronos](../09-kronos/README.md), vendor tree in [07-tcc](../07-tcc/README.md) `src/vendor/tcc-0.9.26/`.
**Out:** `/bin/tcc5` on Kronos. UART: `tcc version 0.9.27`.

This is ladder rung 10 (live-bootstrap: 0.9.26 builds 0.9.27). Self-host is the same Kronos run: `/bin/tcc5` → `/bin/tcc6`, `tcc56 ok`. Next is 12a (`hello from tcc6`), then gcc 4.0.4 — see [docs/2026-08-29-compiler-ladder.md](../../../docs/2026-08-29-compiler-ladder.md).

```bash
python3 -u src/scripts/kronos.py --tcc-self-k
```

Success: `Kronos`, `tcc version 0.9.26`, `cmp ok`, `tcc version 0.9.27`, `tcc56 ok`, `hello from tcc6`.

Snapshot after that UART: `artifacts/tcc6.elf` (the self-host compiler) and `artifacts/hello6.elf`.
