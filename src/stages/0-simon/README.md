# 0 — prep: Simon seed

Host-built kernel you load before the hex0 ladder. Not a guest compiler step: gcc/clang here, then QEMU `-bios`.

- `src/boot.S` — stage 1, board load address. Reads `simon.hex0` from the image tail and jumps to Simon.
- `src/simon.S` — UART CLI, ramfs from `build/simon/src/files.pl`, `hex0` / `do`, syscalls for the ladder.
- `src/smp.S` — extra hart bring-up on QEMU virt (`HART_COUNT > 1`). Other boards link stubs.

**In:** this tree + packed `files.pl`.
**Out:** `artifacts/image.bin` (QEMU `-bios`).

```bash
./src/scripts/pack.sh
./src/scripts/image.sh qemu
```

UART and memory maps live in `src/boot-targets/`, not here.

## Sources

**From:** [ylab-nsu/ws25-bootstrap](https://github.com/ylab-nsu/ws25-bootstrap) (Team 42 winter school). Kernel idea follows [ironmeld/builder-hex0](https://github.com/ironmeld/builder-hex0) stage1/stage2 as used by [live-bootstrap](https://github.com/fosslinux/live-bootstrap), rewritten in RV64 asm. `boot.S` still says ylab-nsu; `simon.S` still says Team 42.

**Not from stage0:** the hex0 *interpreter* lives in this kernel. Guest `hex1.hex0` is a later stage.

**Modified here:**
- Board constants pulled out into `src/boot-targets/` (QEMU virt + Lichee Pi 4A).
- Flat ramfs from packed `files.pl`, CLI `hex0` / `do`, Linux RV64 `ecall`s for the ladder.
- Extra hart bring-up (`smp.S`) on QEMU virt.

Next: [00-hex0](../00-hex0/README.md).
