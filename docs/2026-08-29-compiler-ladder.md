# Compiler ladder after tcc4

Date: 2026-08-29

Цель дальше (Linux с нуля, живой статус): [bootstrap-to-linux.md](bootstrap-to-linux.md).

One rung at a time. Guest compiles guest. Host does not build `cc1` or tcc5 `.o`. No GNU make, no `configure`, no fork.

## Done (do not redo)

```
hex0 → … → tcc-pnut → Kronos → tcc2 → tcc3 → tcc4 → tcc5 → tcc6
```

UART: `Kronos`, `tcc version 0.9.26`, `cmp ok` (`tcc3` ≡ `tcc4`), `tcc version 0.9.27`, `tcc56 ok` (`tcc5` ≡ `tcc6`), `hello from tcc6`.

## Rules

- Do not compile gcc until the current TCC rung is green.
- Do not skip a rung because the next one is more interesting.
- Do not invent a stub `cc1` that always emits hello.s.
- QEMU stays one process: Simon builds the kernel, reset into Kronos, `run()` the next compiler.
- After `hello from tcc6`: the next compiler sources are GCC 4.6.4 (Ekaitz RISC-V fork), compiled by the guest `tcc6`. No prebuilt `cc1`. See [bootstrap-to-linux.md](bootstrap-to-linux.md).

## Rungs

### 10 — tcc5 (fuller TCC)

**Why:** tcc4 is pnut-stripped 0.9.26 (no float). live-bootstrap: 0.9.26 builds 0.9.27 before any gcc.

**Who compiles:** `/bin/tcc4` on Kronos.

**What:** same tree as 07-tcc, but `/src/tcc-full/tccpp.c` from vendor (float/`ldexp`), `tccelf.c` stays the Kronos ELF one, `-DHAVE_FLOAT=1`.

**Driver:** `python3 -u src/scripts/kronos.py --tcc-self-k`

**Success UART (all of these):**

1. `Kronos`
2. `tcc version 0.9.26`
3. `cmp ok`
4. `tcc version 0.9.27`

**Stop.** Do not start gcc in the same run.

### 11 — tcc5 self-host

**Who compiles:** `/bin/tcc5` compiles `/src/tcc-full/tcc.c` → `/bin/tcc6`. `cmp tcc5 tcc6`.

**Success:** `cmp ok` for 5≡6 (separate from 3≡4).

**Stop.**

### 12a — tcc6 compiles a user program (before gcc)

**Who compiles:** `/bin/tcc6`. Not gcc. Proves the working compiler can emit a runnable ELF besides itself.

**What:** `/src/hello/hello.c` (`puts` + `return 0`) → `/bin/hello6`.

**Driver:** same `python3 -u src/scripts/kronos.py --tcc-self-k`

**Success UART:** `hello6 built`, then `hello from tcc6`.

**Snapshot (when green):** `src/stages/10-tcc/artifacts/tcc6.elf` and `hello6.elf` (dumped from Kronos extras after the UART line). Simon `dump_named` cannot see Kronos KFS.

**Stop.** Next compiler is GCC 4.6.4 sources, after this UART is green. See [bootstrap-to-linux.md](bootstrap-to-linux.md).

### 12 — GCC 4.6.4, RISC-V fork, from source

**Who compiles:** `/bin/tcc6` on Kronos. The host does not compile this tree.

**What:** sources of [ekaitz-zarraga/gcc](https://codeberg.org/ekaitz-zarraga/gcc), commit `c0d20242b1` (2024-06-01), GCC 4.6.4 with the RISC-V backend and musl patches. Record the commit. Do not import a built `cc1`, a Guix substitute, or a Debian package.

**Success UART:** guest-built `cc1` prints a 4.6.4 version and compiles hello.

**Not this rung:** GCC 4.0.4, GCC 4.7.4, any prebuilt compiler binary. Upstream GCC 9 comes after this `cc1` can compile C++.

## Layout

| Stage | Dir | Binary |
|---|---|---|
| 10 | `src/stages/10-tcc/` | `/bin/tcc5` |
| 11 | same tree, extra kmain step | `/bin/tcc6` |
| 12 | `src/stages/12-gcc464/` | `/bin/cc1` 4.6.4, built by tcc6 from source |

`10-gcc` and `gcc.py` are deleted. Do not restore them for rung 10 or 11. Do not restore `12-gcc404` or `13-gcc474`.
