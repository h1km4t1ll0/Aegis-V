# Лестница до GCC 4.6.4: дизайн

Дата: 2026-10-28. Статус: дизайн, ждёт ревью. Плана реализации ещё нет.

Заменяет ручную сборку из `docs/2026-10-18-gcc464-plan.md` и `docs/2026-10-27-musl-cc1plus-plan.md`.

## Цель

Гость в QEMU `virt` собирает GCC 4.6.4 (форк Ekaitz Zárraga с RISC-V) его же `configure && make bootstrap`. Сборка проходит три стадии: tcc → gcc₁ → gcc₂ → gcc₃. Готово, когда GCC сам печатает `Comparison successful` (gcc₂ ≡ gcc₃), а hello, собранный gcc₃ и слинкованный с musl, печатает в UART.

Следующая цель после этой — GCC 9.5 (проверенный путь live-bootstrap на riscv64: 4.6.4 → 9.5). В GCC 5 и 6 RISC-V нет, первый upstream GCC с RISC-V — 7.1. В этот документ 9.5 не входит.

## Принципы

- **Только своё.** В лестнице только то, что гость собрал сам из исходников с записанным хешем или коммитом. Готовые бинари (компиляторы, тулчейн bootlin, OpenSBI, пакеты Guix или Debian, старые `cc1.elf`) не входят.
- **Хост только кладёт байты.** Хост не компилирует, не запускает `configure`, `awk`, `sed`, `flex` и генераторы GCC. Он копирует закреплённые тарболлы и скрипты ступеней в память гостя и забирает оттуда результат.
- **Своё пишем только там, где готового нет:** SBI-прослойка, устройство `/dev/outbox`, `init`, `gen-tilck-config.c`, `headers-install.c`, скрипты ступеней, драйвер QEMU на хосте. Утилиты, libc и ядро берём upstream, патчи кладём рядом и записываем.
- **Сгенерированные файлы из релизных тарболлов пока принимаем как исходник:** `configure`, `Makefile.in`, сгенерированные парсеры. Перегенерация через perl и autoconf, как в live-bootstrap, — отдельная ступень после этой цели. Файлы, которых нет в git-архиве Ekaitz (например `gengtype-lex.c`, `options.c`), генерирует гость внутри `make`.
- **Гейт — проверка, которую делает сама программа:** `cmp`, `make check`, `Comparison successful`, а не строка, которую мы печатаем сами.
- **Образ на каждую ступень.** Разработка продолжается с образа прошлой ступени (`--from`). Итоговая проверка — полный прогон с hex0.

## Лестница

Начало не меняется: hex0 → … → tcc-pnut → Kronos → tcc2 … tcc6 (UART `hello from tcc6`).

| # | Ступень | Кто собирает | Что появляется | Гейт |
|---|---|---|---|---|
| 0.5 | **tcc7** | tcc6 на Kronos | upstream tcc mob на пине плюс наши патчи (см. «tcc7») | tcc7 собирает сам себя, tcc7 ≡ tcc8; проходят тесты `src/stages/10-tcc/tests/` |
| 1 | **Tilck + SBI** | tcc7 на Kronos | ядро Tilck, libfdt, SBI-прослойка в режиме M; Kronos передаёт им управление | тест-программа печатает `fork ok`, `pipe ok`, `exec ok`, `dirs ok` |
| 1.5 | **musl 1.1.24** | tcc7 на Kronos, по списку файлов, как Tilck | `libc.a`, `crt1.o`/`crti.o`/`crtn.o`, заголовки; `musl-sysroot.tar` | под Tilck `hello-musl` печатает `hello from musl`, `musl-proc` — `musl proc ok` |
| 2 | **kaem** | tcc7 на Tilck | запускатель скриптов из stage0-posix | kaem-скрипт собирает hello через tcc7 |
| 3 | **make 3.82**, `patch 2.5.9`, `gzip 1.2.4`, `tar 1.12`, `sed 4.0.9`, `bzip2 1.0.8`, `coreutils 5.0` | tcc7, по рецептам live-bootstrap | базовые утилиты | `make` по Makefile собирает программу; `tar`/`gzip` распаковывают архив, `cmp` с оригиналом |
| 4 | **bash 2.05b** | tcc7 + make | shell | скрипт с конвейером, подстановкой команд и `$?` даёт ожидаемый вывод |
| 5 | **tcc7 под musl** (musl собран на ступени 1.5) | tcc7 + make + bash | `libc.c` Kronos уходит | tcc-под-musl собирает себя, две сборки совпадают |
| 6 | `grep 2.4`, `gawk 3.0.4`, `m4 1.4.7`, `diffutils 2.7`, `flex 2.6.4` | tcc7 + musl | всё, что нужно `configure` и генераторам GCC | `./configure && make` тестового пакета (например `diffutils`) проходит, его `make check` зелёный |
| 7 | **binutils 2.30** | `configure && make`, `CC=tcc` | `as`, `ld`, `ar`, `nm`, `ranlib` | hello собран этими `as`/`ld` и запускается |
| 8 | **gmp 4.3.2**, **mpfr 2.4.2**, **mpc 0.8.1** | `configure && make` | настоящая математика для GCC | собственный `make check` каждой библиотеки зелёный |
| 9 | **GCC 4.6.4** | `configure && make bootstrap`, `CC=tcc` | gcc₃, только C | `Comparison successful`, hello на gcc₃ + musl печатает в UART |
| 9b | **GCC 4.6.4 с C++** | gcc₃, `--enable-languages=c,c++` | `cc1plus`, libstdc++ | `hello.cpp` с `iostream` печатает в UART |

Версии на ступенях 3–6 берутся у live-bootstrap (`steps/manifest`). Версии на ступени 8 — из `contrib/download_prerequisites` дерева GCC 4.6.4. Каждый тарболл закрепляется по sha256 в README своей стадии.

### Закреплённые исходники, которые уже есть

| Что | Пин |
|---|---|
| GCC 4.6.4, форк Ekaitz | [codeberg ekaitz-zarraga/gcc](https://codeberg.org/ekaitz-zarraga/gcc) `c0d20242b1d6668a0939c9ccf128d404dce9071e`, `git archive` sha256 `fb8689273a59812638f7d38a6187ebd32fd0487c0a90100d12a86e6251bbbac0` |
| binutils 2.30 | `binutils-2.30.tar.bz2` sha256 `efeade848067e9a03f1918b1da0d37aaffa0b0127a06b5e9236229851d9d0c09` |
| musl 1.1.24 | sha256 `1370c9a812b2cf2a7d92802510cca0058cc37e66a7bedd70051f0a34015022a3` |
| Tilck | [vvaltchev/tilck](https://github.com/vvaltchev/tilck) `01dacaa51a074474a4e5491fc80addfe98956500` (2026-10-12). Тарболл делаем через `git archive` и пиним его sha256: архивы GitHub codeload не гарантируют одинаковые байты |
| tcc mob | [repo.or.cz/tinycc](https://repo.or.cz/tinycc.git) `e23dabed154a8495f3ac1ff3cc9d04fa1c207893` (2026-10-16), тоже через `git archive` |
| Linux uapi | тег `v6.6` (`ffc253263a1375a65fa6c9f62a893e9767fbebfa`), только `include/uapi` и `arch/riscv/include/uapi` |

## tcc7

tcc6 — это 0.9.26 с riscv64-бэкендом от janneke и нашими исправлениями (`src/stages/07-tcc/src`). В нём нет inline asm для riscv64, `_Generic`, `__atomic_*`, `_Static_assert`, а без них не собрать ни Tilck, ни musl.

**tcc7 — это upstream tcc mob** ([repo.or.cz/tinycc](https://repo.or.cz/tinycc.git)) на закреплённом коммите `e23dabed154a8495f3ac1ff3cc9d04fa1c207893` (2026-10-16), который собирает tcc6. Так же live-bootstrap собирает 0.9.27 из 0.9.26. В mob уже есть inline asm для riscv64 (`riscv64-asm.c`, 3059 строк), `_Generic`, `_Static_assert`, `_Alignof`, `__atomic_*` (рантайм `lib/atomic.S`, `lib/stdatomic.c`), `-Wl,-Ttext=` и `-Wl,-oformat=binary`. Собираем с `CONFIG_TCC_PREDEFS=0`: тогда `tccdefs.h` читается при запуске, и хостовый генератор `c2str` не нужен.

Чего не хватает и в mob (проверено компиляцией Tilck, см. ниже). Это наши патчи поверх mob, у каждого свой тест:

| Пробел | Кому нужен |
|---|---|
| ограничение `K` в inline asm (5-битная константа для CSR) | `riscv_utils.h` Tilck |
| `sfence.vma` с операндами в ассемблере | `riscv_utils.h` Tilck |
| `.macro` / `.endm` | 8 файлов `.S` Tilck (6 макросов в `asm_defs.h`) |
| `__attribute__((packed))` у `enum` (размер 1 байт) | `sched.h`, `signal.h` Tilck |
| предопределённые `__ATOMIC_RELAXED` … `__ATOMIC_SEQ_CST`, `__GCC_ATOMIC_*_LOCK_FREE`, `__riscv_xlen` | `atomics.h`, `basic_defs.h` Tilck |

Проверить, что tcc6 вообще собирает mob, на хосте нельзя: кросс-tcc на маке не сворачивает константы `long double` (`tccpp.c:2363`). Поэтому это первый гейт плана, и проверяется он на госте.

Линковку Tilck (символы скрипта линкера: адрес ядра, `__bss_start`, `_end`, `init_array`) решает фаза плана, которая линкует ядро.

## Ядро: Tilck

Почему Tilck, а не своё ядро: BSD-2, совместим с Linux на уровне бинарей, riscv64 поддерживается официально (Sv39, режим S, ns16550, PLIC, goldfish RTC). Уже есть всё, что нужно `make` и `bash`: `clone`, `execve`, `wait4`, `pipe2`, `dup3`, `getdents64`, `symlinkat`, `linkat`, `renameat2`, `unlinkat`, `utimensat`, `fchmodat`, `rt_sigaction`, `kill`, `setpgid`, `mmap`, ramfs с жёсткими и символьными ссылками, devfs.

### Проверка 2026-10-28

Диагностические кросс-tcc собраны на хосте (в лестницу не входят) и скомпилировали 114 файлов riscv64-ядра Tilck с musl 1.1.24.

- **tcc6** (наши исходники): с тремя временными обходами компилируются 40 файлов. Дальше всё упирается в отсутствие inline asm и `_Generic`.
- **tcc mob**: с обходами только тех пробелов, что перечислены в таблице tcc7, и с заголовками Linux uapi компилируются 62 файла. Остальные 52 упираются в эти же пробелы (`K`, `sfence.vma`, `.macro`), в отсутствующий `libfdt.h`, в обработку заголовков `headers_install` и в три места самого Tilck (список ниже). Видна только первая ошибка каждого файла.

### Наши дополнения к Tilck (патчи рядом с тарболлом)

- `kernel/safe_ringbuf.cpp` и `common/itoa.cpp` (около 440 строк C++) переписать на C.
- `include/tilck/common/compiler.h`: добавить ветку `__TINYC__` (сейчас там `#error Compiler not supported`).
- `kernel/main.c`: заголовки ACPICA подключаются без условия; обернуть их в `#if MOD_acpi`.
- Заголовки Linux uapi Tilck подключает через `include_next` из тулчейна (`linux/fb.h`, `linux/sched.h`, `asm-generic/signal-defs.h` и другие). Берём их из закреплённого тарболла Linux (как `linux-headers` у live-bootstrap). Обработку `make headers_install` делает наша программа `headers-install.c`: убирает `__user`, `__force` и `#include <linux/compiler.h>`, создаёт обёртки `asm/*.h` → `asm-generic/*.h`. На Kronos нет sed, поэтому это программа на C.
- Конфиг-заголовки (`config/*.h`, `config/kernel/*.h`, `modules_config.h`) генерирует гость по шаблонам CMake. Ступень 1 идёт ещё на Kronos, где нет ни shell, ни kaem, поэтому генератор — маленькая программа на C (`gen-tilck-config.c`). Её собирает tcc7 и запускает Kronos. CMake не нужен.
- Сборка Tilck на Kronos идёт по списку файлов (`tilck.rsp`), как сейчас Kronos собирает себя по `k.rsp`. Это последняя сборка, расписанная руками. Начиная со ступени 2 всё собирают скрипты и `make`.
- Модули: `console`, `serial`, `riscv_irq`, `goldfish`, `null`, `sysfs`, `tracing`. Без `fb`, `ramfb`, `acpi`, `pci`.
- Устройство `/dev/outbox` (см. ниже).
- Поднять `LINEAR_MAPPING_MB` с 896 так, чтобы Tilck видел 6–8 ГБ. Это главный риск.

### SBI-прослойка

Tilck ждёт под собой SBI. OpenSBI готовым брать нельзя, поэтому пишем свою маленькую прослойку в режиме M (около 200 строк): таймер (`sbi_set_timer`), консоль, `system_reset`, делегирование прерываний и исключений в режим S. Дальше она передаёт Tilck адрес дерева устройств, которое QEMU кладёт в память при старте. Адрес надо пронести через hex0 → Simon → Kronos. Перед передачей прослойка через libfdt вписывает в `/chosen` адрес initrd (`linux,initrd-start`, `linux,initrd-end`).

## Исходники и образы ступеней

**Вход ступени.** На ступенях 0.5 и 1 (Kronos) вход — нынешний KFS. Начиная с Tilck вход — **initrd FAT32**, который Tilck штатно монтирует в `/initrd` только для чтения. Хост упаковывает в образ FAT32 файлы как есть (как сейчас `lib/image.pack_kfs` упаковывает KFS) и кладёт образ в память. Внутри: закреплённые тарболлы ступени, её скрипт и `rootfs.tar` прошлой ступени.

Первый процесс — `/initrd/init`, наша маленькая программа на C. Она распаковывает `rootfs.tar` (читатель ustar, около 150 строк) и запускает скрипт ступени. Её собирает tcc7 на ступени 1, и нужна она, пока нет настоящего `tar` (ступень 3). Первый initrd для Tilck хост собирает из того, что гость выгрузил с Kronos: `tcc7`, `init`, `crt` и `libc` (так же, как сейчас забирается `tcc6.elf`).

**Выход ступени.** Гость сам упаковывает поставленное (`/usr`, `/bin`, `/lib`, `/include`) в архив и пишет его в `/dev/outbox`. Хост забирает байты в `src/stages/<стадия>/artifacts/rootfs.tar` и пишет в журнал sha256.

**Продолжение.** `--from <стадия>` стартует с `rootfs.tar` этой стадии, как сейчас `simon.py --from 06`. Полный прогон с hex0 — итоговая проверка цели.

Снимки всей машины QEMU (`savevm`) не используем: они зависят от версии QEMU, и внутри них не видно, что лежит.

## Стадии в репозитории

Номера рабочие, в плане их можно поменять.

| Стадия | Содержимое |
|---|---|
| `09-kronos` | без изменений, кроме удаления gcc-блоков; мост до передачи управления Tilck |
| `11-tcc7` | пин tcc mob, `config.h`, наши патчи поверх mob, тесты |
| `12-tilck` | пины Tilck, libfdt и Linux uapi; патчи Tilck; `gen-tilck-config.c`, `headers-install.c`, `tilck.rsp`; SBI-прослойка; `init`; тесты `fork`/`pipe`/`exec`/`dirs`/`mem` |
| `13-kaem` | пин stage0-posix / mescc-tools, скрипт |
| `14-coreutils` | make, patch, gzip, tar, sed, bzip2, coreutils: пины, патчи и рецепты по live-bootstrap |
| `15-bash` | bash 2.05b |
| `16-musl` | musl 1.1.24, пересборка tcc7 |
| `17-buildtools` | grep, gawk, m4, diffutils, flex |
| `18-binutils` | binutils 2.30 |
| `19-gmp-mpfr-mpc` | gmp 4.3.2, mpfr 2.4.2, mpc 0.8.1 |
| `20-gcc464` | пин GCC 4.6.4 (форк Ekaitz), патчи, скрипт `configure`/`make bootstrap` |

У каждой стадии, как и сейчас, есть `README.md`, `src/`, `tests/` и `artifacts/` (последний в gitignore).

## Что удаляем и что оставляем

**Перед удалением** одним коммитом записываем текущее состояние ветки как есть, чтобы ручная сборка 4.6.4 осталась в истории.

**Удаляем:**
- `src/stages/12-gcc464/` целиком: ручная сборка, заглушки `gmp.c` и `mpfr-stub.c`, сгенерированные хостом `options.c`, `options.h`, `gengtype-lex.c`, `gcov-iov.h`;
- `src/stages/13-musl/src/` (рукописные заголовки musl);
- в `src/scripts/kronos.py`: `_gcc464_*`, `_plant_binutils464`, `collect_kfs_gcc464`, `collect_kfs_musl` и их вызовы;
- в `src/stages/09-kronos/src/kmain.c`: блоки `/src/gcc/`, `/src/gcc474/`, `/src/gcc464/`, `/src/musl/`.

**Переносим в `docs/archive/`:** `2026-10-18-gcc464-plan.md`, `2026-10-27-musl-cc1plus-plan.md`.

**Переписываем:** `docs/bootstrap-to-linux.md` и `docs/2026-08-29-compiler-ladder.md` под эту лестницу; README в корне.

**Оставляем:**
- исправления tcc в `src/stages/07-tcc/src/` и тесты `src/stages/10-tcc/tests/gcc464-*.c`, `musl-alias.c`;
- хеши GCC 4.6.4 и binutils 2.30, они переезжают в README новых стадий.

## Риски и открытые вопросы

| Риск | Чем закрываем | Запасной путь |
|---|---|---|
| Tilck не видит больше 896 МБ на riscv64 | первая задача ступени 1: поднять `LINEAR_MAPPING_MB`, проверить выделение 4+ ГБ тестом | сборка gcc в 896 МБ (`make -j1`, по одному TU) или Kronos 2 |
| Линковка Tilck без скрипта линкера | решается в tcc7 (минимальная поддержка скриптов) или шагом после линковки | собственный Makefile с явным порядком секций |
| Глубже первой ошибки компиляции Tilck есть ещё пробелы tcc | первая фаза плана повторяет проверку на tcc7 до нуля ошибок | — |
| SBI и дерево устройств | тест: Tilck печатает баннер и `fork ok` в QEMU | — |
| Ранние утилиты по рецептам live-bootstrap написаны под mes libc и их kaem | на ступенях 2–4 ранние утилиты линкуются с нашим `libc.c`, который дополнен обёртками новых системных вызовов | собрать musl раньше, скриптом kaem |
| Сборка GCC под эмуляцией QEMU идёт долго | образы ступеней и `--from`; полный прогон только в конце | — |

## Журнал

- 2026-10-29: musl 1.1.24 переехал со ступени 5 на ступень 1.5. Его собирает tcc7b на Kronos по списку файлов (`src/stages/13-musl/src/SOURCES`), как Tilck; заголовки генерирует наш `musl-gen.c`. Программы следующих ступеней сразу линкуются с musl.
- 2026-10-29: ступени 2 и 5 слились в ступень Tilck `tcc` (`tilck.py --rung tcc`, `src/stages/14-seed`). Kronos последний раз собирает семя: `tcc-m` (tcc mob + musl), `kaem` и утилиты mescc-tools-extra, `rung-init`; под Tilck `tcc-m` собирает себя дважды, `tccm fixed ok` (tcc-m2 ≡ tcc-m3). Каждая ступень Tilck стартует с `rootfs.tar` прошлой и отдаёт свой через `/dev/outbox` (`rootfs sha256 match`). `rootfs-tcc.tar` 5 443 584 байт, 16 с.
- 2026-10-29: ступень 3 под Tilck — `tools1` (`src/stages/15-early-tools`): по рецептам live-bootstrap `b1ceced7` собраны `make 3.82`, `patch 2.5.9`, `gzip 1.2.4`, `tar 1.12`, `sed 4.0.9`, `bzip2 1.0.8`; гейт `tar gzip ok`, `make ok`. Отклонения от рецептов — правки под musl вместо mes libc и патч tar без chown (у Tilck один uid 0), каждое записано в README стадии. `rootfs-tools1.tar` 8 059 904 байт, около 45 с.
- 2026-10-29: `coreutils 5.0` ушёл со ступени 3 на ступень 4. Ступень Tilck `tools2`: `coreutils 5.0`, `oyacc 6.6`, `bash 2.05b` (порядок live-bootstrap); гейт `bash ok` (конвейер, `$(…)`, `$?`, `case`, функция с `local`, `for`, `while read`, перенаправления) и `coreutils ok`. `bash` стоит и как `/usr/bin/sh`. Ступень нашла четыре ошибки Tilck, исправлены в `12-tilck/src/override/`: `utimensat()` ставил файлу мусорное время, ramfs не давал менять время файла только для чтения, `execve()` брал не больше 4 КБ и 32 аргументов (теперь 128 КБ), `read()`/`write()` обычного файла обрывались на 12 КБ. `install` из coreutils пропатчен, чтобы не звать `chown(-1, -1)` (Tilck: `EPERM`). `rootfs-tools2.tar` 17 133 056 байт, 83 с.
- 2026-10-30: ступень 6 под Tilck — `build1` (`src/stages/16-buildtools`): `grep 2.4`, `gawk 3.0.4`, `diffutils 2.7`, `m4 1.4.7`, `flex 2.6.4` по `mk/` live-bootstrap, недостающие программы `coreutils 5.0` (`uname`, `env`, `date`, `dd`, `du`, `nice`, `chown`), `findutils 4.2.33` своим `configure`; гейт `build1 tools ok` и `configure ok` (настоящий `./configure && make && make install` тарболла grep-2.4, установленный grep работает). Сгенерированные файлы тарболлов (`scan.c`/`parse.c` flex, `getdate.c` coreutils, `configure`) приняты как исходник, `awktab.c` пересоздаёт наш yacc. С этой ступени сборки идут через bash (`/bin/sh` → `bash`). Ступень нашла: в tcc `lr`/`sc` кодировались как AMO (атомики musl обнуляли слово, `malloc` не переиспользовал память); rung-init не собирал сирот; в Tilck `brk` отдавал необнулённые страницы, не было `getpriority`/`setpriority`, не снималась блокировка exec после `#!`-скрипта, вложенный тик таймера вызывал панику. Всё исправлено, семя и ступени пересобраны. `rootfs-build1.tar` 25 331 200 байт, 235 с.
- 2026-10-30: ступень 7 — `binutils` (`src/stages/17-binutils`): binutils 2.30 собраны своим `configure` верхнего уровня и `make` с `CC="tcc -static"`, без патчей исходников; libtool съедает `-static`, поэтому `LDFLAGS="-static -Wc,-static"`. Гейт: восемь строк версий 2.30, `binutils asm ok` (`as` + `ld`), `binutils hello ok` (tcc + `ld` + musl), `binutils ar ok`. Перед ступенью в Tilck общий файловый offset после `fork()`/`dup()` (`make > log 2>&1` и `config.log` целые) и `abort()` в обработчике сигнала; ступень нашла ошибку эпилога tcc (кадр отдавался до загрузки `ra`/`s0`, прерывание таймера в этом окне портило `s0` — редкое зависание из плана 3). `rootfs-binutils.tar` 173 776 896 байт, 927 с.
- 2026-10-30: ступень 8 — `gmp` (`src/stages/18-gmp-mpfr-mpc`): gmp 4.3.2, mpfr 2.4.2, mpc 0.8.1 (версии из `contrib/download_prerequisites` GCC 4.6.4; тарболлы с gcc.gnu.org/pub/gcc/infrastructure сверены с ftp.gnu.org, mpfr.org, multiprecision.org) собраны своими `configure`, `make`, `make install` в `/usr`, статически, tcc, без патчей исходников. `config.sub`/`config.guess` (у gmp — `configfsf.*`) взяты из закреплённого тарболла binutils 2.30: в старых нет riscv64 и `linux-musl`. gmp — обобщённый C через CPU `none` (`--host=none-unknown-linux-musl`, `ABI=long`, `MPN_PATH=generic`): `--disable-assembly` появился только в gmp 5.1. Собственный `make check` зелёный у всех трёх: gmp 126 тестов в 7 каталогах, mpfr 148, mpc 57 (`gmp check ok`, `mpfr check ok`, `mpc check ok`). `rootfs-gmp.tar` 179 458 560 байт, 2817 с (из них `make check` 25 мин).
- 2026-10-31: ступень 9, шаг 1 — `gcc1` (`src/stages/19-gcc464`, план `docs/2026-10-30-gcc464-plan.md`): tcc собрал GCC 4.6.4 (форк Ekaitz `c0d20242`) только для C, `--disable-bootstrap`, в `/usr/gcc1`; три патча (`NATIVE_SYSTEM_HEADER_DIR`, `ucontext_t` для musl, обход Ekaitz в `ssa_operand_alloc`), каждый в README стадии. Гейт: `gcc version 4.6.4`, `hello from gcc-1`, `gcc1 sanity ok` (`-O2`). 23 из 25 файлов генераторов совпали с хостовыми побайтно, два других объяснены. `rootfs-gcc1.tar` 200 061 440 байт, 1552 с.
- 2026-10-31: ступень 9, шаг 2 — `musl2`: gcc-1 пересобрал musl 1.1.24 его собственными `configure` и `Makefile` (теперь и с `src/complex`); `musl2 ok`. Ступень нашла в Tilck короткую запись в канал и заглушки `getrlimit`/`prlimit64`, исправлены с тестами. `rootfs-musl2.tar` 200 238 592 байт, 1507 с.
- 2026-10-31: ступень 9 готова — `gcc3`: gcc-1 и musl-2 прошли `make bootstrap` (C, `-O1 -g0`): `Comparison successful`, `bootstrap compare ok`, gcc-3 установлен в `/usr`, `hello from gcc 4.6.4`, `gcc3 sanity ok`. Ступень нашла три ошибки `munmap()`/`mremap()` в Tilck (они портили кучу большого `cc1`; раньше их принимали за ошибку tcc) и предел в 30 аргументов для `#!`-скрипта; всё исправлено с тестами. `make bootstrap` шёл 12 496 с по часам гостя (stage 1 7556 с, stage 2 3041 с, stage 3 1536 с). Открытый вопрос: наш пересчёт после `make bootstrap` насчитал 465 из 595 объектов с ненулевым кодом `cmp`, а `compare` GCC засчитывает только код 1 (README стадии). Его закрывает независимая проверка неподвижной точки на ступени 9b. `rootfs-gcc3.tar` 193 118 720 байт.
- 2026-11-06: ступень 9b готова — `gcc4`: gcc-3 собрал GCC 4.6.4 с C и C++ (`--disable-bootstrap`) в `/usr`; `g++ -v: gcc version 4.6.4`, `hello from g++ 4.6.4` (iostream, string, vector, throw/catch), `gcc4 sanity ok`. Длинные пути заголовков `pb_ds` (tar 1.12 не читает префикс ustar) распаковывает свой `ustar-x.c`, а `mkustar` научился их писать; статические конструкторы C++ заработали после `riscv-init-array.patch`. Новая проверка неподвижной точки (cc1 от gcc-3 и cc1 от gcc-4 компилируют 71 исходник GCC) сначала дала 18 различий. Причина — ошибка в форке: `riscv_rtx_costs` взят из более нового GCC с другим списком аргументов, и 4.6 передавал ему код rtx вместо режима. Цены сложения, умножения и сдвигов читались за границей таблиц `mode_class`/`mode_size`, то есть зависели от раскладки `insn-modes.o`. Исправлено `riscv-rtx-costs.patch` (сигнатура и `mode = GET_MODE (x)`, как в `mips_rtx_costs` 4.6). Добавлен тест раскладки `cc1 layout ok`. `gcc3` перегнан (прогон 6, `rootfs-gcc3.tar` 193 119 232 байт, `10eca495…`), затем `gcc4` прогон 3: `cc1 layout ok`, `fixedpoint 71 files, 0 differ`, `fixedpoint ok`, `rung done gcc4`. `rootfs-gcc4.tar` 215 772 160 байт, `1caf88b7…`, 7065 с. Остался вопрос: наш пересчёт после `make bootstrap` по-прежнему видит 465 различных пар `prev-*`/текущих объектов (`cmp` код 1), хотя `compare` GCC успешен; неподвижная точка gcc-3 = gcc-4 его перекрывает (README стадии).

## Вне этого документа

- GCC 9.5, Linux, busybox.
- Перегенерация `configure` и парсеров (perl, autoconf, automake, bison) — отдельная ступень.
- Несколько ядер, сеть, блочные устройства.
