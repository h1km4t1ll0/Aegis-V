# Simon RISC-V bootstrap

Лестница от hex0 до TCC 0.9.26, который **собирает сам себя** на мини-ядре с Linux RV64 ABI.

```
hex0 → hex1 → hex2 → M0 → cc → M2-Planet
     → M1 / hex2
     → pnut
     → tcc-pnut                          # первый TCC (его собрал pnut)
     → ядро 10-k                         # Linux ecall, U-mode ELF
     → tcc2 → tcc3 → tcc4                # TCC собирает TCC, без pnut
```

Параллельно (не нужен для 10-k): GNU Mes + mescc тоже умеет собрать `tcc.bin` 0.9.26 внутри Simon.

Хостовый gcc/clang трогает **только** seed-ядро Simon (`kernel/boot.S`, `kernel/simon.S`). Компиляторы лестницы и ядро 10-k собираются **внутри QEMU**.

## Карта репо

| | Каталог | Что это |
|---|---|---|
| ядро Simon | `kernel/` | boot.S, simon.S — CLI `do` / `hex0`, плоский ramfs |
| платы | `boards/` | UART, память, make-флаги |
| seed | `stages/` | исходники лестницы |
| хост | `scripts/` | pack, QEMU-тесты, прошивка |
| доки | `docs/` | платы, спека 10-k |
| сборка | `build/` | не в git: `files.pl`, образы, кэш |

Имена **внутри образа Simon** (`hex1.hex0`, `M2.c`, `pnut_tcc.c`, …) клеит `scripts/pack.sh` → `build/files.pl`.

| Стадия | Каталог | Собирает |
|---|---|---|
| boot / simon | `kernel/` | образ, CLI, ФС |
| hex0 | `stages/00-hex0/` | hex1 |
| hex1 | `stages/01-hex1/` | hex2 |
| hex2 | `stages/02-hex2/` | M0, catm |
| M0 | `stages/03-m0/` | M1 |
| cc | `stages/04-cc/` | C → M1 |
| M2-Planet | `stages/05-m2/` | C compiler |
| mescc-tools | `stages/06-mescc-tools/` | blood-elf, M1, hex2 |
| Mes | `stages/07-mes/` | Scheme + mescc (параллельный путь к TCC) |
| TCC | `stages/08-tcc/` | TinyCC 0.9.26, дерево исходников |
| pnut | `stages/09-pnut/` | pnut-exe; амальгама `pnut_tcc.c` |
| **10-k** | `stages/10-k/` | мини-ядро Linux ABI + self-host TCC |

Спека: [docs/superpowers/specs/2026-08-28-linux-abi-kernel-tcc-selfhost-design.md](docs/superpowers/specs/2026-08-28-linux-abi-kernel-tcc-selfhost-design.md).

## Две машины

**QEMU-1 — Simon.** Плоский ramfs, `do файл аргументы`, raw-бинари @ `0x82000000`. Здесь **производят** tcc-pnut и `kernel.bin`. Unix-путей и нормального ELF userland нет.

**QEMU-2 — 10-k.** Ядро M-mode с `0x80000000`, user U-mode, `ecall` = Linux RV64 (`openat` 56, `read` 63, `write` 64, `brk` 214, …). Ramfs — дерево путей (KFS1) сразу после 2 MiB pad (`0x80200000`). Здесь TCC — обычная программа.

```
хост: pack.sh + make (только Simon boot/simon.S)
        │
        ▼
QEMU-1  Simon     pnut → tcc-pnut → kernel.bin, init.elf, cmp.elf
        │
        │  хост только упаковывает: ELF-заголовок seed, KFS1, firmware
        ▼
QEMU-2  10-k      /bin/tcc → /bin/tcc2 → /bin/tcc3 → /bin/tcc4
```

На 10-k pnut **больше не вызывается**. Seed `/bin/tcc` — это tcc-pnut в ELF-обёртке. Дальше:

| Бинарь | Кто собрал | Что это |
|---|---|---|
| `/bin/tcc` | pnut (ещё в Simon) | seed, баннер `tcc version 0.9.26` |
| `/bin/tcc2` | seed из `/src/tcc/tcc.c` | первый TCC как Unix-программа |
| `/bin/tcc3` | **tcc2** | TCC собрал TCC |
| `/bin/tcc4` | **tcc3** | тот же компилятор; `cmp tcc3 tcc4` → `cmp ok` |

`tcc2 ≠ tcc3` ожидаемо: разные родители (pnut vs TCC). Фиксированная точка — `tcc3 ≡ tcc4`. Баннер настоящего TCC: `tcc version 0.9.26 (riscv64 Linux)`.

User ELF линкуется с `-Wl,-Ttext=0x10000` (TCC 0.9.26 `lui` знакорасширяет bit 31, адрес `0x81000000` ломается).

## Сборка seed-образа Simon

Нужны: `qemu-system-riscv64`, RISC-V gcc (на macOS: `riscv64-elf-gcc`). Apple clang для RV64 не подходит.

```bash
cd simon_rv64_multiboard
./scripts/pack.sh
make BOARD=qemu_virt TOOLCHAIN=gcc CROSS_COMPILE=riscv64-elf-
```

Образ: `build/boot.bin + build/simon.hex0 + build/files.pl` → `build/image_qemu.bin`.

## С нуля до pnut (QEMU-1)

Без чекпоинтов, с чистого `image_qemu.bin` (~5 мин):

```bash
python3 scripts/extract_files_pl.py pnut_tcc.c build/pnut_tcc.c
python3 -u -c "
import os, sys
sys.path.insert(0, 'scripts')
from run_m2_qemu_test import Qemu, run_pnut_tcc, ROOT
q = Qemu(qmp_sock=os.path.join(ROOT, 'build', 'qemu-from00-pnut.sock'))
try:
    run_pnut_tcc(q, from_stage=None)
finally:
    q.close()
"
```

Это: hex0 → hex1 → hex2 → M0 → cc → M2 → m1 → h2 → pnut → tcc-pnut. Успех: `tcc version 0.9.26`.

Дамп бинарей в `build/stage-cache/`. Для 10-k достаточно скопировать свежий Simon-образ:

```bash
mkdir -p build/images build/stage-cache
cp build/image_qemu.bin build/images/stage09.bin
cp build/pnut-cache/pnut.bin build/stage-cache/pnut.bin
```

`run_m2_qemu_test.py --pnut` по умолчанию стартует с чекпоинта `--from 06`, не с hex0.

Mescc-путь (часы, не нужен для 10-k):

```bash
python3 -u scripts/run_m2_qemu_test.py
```

## 10-k: ядро и self-host TCC (QEMU-2)

Нужны `build/images/stage09.bin`, `build/stage-cache/pnut.bin`, `build/pnut_tcc.c`.

```bash
python3 -u scripts/run_k_qemu_test.py --boot        # UART: k 10-k
python3 -u scripts/run_k_qemu_test.py --hi          # UART: hi
python3 -u scripts/run_k_qemu_test.py --readlen     # UART: tcc.c <размер файла>
python3 -u scripts/run_k_qemu_test.py --tcc-self-k  # seed → tcc2 → tcc3 → tcc4, cmp ok
```

`--tcc-self-k` каждый раз в QEMU-1 заново собирает tcc-pnut и ядро, затем во втором QEMU гоняет self-host. На UART должны быть `k 10-k`, `tcc version 0.9.26` и `cmp ok`.

Исходники гостевого TCC в KFS: git HEAD для `tcc.c` / `libtcc.c` / `tccgen.c` / …; `tccelf.c` — рабочее дерево (ELF-писатель seed); `config.h` — overlay `stages/10-k/user/tcc-config.h`. Не `#define float int`, не амальгама `pnut_tcc.c` на второй машине.

## Платы

[docs/README_MULTIBOARD.md](docs/README_MULTIBOARD.md), Banana: [docs/README_BANANA_PI_K1X.md](docs/README_BANANA_PI_K1X.md). Железо — после зелёного self-host на `qemu_virt`.

Истоки: [ylab](https://github.com/ylab-nsu/ws25-bootstrap), [live-bootstrap](https://github.com/fosslinux/live-bootstrap), [stage0-posix-riscv64](https://github.com/oriansj/stage0-posix-riscv64).
