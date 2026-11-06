# Ранние утилиты под Tilck — поэтапный план

> **Исполнитель:** одна фаза целиком вместе с гейтом. Ревью по фазам нет, в конце один отчёт.
> **Коммиты** — только после явного «да» пользователя.
> **Всё проверяется на QEMU.**

**Цель:** под Tilck работает tcc, собранный под musl и сходящийся сам с собой. Им по рецептам live-bootstrap собраны `kaem`, `make 3.82`, `patch 2.5.9`, `gzip 1.2.4`, `tar 1.12`, `sed 4.0.9`, `bzip2 1.0.8`, `coreutils 5.0`, `oyacc 6.6`, `bash 2.05b`. Каждая ступень отдаёт свой `rootfs.tar` через `/dev/outbox`, и следующая стартует с него.

**Архитектура:**
- **Kronos** (`kronos.py --tcc-self-k`) последний раз собирает «семя» для Tilck: `tcc-m` (tcc mob, слинкованный с musl), `libtcc1.a`, `kaem` и утилиты `mescc-tools-extra`, `checksum-transcriber`, `simple-patch`, наш `rung-init`, сисрут musl. Всё это упаковывается в `seed-rootfs.tar` (`mkustar`).
- **Ступень на Tilck** (`tilck.py --rung NAME`).
  - initrd собирается из:
    - `bin/init` — это `rung-init`;
    - `rootfs.tar` прошлой ступени (для первой — `seed-rootfs.tar`);
    - `steps/` ступени — рецепты, патчи, файлы `mk/`;
    - `distfiles/` — закреплённые тарболлы.
  - `rung-init` распаковывает `rootfs.tar` в `/` и запускает `/usr/bin/kaem --file /initrd/steps/run.kaem` с окружением `PREFIX=/usr`, `BINDIR=/usr/bin`, `LIBDIR=/usr/lib`, `INCDIR=/usr/include`, `DISTFILES=/initrd/distfiles`, `ARCH=riscv64`, `PATH=/usr/bin`.
  - В конце `run.kaem` упаковывает `/usr` в `/dev/outbox`. Хост забирает его в `artifacts/rootfs-NAME.tar` и сверяет sha256 с тем, что напечатал гость.
- **Рецепты** — копии `steps/<pkg>/` live-bootstrap на закреплённом коммите `b1ceced7ea8a819a26f23796f6dd7ae8496a33a4` (https://github.com/fosslinux/live-bootstrap). Правки под нас (tcc-m, пути, musl вместо mes libc) — отдельными патчами с объяснением в README стадии.
- **Тарболлы.** Хост скачивает их по адресам из файлов `sources` live-bootstrap и проверяет sha256 оттуда же, затем кладёт в `build/sources/`. Гость проверяет их ещё раз через `sha256sum -c`, как в рецептах.

**Стек:** QEMU `virt` 8G. Прошлые планы: `docs/2026-10-28-tcc7-tilck-plan.md`, `docs/2026-10-29-tilck-boot-musl-plan.md` (выполнены). Факты: README стадий 11, 12, 13.

**Стратегия:** risk first. Сначала механизм ступени и tcc на Tilck, потом утилиты по порядку live-bootstrap.

| Фаза | Имя | Гейт |
|------|-----|------|
| 1 | Семя на Kronos | `seed built`; `seed-rootfs.tar` выгружен; внутри `tcc-m`, `kaem`, утилиты, сисрут |
| 2 | Ступень `tcc` на Tilck | `tilck hello ok`, `tccm fixed ok` (tcc-m2 ≡ tcc-m3), `rootfs sha256 match` |
| 3 | Ступень `tools1` | `make`, `patch`, `gzip`, `tar`, `sed`, `bzip2` печатают версии; tar+gzip туда-обратно; `make` по тестовому Makefile; `rootfs sha256 match` |
| 4 | Ступень `tools2` | `coreutils 5.0`, `oyacc`, `bash 2.05b`; тест-скрипт bash (конвейер, `$(…)`, `$?`, `case`, функция) даёт ожидаемый вывод; `rootfs sha256 match` |

## Allowed paths

| Путь | Фазы |
|------|------|
| `src/stages/14-seed/**` (новая: README, `src/rung-init.c`, рецепты сборки семени, тесты) | 1–2 |
| `src/stages/15-early-tools/**` (новая: README, `steps/<pkg>/` из live-bootstrap и наши патчи к ним, `steps/run-*.kaem`, тесты) | 3–4 |
| `src/stages/11-tcc7/src/override/*`, `tests/*`, `README.md` | 1–4 (только если блокирует ошибка tcc; тест сначала) |
| `src/stages/12-tilck/**` | 1–4 (только если блокирует ошибка Tilck) |
| `src/stages/13-musl/**` | 1–4 (только если блокирует ошибка musl или его сборки) |
| `src/stages/09-kronos/src/kmain.c` (CRLF), `src/scripts/kronos.py`, `src/scripts/tilck.py`, `src/scripts/lib/phases.py` (`STAGE_DIR`) | 1–4 |
| `build/sources/` (gitignore) | 1–4 |
| `docs/2026-10-28-gcc464-ladder-design.md` (лестница и журнал) | 4 |

Не трогать: `07-tcc`, `10-tcc`, libc Kronos и прочие файлы Kronos, `lib/qemu.py`, README в корне.

## Вне плана

- grep, gawk, m4, diffutils, flex, binutils, gmp/mpfr/mpc, GCC — следующие планы.
- Перегенерация `configure` (perl, autoconf).

---

## Фаза 1: Семя на Kronos

### Задача 1. Пины
- stage0-posix `45d90f5955b6907dc6cdea9ebafce558359edcd3` (подмодуль live-bootstrap, https://github.com/oriansj/stage0-posix, вместе с его подмодулями `mescc-tools`, `mescc-tools-extra`, `M2libc`). `git archive` каждого → `build/sources/`, sha256 в README.
- live-bootstrap `b1ceced7…` — `git archive` → `build/sources/live-bootstrap-b1ceced7.tar` (рецепты, `checksum-transcriber`, `simple-patch`).
- tcc mob — уже закреплён (стадия 11).

### Задача 2. Сборка на Kronos (tcc7b + musl)
- **`tcc-m`.** Исходники mob и override стадии 11, свой `config.h` в `14-seed/src/`:
  - `CONFIG_TCCDIR "/usr/lib/tcc"`, `CONFIG_TCC_SYSINCLUDEPATHS "{B}/include:/usr/include"`, `CONFIG_TCC_LIBPATHS "/usr/lib"`, `CONFIG_TCC_CRTPREFIX "/usr/lib"`;
  - `CONFIG_TCC_PREDEFS 0`, `CONFIG_TCC_STATIC 1`, `TCC_LIBTCC1 "libtcc1.a"`;
  - по умолчанию статическая линковка (если в mob есть такой конфиг; иначе рецепты передают `-static`, как live-bootstrap).
  Линковка — шаблон musl из README стадии 13.
- **`libtcc1.a`** — `lib/lib-arm64.c`, `lib/atomic.S`, `lib/stdatomic.c`, `lib/builtin.c`, `lib/alloca.S` (то, что есть для riscv64 в `lib/Makefile` mob), собран `tcc7b` и `tcc -ar`.
- **`kaem`** (mescc-tools) и из mescc-tools-extra: `untar`, `ungz`, `unbz2`, `unxz` (если собирается), `sha256sum`, `catm`, `mkdir`, `cp`, `chmod`, `rm`, `replace`, `match`, `wrap` (что есть). Собираются обычным C с musl; `M2libc` не используется. Если файлу нужен `M2libc`-специфичный заголовок, это записывается в README и закрывается минимальной правкой в `14-seed/src/override/`.
- **`checksum-transcriber`, `simple-patch`** — из `steps/*/src` live-bootstrap.
- **`rung-init.c`** (наш):
  - открывает `/dev/console`;
  - распаковывает `/initrd/rootfs.tar` в `/`. Можно наш маленький читатель ustar или запуск `/initrd/bin/untar`, но тогда `untar` должен лежать в initrd;
  - выставляет окружение;
  - делает `execve("/usr/bin/kaem", {"kaem", "--verbose", "--strict", "--file", "/initrd/steps/run.kaem"})`;
  - после завершения kaem (форк, ожидание) печатает `rung exit N` и висит, потому что pid 1 не выходит.
- **`seed-rootfs.tar`** (`mkustar`): `/usr/bin/{tcc,kaem,утилиты}`, `/usr/lib/{crt*.o,libc.a}`, `/usr/lib/tcc/{libtcc1.a,include/*}`, `/usr/include/*` (musl). `rung-init` идёт отдельным файлом. Маркер `seed built`, `snap seed` и `snap rinit` → `14-seed/artifacts/`.

### Задача 3. Гейт фазы 1
`kronos.py --tcc-self-k` (в фоне, около 10 минут) зелёный, `seed built`. Хост распаковывает `seed-rootfs.tar` в `/tmp` и проверяет список файлов и ELF-заголовки (riscv64, статические).

---

## Фаза 2: Ступень `tcc` на Tilck

### Задача 4. `tilck.py --rung NAME`
- Собирает initrd:
  - `bin/init` — `14-seed/artifacts/rung-init.elf`;
  - `rootfs.tar` — `seed-rootfs.tar` или `artifacts/rootfs-<прошлая>.tar`, по таблице ступеней в `tilck.py`;
  - `steps/` из каталога ступени;
  - `distfiles/` — тарболлы ступени.
- Ждёт `rung done NAME` или `rung exit N` / `PANIC`. По строке `outbox` забирает `rootfs-NAME.tar`, сверяет sha256 с `rootfs sha256 <hex>`, который печатает гость, и пишет `rootfs sha256 match`.
- Таймаут — по ступени, с запасом.

### Задача 5. Ступень `tcc` (`14-seed/steps/run.kaem` + файлы)
- tcc-m на Tilck собирает и запускает `hello.c` с musl → `tilck hello ok`.
- tcc-m собирает tcc mob (с override стадии 11) → `tcc-m2`; `tcc-m2` → `tcc-m3`; `cmp` → `tccm fixed ok`. В `/usr/bin/tcc` ставится `tcc-m3`, `libtcc1.a` пересобирается им же.
- Упаковка `/usr` в `/dev/outbox` и `rootfs sha256 <hex>`. Утилиты: `mkustar` (стадия 13) в семени или `tar` после ступени `tools1`. Здесь — `mkustar`, а SHA-256 считает `sha256sum` из mescc-tools-extra.
- `rung done tcc`.

### Задача 6. Гейт фазы 2
`tilck.py --rung tcc`: `tilck hello ok`, `tccm fixed ok`, `rootfs sha256 match`, `rung done tcc`.

---

## Фаза 3: Ступень `tools1`

### Задача 7. Тарболлы и рецепты
- Из `steps/{make-3.82,patch-2.5.9,gzip-1.2.4,tar-1.12,sed-4.0.9,bzip2-1.0.8}/sources` live-bootstrap взять адреса и sha256, скачать на хосте в `build/sources/`, проверить sha256.
- Скопировать `steps/<pkg>/` (`pass1.kaem`, `mk/`, `patches/`, `files/`, `sources`) в `15-early-tools/steps/<pkg>/`. Наши отличия — патчами к рецептам с README:
  - `tcc` — это tcc-m;
  - нет `mes libc`: патчи live-bootstrap «для mes libc» применять, только если они нужны и с musl. Каждое решение — строка в README;
  - всё остальное без изменений.
- `steps/run.kaem`: по очереди `cd` в каждый `steps/<pkg>` с `pkg=<имя>` и запуск его `pass1.kaem` (как `script-generator` live-bootstrap), затем тесты и упаковка `/usr`.

### Задача 8. Тесты и гейт
- `make --version`, `patch --version`, `gzip --version`, `tar --version`, `sed --version`, `bzip2 --help`;
- `tar cf` + `gzip` → `gunzip` + `tar xf`, `cmp` с оригиналом → `tar gzip ok`;
- `make -f test.mk` собирает программу → `make ok`.
- `tilck.py --rung tools1`: всё выше плюс `rootfs sha256 match`, `rung done tools1`.

---

## Фаза 4: Ступень `tools2`

### Задача 9. coreutils 5.0, oyacc 6.6, bash 2.05b
Так же, как фаза 3: рецепты live-bootstrap (`pass1.kaem` + `mk/` + `patches/`) с нашими патчами к ним. Порядок: `coreutils-5.0`, `oyacc-6.6`, `bash-2.05b`.

### Задача 10. Тесты и гейт
- `tests/sh-test.sh`, запуск `bash sh-test.sh`: конвейер `printf | sed | tr`, подстановка `$(…)`, `$?` после `false`, `case`, функция с локальной переменной, `for`, `while read`, перенаправление в файл и `cat` → `bash ok`;
- `ls`, `cp`, `mv`, `rm`, `mkdir -p`, `install`, `chmod`, `test`, `expr`, `cat`, `head`, `tail`, `wc`, `sort`, `uniq`, `tr`, `cut` — однострочные проверки → `coreutils ok`;
- `tilck.py --rung tools2`: `bash ok`, `coreutils ok`, `rootfs sha256 match`, `rung done tools2`;
- в design doc — строки журнала про ступени `tcc`, `tools1`, `tools2`.

## Итоговый отчёт
Гейты с выдержками; отклонения от рецептов live-bootstrap и почему; найденные ошибки tcc, Tilck, musl и чем закрыты; размеры `rootfs-*.tar`; время ступеней.
