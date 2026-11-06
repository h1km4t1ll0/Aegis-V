# Утилиты для configure, binutils, gmp/mpfr/mpc — поэтапный план

> **Исполнитель:** одна фаза целиком вместе с гейтом. Ревью по фазам нет, в конце один отчёт.
> **Коммиты** — только после явного «да» пользователя.
> **Всё проверяется на QEMU.**

**Цель:** под Tilck есть всё, что нужно `configure && make` GCC 4.6.4:
- grep, gawk, diffutils, m4, flex, findutils, недостающие программы coreutils;
- binutils 2.30 (`as`, `ld`, `ar`, `nm`, `ranlib`, `objdump`, `strip`);
- gmp, mpfr, mpc с зелёным собственным `make check`.

Всё собрано гостем под Tilck.

**Архитектура:** ступени Tilck, как в плане 3 (`tilck.py --rung NAME`, `rung-init` + kaem, `rootfs.tar` по цепочке через `/dev/outbox`). С этого плана сборки идут через **bash и `configure`**: `run.kaem` делает `/bin/sh` → `/usr/bin/bash` и запускает `bash steps/run.sh`. Рецепты live-bootstrap (`b1ceced7`) используем там, где они есть: их `mk/` и патчи. Шаги «пересоздать сгенерированные файлы» (удаление `*.c` от bison/flex, `autoreconf`) пропускаем: по дизайну сгенерированные файлы из релизных тарболлов пока принимаются как исходник.

**Стек:** QEMU `virt` 8G. Факты — README стадий 12, 14, 15. Прошлые планы выполнены.

| Фаза | Имя | Гейт |
|------|-----|------|
| 1 | Tilck для долгих сборок | тесты `fdmax ok`, `chown ok`, `rootperm ok`, `fatseek ok`, `xbit ok`; пять прогонов `tools2` подряд зелёные; стек ядра увеличен |
| 2 | Ступень `build1` | grep, gawk, diffutils, m4, flex, findutils, coreutils-extra работают; настоящий `./configure && make` тарболла grep-2.4 собирает работающий grep → `configure ok` |
| 3 | Ступень `binutils` | `binutils 2.30` установлены; `as` + `ld` + crt musl → hello печатает `binutils hello ok`; `ar`/`nm`/`ranlib` на `libc.a` |
| 4 | Ступень `gmp` | `make check` gmp, mpfr, mpc зелёный (`gmp check ok`, `mpfr check ok`, `mpc check ok`) |

## Allowed paths

| Путь | Фазы |
|------|------|
| `src/stages/12-tilck/**` | 1 (и дальше, если блокирует ошибка Tilck) |
| `src/stages/14-seed/**` (`rung-init.c`, `mkustar` в 13-musl) | 1 |
| `src/stages/13-musl/src/mkustar.c` | 1 |
| `src/stages/15-early-tools/**` | 1 (перезапуск ступеней после пересборки) |
| `src/stages/16-buildtools/**` (новая) | 2 |
| `src/stages/17-binutils/**` (новая) | 3 |
| `src/stages/18-gmp-mpfr-mpc/**` (новая) | 4 |
| `src/stages/11-tcc7/src/override/*`, `tests/*`, `README.md` | 1–4 (только если блокирует ошибка tcc; тест сначала; после — пересборка семени и ступеней) |
| `src/stages/09-kronos/src/kmain.c` (CRLF), `src/scripts/kronos.py` | 1–4 (только провязка сборки семени и Tilck) |
| `src/scripts/tilck.py`, `src/scripts/lib/phases.py` (`STAGE_DIR`) | 1–4 |
| `build/sources/` (gitignore) | 2–4 |
| `docs/2026-10-28-gcc464-ladder-design.md` (лестница и журнал) | 4 |

Не трогать: `07-tcc`, `10-tcc`, libc и прочие файлы Kronos, `lib/qemu.py`, README в корне.

## Вне плана
- GCC 4.6.4 — план 5.
- Пересоздание `configure` и парсеров (autoconf, bison, flex 2.5.11) — отдельная ступень потом.

---

## Фаза 1: Tilck для долгих сборок

Причины из плана 3: редкое зависание (петля ловушек `scause 0xf` на входе ловушки, мусорный `sscratch` — похоже на переполнение стека ядра), 16 дескрипторов на процесс, `chown(path,-1,-1)` → `EPERM`, ramfs применяет права владельца к root, `lseek(SEEK_END)` на FAT32 initrd, исполняемые скрипты теряют `+x` между ступенями, нет `/bin/sh`.

### Задача 1. Тесты (сначала падают)
`src/stages/12-tilck/tests/hard.c` (musl), запуск из `proc`, каждая проверка печатает свою строку:
- `fdmax ok` — открыть 200 файлов одновременно;
- `chown ok` — `chown(f, -1, -1) == 0` и `chown(f, 0, 0) == 0`;
- `rootperm ok` — root открывает на запись файл с правами 0444 и создаёт файл в каталоге 0555 (как Linux с `CAP_DAC_OVERRIDE`);
- `fatseek ok` — `lseek(fd, 0, SEEK_END)` на файле из `/initrd` возвращает его размер;
- `deepstack ok` — рекурсия 4 МБ в пользовательском стеке плюс глубокие вызовы ядра: 50 вложенных каталогов, `getcwd`, `fork` внутри глубокой рекурсии.

### Задача 2. Исправления Tilck (overrides)
- `KERNEL_STACK_PAGES` и стек ловушек — увеличить (с 4 страниц до 16) и найти причину зависания. Запустить ступень `tools2` пять раз подряд. Если зависание повторяется, пройти отладчиком QEMU (`-s`) до места переполнения.
- `KRN_MAX_HANDLES` → 256.
- `chown` для root: `-1` значит «не менять»; смена владельца у root разрешена.
- ramfs: для uid 0 права чтения и записи не проверяются. Исполнение — только если стоит хоть один бит `x`, как в Linux.
- `kernel/fs/fat32.c`: `SEEK_END` = размер файла + смещение.
- `access()`: настоящая проверка по правам (сейчас всегда 0). Для root — как в Linux: `R_OK`/`W_OK` всегда, `X_OK` — если стоит хоть один бит `x`.

### Задача 3. Флаги исполнения и `/bin/sh`
- `mkustar` записывает настоящий `mode` из `stat`, а `rung-init` восстанавливает `mode` из заголовка tar (вместо правила «+x только для ELF»).
- `rung-init` после распаковки создаёт `/bin` и символьную ссылку `/bin/sh` → `/usr/bin/bash`, если `bash` есть. Если нет — ссылку не создаёт.
- Пересборка: `kronos.py --tcc-self-k`, потом ступени `tcc`, `tools1`, `tools2` по очереди. sha256 их `rootfs` могут измениться из-за `mode` — это ожидаемо, записать новые.

### Задача 4. Гейт фазы 1
`tilck.py --test` (все прошлые метки плюс пять новых); `--rung tcc`, `--rung tools1`, затем `--rung tools2` пять раз подряд — все зелёные; в `rootfs-tools2.tar` у `usr/bin/yyfix` стоит `+x`.

---

## Фаза 2: Ступень `build1`

### Задача 5. Состав и рецепты
Тарболлы и sha256 — из файлов `sources` live-bootstrap: `grep-2.4`, `gawk-3.0.4`, `diffutils-2.7`, `m4-1.4.7`, `flex-2.6.4`, `findutils-4.2.33`.
- **grep, gawk, diffutils, m4** — `mk/` live-bootstrap и `make -f`. Сгенерированные файлы (`awktab.c` и подобные) удалять и пересоздавать нашим `yacc`, если он справляется. Иначе оставить из тарболла и записать в README.
- **flex 2.6.4** — `mk/` live-bootstrap, но `scan.c`, `parse.c`, `parse.h`, `skel.c` из тарболла не удалять (до flex 2.5.11 и heirloom lex мы пока не идём; решение записать).
- **findutils 4.2.33** — настоящий `./configure --prefix=/usr` из тарболла, без `autoreconf` и gnulib-импорта live-bootstrap. Это первая проверка `configure`.
- **coreutils-extra** — недостающие программы из того же дерева coreutils 5.0 (`uname`, `env`, `date`, `mktemp`, `chown`, `dd`, `du`, `sleep`, `nice`, `printf`, `basename`, `dirname`, `pwd`, `true`, `false`, `ln`, `touch`, `rmdir`, `cmp` — что из этого ещё нет в `usr/bin`). Через Makefile из `mk/` live-bootstrap, дополненный этими целями.
- В `run.sh` перед сборками: `export SHELL=/bin/sh CONFIG_SHELL=/bin/sh`.

### Задача 6. Тесты и гейт
- По строке на утилиту: `grep -c`, `awk '{s+=$1} END {print s}'`, `cmp`/`diff -u` на двух файлах, `m4` с `define`, `flex` на маленьком `.l` → tcc → запуск, `find . -name '*.c' | xargs wc -l`, `uname -m` = `riscv64` → строка `build1 tools ok`.
- **Настоящий configure.** Распаковать `grep-2.4.tar.gz` заново в `/build/cfgtest`, `./configure --prefix=/tmp/x CC=tcc && make && make install`, запустить установленный `grep` → `configure ok`.
- `tilck.py --rung build1`: `build1 tools ok`, `configure ok`, `rootfs sha256 match`, `rung done build1`.

---

## Фаза 3: Ступень `binutils`

### Задача 7. Сборка
- `binutils-2.30.tar.bz2` — уже в `build/sources/` (sha256 `efeade84…0c09`).
- `./configure --prefix=/usr --build=riscv64-unknown-linux-musl --host=riscv64-unknown-linux-musl --target=riscv64-unknown-linux-musl --disable-nls --disable-werror --disable-shared --enable-static --disable-gold --disable-plugins --enable-deterministic-archives CC="tcc -static" AR="tcc -ar" RANLIB=true` (`true` — если до установки нашего `ranlib`).
- Отклонения (патчи под tcc и musl) — по строке в README стадии 17. Шаги live-bootstrap «пересоздать bison/flex/opcodes» пропускаются и записываются в README.
- `make` с `MAKEINFO=true` (документацию не строим), `make install`.

### Задача 8. Тесты и гейт
- `as --version`, `ld --version`, `ar`, `nm`, `ranlib`, `objdump`, `strip`, `readelf` — версии 2.30.
- `hello.s` (написан руками, системные вызовы `write`/`exit`) → `as` → `ld -static` → запуск → `binutils asm ok`.
- `tcc -c hello.c` → `ld` с `crt1.o crti.o hello.o libc.a libtcc1.a crtn.o` → запуск → `binutils hello ok`.
- `nm /usr/lib/libc.a | grep -c ' T '` > 1000 и `ar t` → `binutils ar ok`.
- `tilck.py --rung binutils`: эти метки плюс `rootfs sha256 match`, `rung done binutils`.

---

## Фаза 4: Ступень `gmp`

### Задача 9. Версии и пины
Версии — из `contrib/download_prerequisites` дерева GCC 4.6.4: **gmp 4.3.2, mpfr 2.4.2, mpc 0.8.1**. Тарболлы скачать с `https://gcc.gnu.org/pub/gcc/infrastructure/` и сверить sha256 со вторым независимым источником: `ftp.gnu.org/gnu/gmp` для gmp, `mpfr.org` для mpfr, `multiprecision.org` для mpc. Хеши и источники — в README стадии 18. Если источники расходятся — стоп, BLOCKED.

### Задача 10. Сборка
- В `config.sub` и `config.guess` этих версий нет riscv64. Подменить их на файлы из закреплённого тарболла binutils 2.30 (там riscv64 есть) — записать в README.
- **gmp:** `./configure --prefix=/usr --build=riscv64-unknown-linux-musl --disable-shared --enable-static CC=tcc` с обобщённым кодом на C. Если gmp 4.3.2 не умеет `--disable-assembly`, указать `--host=none-unknown-linux-musl` или `ABI`/`MPN_PATH=generic`, как требует его `configure`. Выбор и причина — в README.
- **mpfr:** `--with-gmp=/usr`. **mpc:** `--with-gmp=/usr --with-mpfr=/usr`.
- `make && make check && make install` для каждой.

### Задача 11. Гейт
- Итог каждого `make check` (сколько тестов прошло) в UART; метки `gmp check ok`, `mpfr check ok`, `mpc check ok` печатает `run.sh`, только если `make check` вернул 0.
- `tilck.py --rung gmp`: три метки плюс `rootfs sha256 match`, `rung done gmp`.
- Строки журнала в design doc про `build1`, `binutils`, `gmp`.

## Итоговый отчёт
Гейты с выдержками; отклонения от live-bootstrap и upstream; ошибки tcc, Tilck, musl и чем закрыты; размеры `rootfs-*.tar`; время каждой ступени (особенно `configure` под эмуляцией).
