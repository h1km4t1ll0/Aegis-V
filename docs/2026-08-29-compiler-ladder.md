# Compiler ladder after tcc4

Date: 2026-08-29

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

**Stop.** Do not vendor gcc until this UART is green.

### 12b — vendor gcc-core-4.0.4, print `version_string`

**Who compiles:** `/bin/tcc6`.

**What:** tarball `gcc-core-4.0.4.tar.bz2` (sha256 `e9bf58c761a4f988311aef6b41f12fd5c7e51d09477468fb73826aecc1be32e7`). Guest compiles real `gcc/version.c` (`const char version_string[] = "4.0.4"`) plus `printver.c`. This is not `cc1`.

**Success UART:** `gcc404` then `4.0.4` (the latter is `puts(version_string)`).

**Snapshot:** `src/stages/12-gcc404/artifacts/gccver.elf`

**Stop.** Do not link `cc1` yet.

### 12c — libiberty `xstrdup` (first library, not cc1)

**Who compiles:** `/bin/tcc6` via `@/src/gcc/xstrdup.rsp`.

**What:** real gcc 4.0.4 `libiberty/xmalloc.c`, `xstrdup.c`, `xexit.c` plus `include/ansidecl.h` and `include/libiberty.h`. Hand `config.h` only (no `configure`). Driver `xstrdup-main.c` calls `xstrdup`.

**Success UART:** `xstrdup-ok`

**Snapshot:** `src/stages/12-gcc404/artifacts/xstrdup.elf`

**Stop.** Do not plant all of libiberty or libcpp yet.

### 12d — `gengenrtl` (first gcc generator binary)

**Who compiles:** `/bin/tcc6` via `@/src/gcc/gengenrtl.rsp`.

**What:** real `gcc/gengenrtl.c` + `rtl.def`. Hand-written `bconfig.h` / `system.h` (only what this generator needs). Run `/bin/gengenrtl -h`.

**Success UART:** `gengenrtl from rtl.def` (from `genlegend()`).

**Snapshot:** `src/stages/12-gcc404/artifacts/gengenrtl.elf`

**Stop.** Do not link `cc1` yet.

### 12d2 — `genmodes` (insn-modes.h)

**Who compiles:** `/bin/tcc6` via `@/src/gcc/genmodes.rsp`.

**What:** real `gcc/genmodes.c` + `machmode.def` + `errors.c`. Not libiberty `hashtab.c` (tcc6 hung compiling it). Guest `mini-htab.c` + `c-alloca.c` instead. Probe `gmhdr` compiles the same headers/deps without `genmodes.c`.

**Success UART:** `gmhdr-ok`, then `insn-modes VOIDmode`, `insn-modes.c ok`, `genrtl.h ok`. Guest `genmodes -h` / `gengenrtl -h` write KFS files (stdout redirect via last non-flag argv).

**Snapshot:** `src/stages/12-gcc404/artifacts/genmodes.elf`, `insn-modes.h`

**Stop.** Do not link `cc1` yet.

### 12c — libiberty + libcpp as objects linked by tcc6

Hand-written `config.h`. tcc6 links the libraries in one invocation (`@rsp`), one extra slot for the output. No intermediate `.o` forest if extras are tight.

**Stop.**

### 12d — tiny `riscv64.md` + gen* → `insn-*.c`

Smallest integer RV64 machine description. Asm dialect must be TCC-legal (`ld rd, rs, imm`, no `.cfi_*`).

**Stop.**

### 12e — tcc6 links `/bin/cc1`

Real gcc 4.0.4 C frontend + that backend. Not a stub that always emits hello.s.

### 12 — simple gcc (4.0.4 C only)

**Who compiles:** tcc5/tcc6, not tcc-pnut, not tcc4.

**What:** gcc 4.0.4 C frontend only (live-bootstrap). Upstream 4.0.4 has no RISC-V. This rung adds the **smallest** RV64 integer backend that can compile `hello.c` (puts + return 0). Asm must be TCC-legal (`ld rd, rs, imm`, no `.cfi_*`).

**Success UART:** gcc version line containing `4.0.4`, then `hello from gcc`.

**Not this rung:** gcc 4.7, libstdc++, configure, GNU as/ld.

### 13 — gcc 4.7.4

**Who compiles:** the 4.0.4 gcc from rung 12, or tcc6 if 4.0.4 still cannot eat 4.7 sources.

**What:** gcc 4.7.4 C + a fuller RV64 backend. Last C-language gcc; later gcc is C++.

**Success:** version `4.7.4`, hello still runs.

**Out of this ladder:** gcc 7, Linux, musl-as-replacement.

## Layout

| Stage | Dir | Binary |
|---|---|---|
| 10 | `src/stages/10-tcc/` | `/bin/tcc5` |
| 11 | same tree, extra kmain step | `/bin/tcc6` |
| 12 | `src/stages/12-gcc404/` (create when 11 is green) | `/bin/cc1` 4.0.4 |
| 13 | `src/stages/13-gcc474/` (create when 12 is green) | `/bin/cc1` 4.7.4 |

`10-gcc` and `gcc.py` are deleted. Do not restore them for rung 10 or 11.
