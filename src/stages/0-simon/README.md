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

Next: [00-hex0](../00-hex0/README.md).
