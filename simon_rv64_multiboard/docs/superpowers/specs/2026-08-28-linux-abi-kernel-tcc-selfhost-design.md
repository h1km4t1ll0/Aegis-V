# Ядро с Linux RV64 ABI и self-host TCC

Дата: 2026-08-28  
Репо: `simon_rv64_multiboard`  
Статус: согласованный дизайн, реализация не начата

## Цель

Среда, в которой TCC 0.9.26 собирается из дерева `stages/08-tcc` как обычная Unix-программа: пути, заголовки, libc, ELF. Без амальгамы `pnut_tcc.c`, без `#define float int`, без плоского ramfs Simon и без raw-`do` как модели запуска.

Промежуточный (и единственный для этой спеки) критерий успеха:

```text
/bin/tcc2 -version          →  tcc version 0.9.26
cmp /bin/tcc2 /bin/tcc3     →  совпадают
```

`tcc2` собран seed-`/bin/tcc` из исходников `08-tcc`. `tcc3` собран `tcc2` той же командной строкой.

Дальше (не эта спека): перенос hex0…pnut как `/bin/*`, затем gcc 4.0.x. Современный GCC одним TCC не собирается; путь live-bootstrap (tcc → gcc 4.x на C → новый gcc) требует того же Linux syscall ABI, не отдельного «своего» ABI.

## Не цели

- Не Linux, не xv6, не OpenSBI/S-mode в первом срезе.
- Не virtio-blk, не сеть, не SMP, не MMU/SATP.
- Не musl (отдельный этап после self-host).
- Не порт стейджей hex0–mes на новое ядро.
- Не правки алгоритма компилятора в `08-tcc` под Simon (кодогенератор, парсер, `switch` по токенам).
- Не коммит этой спеки, пока пользователь явно не попросит.

## Платформа

- QEMU: `qemu-system-riscv64 -M virt -bios kernel.bin` (как текущий Simon: образ = firmware с `0x80000000`).
- Привилегии: ядро M-mode, userland U-mode, `ecall` = Linux RV64 syscall.
- Железо (Lichee/Banana) — после зелёного self-host на virt, не блокирует дизайн.

Номера syscall — Linux RISC-V 64, не выдуманные. Первый набор (обязаны быть настоящими, не `return 0`):

| a7 | имя |
|----|-----|
| 17 | getcwd |
| 56 | openat |
| 57 | close |
| 62 | lseek |
| 63 | read |
| 64 | write |
| 93 | exit |
| 160 | uname |
| 169 | gettimeofday |
| 214 | brk |
| 222 | mmap |
| 226 | mprotect |
| 215 | munmap |
| 34 | mkdirat |
| 79 | newfstatat |
| 80 | fstat |
| 25 | fcntl |
| 113 | clock_gettime |
| 48 | faccessat |

Нереализованный syscall: `-ENOSYS` (−38), не молчаливый 0. Нет файла: `-ENOENT` (−2). `ioctl`: `-ENOTTY` (−25), пока не понадобится TTY для gcc.

`mmap` в первом срезе: анонимный (`MAP_ANONYMOUS`) bump-аллокатор, достаточный для tcc; file-backed mmap — не обязателен, пока tcc не потребует.

## Как появляется `kernel.bin`

Цепочка hex0→…→`tcc-pnut` остаётся **производителем** ядра, не средой self-host.

1. В госте Simon: `tcc-pnut` собирает ядро как **freestanding** C + минимальный asm, raw binary, база **`0x80000000`** (не user-слот `0x82000000`).
2. Для этого нужен отдельный режим линковки TCC «kernel»: raw, `text_addr = 0x80000000`, без user CRT (`tcc-crt` / `simon_crt`). Существующий force EXE→`0x82000000` для userland Simon не ломаем: kernel — другой флаг или отдельный вход.
3. Хост снимает `kernel.bin` из гостя (как сейчас cache/plant).
4. Второй QEMU грузит только новое ядро: `-bios kernel.bin`.

Ядро не линкуется с userspace libc. Свои `memcpy`/`memset`/`kprintf` внутри ядра.

Asm ядра — синтаксис, который ест TCC 0.9.26 RV (три операнда, без `ret`/`call`, CSR при необходимости через кодировки). Не рассчитывать на gcc-extended asm.

## Состав ядра

Каталог: `stages/10-k/` (stage после pnut/tcc).

| файл | роль |
|------|------|
| `entry.s` | стек, `mtvec`, обнуление BSS, вызов `kmain` |
| `uart.c` | 16550 @ `0x10000000` |
| `trap.c` | сохранение регистров, разбор `mcause`, ecall → `syscall.c`, `mret` в U-mode |
| `kmem.c` | bump: `brk` процесса и анонимный `mmap` |
| `ramfs.c` | дерево путей (`/`, каталоги), не плоский список имён |
| `elf.c` | загрузка static `ET_EXEC` RV64, вход в U-mode |
| `proc.c` | один процесс в первом срезе; таблица fd; `exit` |
| `syscall.c` | таблица a7 |
| `kmain.c` | разбор ramfs-blob, `exec("/bin/init")` |

Нет: планировщик на несколько CPU, fork, динамический линкер.

Паника: строка на UART, бесконечный цикл. Userspace `exit(n)`: init печатает код на UART, ядро не ребутит (тест ловит код).

## Ramfs

Формат: хостовая упаковка **с путями** (не `files.pl` Simon). Хост делает `cat kernel.bin ramfs.bin > firmware.bin`. QEMU грузит весь файл с `0x80000000`. Линкерный символ `_end` — конец связанного ядра; сразу после него лежит заголовок `{magic "KFS1", size}` и архив. `kmain` читает ramfs с `&_end`, не сканирует память и не использует QEMU `-initrd`.

Дерево минимум:

```text
/bin/init
/bin/tcc          # seed: текущий tcc-pnut, упакованный как ELF (или один конвертер raw→ELF)
/bin/sh           # достаточно exec скрипта self-host; полноценный POSIX sh не обязателен
/include/…        # stdio, stdlib, stdarg, string, errno, unistd, fcntl, setjmp,
                  # time, math, assert, stddef, stdint, limits, ctype,
                  # sys/stat.h, sys/time.h, sys/types.h
/lib/crt.o
/lib/libc.c       # или несколько .c, не stubs «return 0» для I/O и malloc
/src/tcc/         # tcc.c libtcc.c tccpp.c tccgen.c tccelf.c tccasm.c
                  # riscv64-gen.c riscv64-link.c riscv64-asm.c
                  # tcc.h libtcc.h config.h tcctok.h riscv64-tok.h elf.h stab.h stab.def
```

`openat(AT_FDCWD, path)` понимает относительные пути от cwd и абсолютные `/…`. `#include <stdio.h>` ищет `/include/stdio.h` (sysinclude). `#include "libtcc.c"` ищет рядом с текущим файлом (`/src/tcc/libtcc.c`).

## Userspace libc и CRT

Свой маленький POSIX-слой на Linux syscall’ах. Не musl. Не `pnut_tcc_libc.c` с `printf` = запись форматной строки как есть.

Обязаны работать: `printf`/`snprintf`/`fprintf` с `%s` `%d` `%x`; `malloc`/`free` через `brk`; `fopen`/`fread`/`fwrite`/`FILE*` как поток с fd; `memcpy`/`strlen`/`strto*`; `open`/`read`/`write`/`close`/`lseek`; `exit`.

`/lib/crt.o`: `_start` читает argc/argv с стека U-mode (ядро кладёт Linux-совместимый стек: argc, argv[], NULL, envp[], NULL, auxv), вызывает `main`, затем `exit`.

Seed `/bin/tcc`: если текущий `tcc-pnut` (сборка pnut с `float=int`) не переваривает чистое дерево `08-tcc` (`duplicate case` и т.п.), **один раз** чиним seed (пересборка tcc без искажения типов), а не исходники `08-tcc`. Успех спеки — `tcc2`/`tcc3` из чистого дерева.

## Контракт сборки TCC (без бубна)

Команда (пути могут быть абсолютными, смысл фиксирован):

```text
/bin/tcc -o /bin/tcc2 \
  -DONE_SOURCE -DTCC_TARGET_RISCV64 \
  -I /include \
  /src/tcc/tcc.c
```

`tcc.c` сам включает `libtcc.c` и остальное через `#include "..."`. Не склеивать amalgamation. Не вырезать `#include`. Не `#define float int`.

Допустимо: `config.h` под RISC-V / `ONE_SOURCE`; флаги `-nostdlib` + явный `/lib/crt.o` `/lib/libc.c`, если так проще линковать. Это флаги, не правка семантики компилятора.

Запрещено переносить в это дерево патчи Simon: force raw `0x82000000`, отключение crt1 через `if (0)`, срез префикса `./` в `tcc_open` как замена каталогов.

Выход `tcc2` — static ELF `ET_EXEC` RV64, ядро грузит его через `elf.c`.

Второй проход той же строкой с `/bin/tcc2` → `/bin/tcc3`. `cmp` байт-в-байт.

## Поток

1. Simon: собрать `kernel.bin` + вшить ramfs.
2. Хост: второй QEMU `-bios kernel.bin`.
3. UART: баннер ядра.
4. `exec /bin/init`.
5. Init запускает self-host (скрипт или фиксированные exec).
6. На UART: `tcc version 0.9.26` от tcc2; код выхода 0 после `cmp`.

## Тесты (автомат)

Отдельный прогон, по духу `scripts/run_m2_qemu_test.py --tcc-self`, но образ — новое ядро, не stage09 Simon.

Обязательные гейты, по порядку:

1. UART баннер ядра, init стартовал.
2. Минимальный ELF: `write(1, "hi\n", 3)` → на UART есть `hi`.
3. `open("/src/tcc/tcc.c")` + `read`: длина совпадает с файлом на хосте.
4. Self-host: `tcc2 -version` содержит `tcc version 0.9.26`.
5. `cmp /bin/tcc2 /bin/tcc3` успешен (exit 0).

Пока пункт 5 не зелёный — ядро не готово. Порт hex0 и gcc в этот набор не входят.

## Риски (принятые)

- TCC 0.9.26 слабоват для «ядра как Linux»: CSR, привилегии, inline asm — только проверенный синтаксис TCC RV.
- Seed tcc-pnut может не скомпилировать `08-tcc`; тогда чиним seed, не дерево TCC.
- Анонимный mmap-bump может не покрыть всё, что захочет tcc; тогда расширяем `kmem.c`, не меняем цель спеки.
- QEMU virt `0x10000000` UART и DRAM map — как у текущего `boards/qemu_virt.h`; другие платы вне среза.

## Критерий готовности одной фразой

На QEMU virt новое ядро загружает ELF, по Linux syscall’ам seed-tcc собирает TCC из `stages/08-tcc`, этот бинарь печатает `tcc version 0.9.26` и фиксирует сам себя (`tcc2` = `tcc3`).
