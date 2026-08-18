# Simon RISC-V bootstrap

Лестница от hex0 до TCC 0.9.26.

```
hex0 → hex1 → hex2 → M0 → cc → M2-Planet
     → blood-elf / M1 / hex2
     → GNU Mes + mescc
     → TCC 0.9.26
```

| | Каталог | Что это |
|---|---|---|
| ядро | `kernel/` | boot.S, simon.S |
| платы | `boards/` | UART, память, make-флаги |
| seed | `stages/` | исходники лестницы |
| хост | `scripts/` | pack, QEMU-тест, hex0, прошивка |
| доки | `docs/` | платы, языки |
| U-Boot SPL | `firmware/` | входной блоб для fullflash |
| сборка | `build/` | `files.pl`, `.elf/.bin`, образы |

Имена **внутри образа** (`hex1.hex0`, `M2.c`, `tcc.c`, …) не совпадают с путями на диске. Их клеит `scripts/pack.sh` → `build/files.pl`.

| Стадия | Каталог | Собирает | README |
|---|---|---|---|
| boot / simon | `kernel/` | образ, CLI, ФС | этот файл |
| hex0 | `stages/00-hex0/` | hex1 | [00](stages/00-hex0/README.md) |
| hex1 | `stages/01-hex1/` | hex2 | [01](stages/01-hex1/README.md) |
| hex2 | `stages/02-hex2/` | M0, catm | [02](stages/02-hex2/README.md) |
| M0 | `stages/03-m0/` | M1 | [03](stages/03-m0/README.md) |
| cc | `stages/04-cc/` | C → M1 | [04](stages/04-cc/README.md) |
| M2-Planet | `stages/05-m2/` | C compiler | [05](stages/05-m2/README.md) |
| mescc-tools | `stages/06-mescc-tools/` | blood-elf, M1, hex2 | [06](stages/06-mescc-tools/README.md) |
| Mes | `stages/07-mes/` | Scheme + mescc | [07](stages/07-mes/README.md) |
| TCC | `stages/08-tcc/` | tcc 0.9.26 | [08](stages/08-tcc/README.md) |

Платы: [docs/README_MULTIBOARD.md](docs/README_MULTIBOARD.md), Banana: [docs/README_BANANA_PI_K1X.md](docs/README_BANANA_PI_K1X.md). Языки (x86-справка): [docs/parts.rst](docs/parts.rst).

## Сборка и QEMU

```bash
./scripts/pack.sh
make BOARD=qemu_virt TOOLCHAIN=gcc CROSS_COMPILE=riscv64-elf- image_qemu.bin
python3 -u scripts/run_m2_qemu_test.py
```

Образ: `build/boot.bin + build/simon.hex0 + build/files.pl` → `build/image_qemu.bin`.

Истоки: [ylab](https://github.com/ylab-nsu/ws25-bootstrap), [live-bootstrap](https://github.com/fosslinux/live-bootstrap), [stage0-posix-riscv64](https://github.com/oriansj/stage0-posix-riscv64).
