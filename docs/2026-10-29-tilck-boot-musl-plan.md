# Загрузка Tilck и musl — поэтапный план

> **Исполнитель:** одна фаза целиком вместе с гейтом, потом следующая. Ревью по фазам нет, в конце один отчёт.
> **Коммиты** — только после явного «да» пользователя в чате.
> **Всё проверяется на QEMU.**
> Прогресс отмечать чекбоксами `- [ ]`.

**Цель:** ядро Tilck, собранное гостем (`src/stages/12-tilck/artifacts/tilck.elf`), загружается в QEMU и запускает статические программы. musl 1.1.24 собран гостем, и программы на нём работают под Tilck: `fork`, `pipe`, `exec`, каталоги, 3 ГБ памяти. Результат программа отдаёт хосту через `/dev/outbox`.

**Архитектура:**
- **Сборка** остаётся на Kronos (`kronos.py --tcc-self-k`): tcc7b собирает SBI-прослойку, `init`, тесты и musl. Хост забирает их через `snap`.
- **Запуск** — новый драйвер `src/scripts/tilck.py`. Это отдельный процесс QEMU, который стартует с образа прошлой ступени.
  - `-bios` — SBI-прослойка, собранная гостем.
  - Через `-device loader` в память кладутся байты `tilck.elf`, initrd FAT32 и маленький блок `bootinfo` с адресами и размерами.
- **Initrd FAT32** упаковывает `src/scripts/lib/fat32.py`. Он только раскладывает готовые файлы, так же как `pack_kfs` раскладывает KFS.

**Стек:** QEMU `virt`, `-m 8G`. Дизайн: `docs/2026-10-28-gcc464-ladder-design.md`. Прошлый план: `docs/2026-10-28-tcc7-tilck-plan.md` (выполнен).

**Стратегия фаз:** risk first. Сначала загрузка ядра (GOT до включения страниц, SBI, дерево устройств), потом процессы, потом musl, потом память и `outbox`.

| Фаза | Имя | Гейт (UART Tilck или проверка хоста) |
|------|-----|------|
| 1 | Tilck печатает баннер | `Hello from Tilck` |
| 2 | init и процессы | `init ok`, `fork ok`, `pipe ok`, `exec ok`, `dirs ok` |
| 3 | musl | `hello from musl`, `musl proc ok` |
| 4 | Память и outbox | `mem 3072M ok`; sha256 файла из `/dev/outbox` совпадает с тем, что напечатал гость |

**Правило:** гейт фазы k зелёный, прежде чем начинать фазу k+1.

## Изменение дизайна (записать в design doc в фазе 3)

musl переезжает со ступени 5 сюда. Его собирает tcc7b на Kronos по списку файлов, как Tilck. Так все утилиты следующих ступеней (kaem, make, bash, …) сразу линкуются с настоящей libc, а не с `libc.c` Kronos. Tilck и сам рассчитан на программы с musl.

## Allowed paths

| Путь | Фазы |
|------|------|
| `src/stages/11-tcc7/src/override/*`, `src/stages/11-tcc7/tests/*`, `src/stages/11-tcc7/README.md` | 1–4 (только правки tcc7, у каждой — тест) |
| `src/stages/12-tilck/**` (кроме `artifacts/`, он в gitignore) | 1–4 |
| `src/stages/13-musl/**` (новая стадия) | 3 |
| `src/stages/09-kronos/src/kmain.c` (CRLF) | 1–4 |
| `src/scripts/kronos.py` | 1–4 |
| `src/scripts/tilck.py` (новый) | 1–4 |
| `src/scripts/lib/fat32.py` (новый) | 2 |
| `src/scripts/lib/phases.py` (только `STAGE_DIR`) | 3 |
| `build/sources/` (gitignore) | 3 |
| `docs/2026-10-28-gcc464-ladder-design.md` (только раздел про ступени и журнал) | 3 |
| этот план (только чекбоксы) | 1–4 |

Остальное не трогать: `07-tcc`, `10-tcc`, libc и ядро Kronos (кроме `kmain.c`), `lib/qemu.py` (новый драйвер можно на нём построить, но сам файл не менять), README в корне.

## Вне плана

- kaem, make, bash и дальше по лестнице.
- Итоговый прогон «с нуля в одном QEMU».
- Пересборка tcc7 под musl — следующий план.

---

## Фаза 1: Tilck печатает баннер

**Вход:** гейт плана 1 зелёный, `tilck.elf` собирается гостем.

**Выход:** `python3 -u src/scripts/tilck.py --boot` показывает в UART `Hello from Tilck`.

### Задача 1. tcc7: адресация без GOT для ядра

tcc mob обращается к глобальным символам, которые не `static`, через GOT с абсолютными виртуальными адресами (`riscv64-gen.c`, `load_symofs`, вызовы через `R_RISCV_GOT_HI20`). `start.S` Tilck вызывает C-код до включения страниц, по физическим адресам, поэтому такие обращения промахиваются. Tilck собирается с `-fno-pic`, как и у GCC (`-mcmodel=medany`).

- [x] **Тест** `src/stages/11-tcc7/tests/nopic.c`: глобальная переменная из другой единицы трансляции, указатель на функцию, вызов внешней функции. Тест собирается с `-fno-pic -static`. Запуск печатает `nopic ok`. Хост-проверка в `kronos.py`: в ELF теста нет секции `.got` или все обращения к данным идут через `R_RISCV_PCREL_HI20`. Исполнитель выбирает проверяемый признак и записывает его.
- [x] **Патч** в `override/riscv64-gen.c` (и в `libtcc.c`/`tcc.h`, если нужно разобрать флаг): при `-fno-pic` глобальные данные и вызовы адресуются через `auipc`+`addi`/`ld` (`R_RISCV_PCREL_HI20`), как у `static`. Без флага — поведение mob не меняется.
- [x] **`cflags.rsp` Tilck:** добавить `-fno-pic`. Гейт плана 1 (`tilck linked`) остаётся зелёным, и в `tilck.elf` больше нет обращений к GOT из кода ядра.

### Задача 2. SBI-прослойка

**Files:** `src/stages/12-tilck/src/sbi/sbi-entry.s`, `src/stages/12-tilck/src/sbi/sbi.c`, `src/stages/12-tilck/src/sbi/README.md`.

Режим M, около 300 строк. Собирает tcc7b на Kronos в сырой образ: `-nostdlib -static -Wl,-Ttext=0x80000000 -Wl,-oformat=binary` плюс libfdt из `/src/tilck/libfdt`. Хост забирает его через `snap sbi`.

Поведение:
- старт с `a0` = hartid и `a1` = адрес дерева устройств от QEMU;
- PMP открыт для режима S на всю память;
- `medeleg` и `mideleg` делегируют в S всё, кроме `ecall` из S и машинного таймера;
- `mcounteren` разрешает S-режиму читать `time`;
- обработчик ловушек в режиме M:
  - `ecall` из S:
    - `BASE` (0x10) — версия 0.2 и `probe_extension`;
    - `TIME` (0x54494D45) — `set_timer`: пишет `mtimecmp` CLINT (`0x2004000`) и снимает `STIP`;
    - legacy 0x01 — `putchar` в UART `0x10000000`;
    - legacy 0x02 — `getchar`;
    - `SRST` (0x53525354) — выключение через `sifive_test` (`0x100000`);
    - неизвестный — `SBI_ERR_NOT_SUPPORTED`;
  - машинный таймер: запретить `MTIE` и поднять `STIP`;
- блок `bootinfo` по фиксированному адресу `0x87F00000`: магия, `elf_addr`, `elf_size`, `initrd_addr`, `initrd_size`, строка `bootargs`;
- ELF-загрузчик: копирует `PT_LOAD` из `elf_addr` по физическому адресу `p_vaddr - 0xFFFFFFFF00000000` и обнуляет хвост `bss`;
- через libfdt дописывает в `/chosen` `linux,initrd-start`, `linux,initrd-end` и `bootargs`. Дерево копируется в буфер с запасом (`fdt_open_into`);
- `mret` в режим S на `e_entry - 0xFFFFFFFF00000000` с `a0` = hartid и `a1` = адрес дерева.

### Задача 3. Драйвер `tilck.py --boot`

**Files:** `src/scripts/tilck.py`.

- Читает `12-tilck/artifacts/{sbi.bin,tilck.elf}` и, если есть, `initrd.img`. Пишет `bootinfo.bin`.
- Запускает `qemu-system-riscv64 -M virt -m 8G -smp 1 -nographic -bios sbi.bin` с `-device loader,file=…,addr=…,force-raw=on`: `tilck.elf` по `0x88000000`, initrd по `0x90000000`, `bootinfo` по `0x87F00000`.
- Ждёт маркеры UART (по образцу `wait_uart` в `kronos.py`), пишет лог в `12-tilck/artifacts/uart-tilck.log` и гасит QEMU.
- Режимы: `--boot` (маркер `Hello from Tilck`), позже `--test` (маркеры фаз 2–4).

### Задача 4. Гейт фазы 1

```bash
python3 -u src/scripts/kronos.py --tcc-self-k 2>&1 | tee /tmp/b1-build.log   # nopic ok, tilck linked, snap sbi/tilck
python3 -u src/scripts/tilck.py --boot 2>&1 | tee /tmp/b1-gate.log
grep -E 'nopic ok|tilck linked' /tmp/b1-build.log; grep 'Hello from Tilck' /tmp/b1-gate.log
```

Если Tilck паникует, лог UART с паникой и адресом идёт в отчёт. Исправлять в пределах Allowed paths, если повтор — BLOCKED.

---

## Фаза 2: init и процессы

**Вход:** фаза 1 зелёная.

**Выход:** под Tilck `/initrd/bin/init` печатает `init ok` и запускает тесты процессов.

### Задача 5. Упаковщик FAT32

**Files:** `src/scripts/lib/fat32.py`. Функция `pack_fat32(files: dict[str, bytes]) -> bytes` строит образ FAT32 с длинными именами (LFN), каталогами и кластером 512 или 4096 байт. Тест на хосте: упакованный образ читается обратно своим же читателем в `fat32.py`, и содержимое совпадает. Готовые утилиты хоста (`mkfs.fat`, `mtools`) не нужны и не используются.

### Задача 6. init и тесты

**Files:** `src/stages/12-tilck/src/init.c`, `src/stages/12-tilck/tests/{proc.c,child.c}`.

Собирает tcc7b на Kronos как статические программы с Linux ABI. Их libc — это `libc.c` Kronos плюс наши обёртки системных вызовов в самих файлах через inline asm `ecall`. Номера riscv64: `clone` 220, `execve` 221, `wait4` 260, `pipe2` 59, `dup3` 24, `getdents64` 61, `mkdirat` 34, `unlinkat` 35, `chdir` 49.

- `init`: печатает `init ok`, затем `execve("/initrd/bin/proc")`.
- `proc`:
  - `clone(SIGCHLD)` как `fork`; ребёнок выходит с кодом 7, родитель через `wait4` получает 7 → `fork ok`;
  - `pipe2`, ребёнок пишет строку, родитель читает → `pipe ok`;
  - `execve("/initrd/bin/child")`, ребёнок печатает и выходит с 0 → `exec ok`;
  - `mkdirat`, `openat`/`write` в `/tmp/t/a`, `getdents64` видит `a`, `unlinkat` → `dirs ok`;
  - потом `tests done`.
- `kronos.py` выгружает `init`, `proc` и `child` через `snap`. `tilck.py --test` пакует их в initrd (`bin/init`, `bin/proc`, `bin/child`) и ждёт маркеры.

### Задача 7. Гейт фазы 2

```bash
python3 -u src/scripts/kronos.py --tcc-self-k > /tmp/b2-build.log 2>&1
python3 -u src/scripts/tilck.py --test 2>&1 | tee /tmp/b2-gate.log
grep -E 'Hello from Tilck|init ok|fork ok|pipe ok|exec ok|dirs ok|tests done' /tmp/b2-gate.log
```

---

## Фаза 3: musl

**Вход:** фаза 2 зелёная.

**Выход:** под Tilck `hello-musl` печатает `hello from musl`, а `musl-proc` (`fork`/`pipe`/`exec`/`opendir` через musl) печатает `musl proc ok`.

### Задача 8. Пин и генерация

- **Пин:** `build/sources/musl-1.1.24.tar.gz`, sha256 `1370c9a812b2cf2a7d92802510cca0058cc37e66a7bedd70051f0a34015022a3` (уже лежит). `kronos.py` разжимает gzip и читает tar, проверяя sha256. Это распаковка байтов, а не генерация.
- **`src/stages/13-musl/src/musl-gen.c`** — наша замена шагов `Makefile`/`tools/mkalltypes.sed`:
  - `obj/include/bits/alltypes.h` из `arch/riscv64/bits/alltypes.h.in` и `include/alltypes.h.in` (правила `mkalltypes.sed`: `TYPEDEF`, `STRUCT`, `UNION` → блоки `#if defined(__NEED_x) && !defined(__DEFINED_x)`);
  - `obj/include/bits/syscall.h` из `arch/riscv64/bits/syscall.h.in` (копия плюс строки с заменой `__NR_` на `SYS_`);
  - `obj/src/internal/version.h` с `#define VERSION "1.1.24"`.
  Сверка на хосте: выход совпадает с тем, что даёт `sed -f tools/mkalltypes.sed` и `make obj/include/bits/syscall.h` на копии дерева в `/tmp` (только диагностика).
- **`src/stages/13-musl/src/SOURCES`** — список файлов по правилу `Makefile` musl 1.1.24: `src/*/*.c` плюс `src/*/riscv64/*.[csS]`, где файл архитектуры заменяет общий с тем же именем; `crt/crt1.c`, `crt/crti.s`/`crtn.s` (если есть для riscv64). Список пишется один раз и лежит в репозитории.
- **`src/stages/13-musl/src/cflags.rsp`:** `-std=c99 -nostdinc -ffreestanding -D_XOPEN_SOURCE=700 -I /src/musl/arch/riscv64 -I /src/musl/arch/generic -I /build/musl/obj/src/internal -I /src/musl/src/internal -I /build/musl/obj/include -I /src/musl/include -I /lib/tcc7/include -fno-pic`. Флаги, которые tcc не знает (`-fomit-frame-pointer`, `-fno-stack-protector`), не передавать.

### Задача 9. Сборка на Kronos

- `kmain.c`:
  - `musl-gen` пишет заголовки;
  - каждый файл из `SOURCES` компилируется в `/build/musl/obj/...o` тем же циклом, что Tilck (переиспользовать `build_tilck_objs`, обобщив до `build_objs(list, cflags, srcroot, objroot)`);
  - `/bin/tcc7b -ar rcs /build/musl/lib/libc.a @/src/musl/objs.rsp`;
  - `crt1.o`, `crti.o`, `crtn.o` → `/build/musl/lib/`;
  - маркер `musl built`.
- Заголовки для установки: `/build/musl/include` = `include/` + `arch/generic/bits` + `arch/riscv64/bits` + сгенерированные `bits/alltypes.h`, `bits/syscall.h`. Порядок наложения — как в `make install-headers`.
- Тест-программы `src/stages/13-musl/tests/{hello-musl.c,musl-proc.c}` собираются tcc7b против musl: `-nostdlib -static -I /build/musl/include /build/musl/lib/crt1.o /build/musl/lib/crti.o SRC /build/musl/lib/libc.a /build/musl/lib/crtn.o`. Если tcc не находит `__*` из своего рантайма (атомики, `long double`), добавить `/src/tcc7/lib/{lib-arm64.c,atomic.S,stdatomic.c}` в линковку.
- `snap` для `hello-musl`, `musl-proc` и архива `musl-sysroot.tar` (заголовки, `libc.a`, `crt*.o`). Архив пишет наш маленький писатель ustar в `kmain`-шаге или отдельная программа на C в `13-musl/src/`. Он понадобится следующему плану.
- `tilck.py --test` кладёт `hello-musl` и `musl-proc` в initrd, а `proc` после своих тестов запускает их по очереди.

### Задача 10. Гейт фазы 3

```bash
python3 -u src/scripts/kronos.py --tcc-self-k > /tmp/b3-build.log 2>&1; grep -E 'musl built' /tmp/b3-build.log
python3 -u src/scripts/tilck.py --test 2>&1 | tee /tmp/b3-gate.log
grep -E 'hello from musl|musl proc ok' /tmp/b3-gate.log
```

В design doc в разделе «Лестница» отметить, что musl переехал в ступень 1.5, и добавить строку в журнал.

---

## Фаза 4: Память и outbox

**Вход:** фаза 3 зелёная.

**Выход:** под Tilck тест выделяет и заполняет 3 ГБ (`mem 3072M ok`); файл, записанный в `/dev/outbox`, хост забирает, и sha256 совпадает.

### Задача 11. Больше памяти

- В `tilck-config.txt` поднять `LINEAR_MAPPING_MB`, чтобы Tilck видел 8 ГБ QEMU (сколько позволяет раскладка Sv39 в `paging.c`: проверка `LINEAR_MAPPING_END > HI_VMEM_START`). Если 8 ГБ не влезает, взять максимум и записать в README.
- Тест `src/stages/12-tilck/tests/mem.c` (musl): `mmap` 3 ГБ анонимно, записать байт в каждую страницу, проверить, `munmap`. Печатает `mem 3072M ok`.

### Задача 12. `/dev/outbox`

- Патч Tilck (`override/modules/outbox/outbox.c` плюс регистрация модуля, по образцу `modules/null/null.c`): символьное устройство, `write` копирует байты в зарезервированную область RAM. Адрес и размер области берутся из `bootinfo`. SBI исключает её из узла `/memory` или добавляет `reserved-memory`. `close` печатает в консоль `outbox ADDR SIZE`.
- `tilck.py` по строке `outbox` делает `pmemsave` через QMP в `12-tilck/artifacts/outbox.bin`.
- Тест: `proc` пишет в `/dev/outbox` содержимое `/initrd/bin/child` и печатает его sha256, посчитанный в гостевой программе (маленькая реализация SHA-256 в тесте). Хост сравнивает.

### Задача 13. Гейт фазы 4

```bash
python3 -u src/scripts/kronos.py --tcc-self-k > /tmp/b4-build.log 2>&1
python3 -u src/scripts/tilck.py --test 2>&1 | tee /tmp/b4-gate.log
grep -E 'mem 3072M ok|outbox ' /tmp/b4-gate.log
```

Плюс в логе `tilck.py` строка `outbox sha256 match`.

---

## Итоговый отчёт

Гейты с выдержками UART; найденные пробелы tcc7/Tilck и чем закрыты; что записано в README стадий 12 и 13; что не сделано.
