<div align="center">

# Aegis-V

*A readable RISC-V bootstrap: hex0 to self-hosted TinyCC on Kronos.*

[![ISA](https://img.shields.io/badge/ISA-RV64-1f6feb?style=flat-square)](https://riscv.org)
[![QEMU](https://img.shields.io/badge/board-QEMU%20virt-0969da?style=flat-square)](https://www.qemu.org)
[![Ladder](https://img.shields.io/badge/ladder-hex0%20→%20Kronos-1a7f37?style=flat-square)](#ladder)
[![TCC](https://img.shields.io/badge/TCC-0.9.26-bc4c00?style=flat-square)](https://bellard.org/tcc/)

</div>

Aegis-V is a human-checkable bootstrap on RISC-V: **hex0 → … → tcc-pnut → Kronos → tcc2 → tcc3 → tcc4 → tcc5**. The host toolchain builds only the Simon seed. Everything after that is compiled inside QEMU.

These compilers are not original to this repo. Each stage README names the upstream file, license, and what we changed. Links:

| What | Upstream | License |
|---|---|---|
| Ladder idea / hex0 kernel idea | [live-bootstrap](https://github.com/fosslinux/live-bootstrap), [ironmeld/builder-hex0](https://github.com/ironmeld/builder-hex0) | GPL |
| Simon (prep kernel) | [ylab-nsu/ws25-bootstrap](https://github.com/ylab-nsu/ws25-bootstrap) (Team 42) | ylab / Team 42 |
| hex0→hex2, M0, catm, cc | [oriansj/stage0-posix-riscv64](https://github.com/oriansj/stage0-posix-riscv64) ([stage0-posix](https://github.com/oriansj/stage0-posix), [stage0](https://github.com/oriansj/stage0)) | [GPL-3.0](https://github.com/oriansj/stage0-posix-riscv64/blob/master/LICENSE) |
| M2-Planet + bootstrap libc | [oriansj/M2-Planet](https://github.com/oriansj/M2-Planet), [M2libc](https://github.com/oriansj/M2libc) | GPL-3.0 |
| M1 / hex2 (C) | [oriansj/mescc-tools](https://github.com/oriansj/mescc-tools) | GPL-3.0 |
| TinyCC 0.9.26 | [Fabrice Bellard](https://bellard.org/tcc/) ([repo.or.cz](https://repo.or.cz/tinycc.git)) | [LGPL-2.1](src/stages/07-tcc/src/vendor/tcc-0.9.26/COPYING) |
| Bootstrappable TCC patches | [janneke/tinycc](https://gitlab.com/janneke/tinycc) | LGPL-2.1 |
| pnut-exe | [udem-dlteam/pnut](https://github.com/udem-dlteam/pnut) | [BSD-2-Clause](https://github.com/udem-dlteam/pnut/blob/master/LICENSE) |

**Written here (not a copy of the above):** Kronos, pnut `riscv64.c`, Simon ramfs/`do`, pack/QEMU drivers. Typical adaptations of upstream: strip ELF so Simon can `do` a raw image at `0x82000000`; concatenate sources for compilers that have no `#include`; libc stubs instead of musl.

After tcc4 the ladder is [docs/2026-08-29-compiler-ladder.md](docs/2026-08-29-compiler-ladder.md): tcc5 → tcc5 self-host → gcc 4.0.4 → gcc 4.7.4.

## Build (QEMU virt)

Needs `qemu-system-riscv64` and a RISC-V gcc (`riscv64-elf-gcc` on macOS Homebrew, `riscv64-unknown-elf-gcc` on Linux). Apple clang cannot target RV64.

```bash
./src/scripts/pack.sh
./src/scripts/image.sh qemu
python3 -u src/scripts/simon.py
python3 -u src/scripts/kronos.py --tcc-self-k
```

Image: `src/stages/0-simon/artifacts/image.bin`. Simon prompt: `Simon says~:`. Leave QEMU with **Ctrl+A**, then **X**. Override the compiler with `TOOLCHAIN=gcc CROSS_COMPILE=riscv64-elf-` if autodetection misses it.

`kronos.py --tcc-self-k` needs `src/stages/08-pnut/artifacts/tcc-pnut.bin` from the Simon run.

## Ladder

```
hex0 → hex1 → hex2 → M0 → cc → M2-Planet
     → m1 / h2
     → pnut
     → tcc-pnut                         # first TCC (built by pnut)
     → Kronos                           # same QEMU: reset onto the new kernel
     → tcc2 → tcc3 → tcc4               # TCC builds TCC, no pnut
     → tcc5                             # tcc4 builds fuller TCC (float / 0.9.27)
```

**Simon** is the prep seed: flat ramfs, `do file args`, raw binaries at `0x82000000`. Host gcc/clang only builds `src/stages/0-simon/src/{boot,simon,smp}.S`. You load this image before hex0 runs.

**Kronos** is a mini Linux RV64 ABI kernel (M-mode at `0x80000000`, user U-mode, `ecall` = Linux syscalls). `tcc-pnut` builds it inside Simon; the same QEMU process `system_reset`s into Kronos.

Success of `--tcc-self-k`: UART `Kronos`, `tcc version 0.9.26`, `cmp ok` (`/bin/tcc3` ≡ `/bin/tcc4`), then `tcc version 0.9.27` (`/bin/tcc5`). `tcc2 ≠ tcc3` is expected.

## Tree

| Path | What |
|---|---|
| `src/stages/0-simon/` | Prep: host-built seed, `artifacts/image.bin` (QEMU `-bios`) |
| `src/stages/00-hex0` … `10-tcc` | Ladder stages: `src/`, `tests/` (if any), `artifacts/`, `README.md` |
| `src/boot-targets/` | QEMU virt and Lichee Pi 4A headers / board vars |
| `src/scripts/` | `pack.sh`, `image.sh`, `simon.py`, `kronos.py` (`lib/` is internals) |
| `docs/` | Platforms and Kronos notes |
| `build/` | Packed `files.pl` and unpack copies (gitignored) |

Guest names inside the Simon image (`hex1.hex0`, `M2.c`, `pnut_tcc.c`, …) are wired by `src/scripts/pack.sh` → `build/simon/src/files.pl`. Each stage folder on disk is:

```text
src/stages/<phase>/
  README.md
  src/                 sources
  tests/               hello / probes (when the stage has them)
  artifacts/           bins + image.bin after a run (gitignored)
```

| Stage | Directory | Builds |
|---|---|---|
| prep / simon | `src/stages/0-simon/` | image, CLI, ramfs (load first) |
| hex0 | `src/stages/00-hex0/` | hex1 |
| hex1 | `src/stages/01-hex1/` | hex2 |
| hex2 | `src/stages/02-hex2/` | M0, catm |
| M0 | `src/stages/03-m0/` | M1 |
| cc | `src/stages/04-cc/` | C → M1 |
| M2-Planet | `src/stages/05-m2/` | C compiler |
| tools | `src/stages/06-tools/` | M1 and hex2 (assembler/linker for pnut) |
| TCC | `src/stages/07-tcc/` | TinyCC 0.9.26 sources |
| pnut | `src/stages/08-pnut/` | pnut-exe; amalgam `pnut_tcc.c` |
| **Kronos** | `src/stages/09-kronos/` | Linux ABI kernel + TCC self-host |
| TCC 0.9.27 | `src/stages/10-tcc/` | tcc4 builds fuller TCC (float) |

Hardware notes: [docs/platforms.md](docs/platforms.md). Kronos: [docs/kronos.md](docs/kronos.md). Per-stage provenance: each `src/stages/*/README.md`.
