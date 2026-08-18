# Aegis-V

Минимальный **bootstrap** под RISC-V: короткая цепочка загрузки, которую можно прочитать и проверить человеком. Образ поднимается в QEMU (`virt`) и на плате Lichee Pi 4A.

Истоки: [зимняя школа ylab](https://github.com/ylab-nsu/ws25-bootstrap) и идея [live-bootstrap](https://github.com/fosslinux/live-bootstrap). Ассемблер: [RISC-V cheat sheet](https://projectf.io/posts/riscv-cheat-sheet/).

## Purpose

Обычный компилятор — чёрный ящик: много кода, мало доверия. Этот проект собирает первые стадии снизу вверх:

1. **boot** (stage 1) — единственный бинарник «из компилятора». Декодирует `hex0` и прыгает во вторую стадию.
2. **simon** (stage 2) — минимальное ядро: UART-CLI, крошечная ФС в образе, компиляция `hex0` / `hex1`, запуск программ, kernel panic с reboot/shutdown.
3. **payload** (stage 3) — тестовая программа («Hello, World!»), которую simon собирает уже сам.

Цель — не Linux и не полноценная ОС, а **воспроизводимый и обозримый путь** от нескольких килобайт до работающей среды на RISC-V.

## Роутер по проекту

| Хочу… | Иди сюда |
|---|---|
| Понять, зачем репозиторий | этот файл, секция Purpose |
| Запустить в QEMU | [`builder/`](builder/) → `make build-and-run` |
| Собрать образ без запуска | [`builder/Makefile`](builder/Makefile) |
| Поменять загрузчик | [`builder/src/boot.S`](builder/src/boot.S) |
| Поменять ядро / команды CLI | [`builder/src/simon.S`](builder/src/simon.S) |
| Поменять тестовый payload | [`builder/src/payload.S`](builder/src/payload.S), [`builder/payload.hex0`](builder/payload.hex0), [`builder/payload.hex1`](builder/payload.hex1) |
| Понять стадии boot / simon / hex0 | [`builder/parts.rst`](builder/parts.rst) |
| Прошить Lichee Pi 4A | [`builder-lichee/`](builder-lichee/) |
| UART-отладка с Arduino Nano | [`builder-lichee/src/nano_code_uar_read/`](builder-lichee/src/nano_code_uar_read/) |
| Интеграционные тесты | [`builder/tests/`](builder/tests/) |

### Карта каталогов

```
Aegis-V/
├── README.md                 ← ты здесь
├── builder/                  основная ветка: QEMU virt
│   ├── Makefile              сборка образа + qemu + pytest
│   ├── README.md             подробности сборки
│   ├── parts.rst             что такое boot / simon / hex0 / payload
│   ├── src/
│   │   ├── boot.S            stage 1, линк на 0x80000000
│   │   ├── simon.S           stage 2, ядро и CLI
│   │   └── payload.S         исходник тестовой stage 3
│   ├── payload.hex0          stage 3 в hex0 (упаковывается в образ)
│   ├── payload.hex1          то же в hex1
│   ├── bin-to-hex0.sh        .bin → hex0
│   ├── hex0.sh               hex0 → .bin
│   ├── elf2hex1.sh           ELF → черновик hex1
│   ├── add_null.sh           нулевой байт в конце файла ФС
│   └── tests/                boot, команды, hex0/hex1 через QEMU
└── builder-lichee/           железо Lichee Pi 4A
    ├── Makefile              образ для dd на флешку (+ U-Boot SPL)
    ├── config.mk             адрес DDR, источники, имена образов
    └── src/                  boot, ddr_init, варианты simon, trap
```

`docs/` и `presentation/` в репозитории больше нет — смотри этот README и `builder/parts.rst`.

## Как устроена загрузка

Образ для QEMU склеивается так:

```
image_qemu.bin = boot.bin + simon.hex0 + payload.hex0 + payload.hex1
```

- `boot` читает `simon.hex0` из хвоста образа, кладёт бинарник в RAM и прыгает в него.
- `simon` поднимает UART, поднимает файлы из образа (`src <имя>` … `\0`) и ждёт команды.
- `hex0` / `hex1` собирают payload; `do` запускает получившийся бинарник.

На QEMU `virt` UART0 = `0x10000000`, точка входа = `0x80000000` (флаг `-bios`, не `-kernel`).

## Быстрый старт (QEMU)

Зависимости: `riscv64-unknown-elf-gcc` (Linux) или `riscv64-elf-gcc` (macOS Homebrew), `qemu-system-riscv64`, `make`, `xxd`.

```bash
cd builder
chmod +x bin-to-hex0.sh hex0.sh add_null.sh elf2hex1.sh

# Linux / WSL
make build-and-run

# macOS, если префикс компилятора другой
make CROSS_COMPILE=riscv64-elf- build-and-run
```

Только собрать / только запустить:

```bash
make
qemu-system-riscv64 -M virt -bios image_qemu.bin -nographic
```

Выход из QEMU: **Ctrl+A**, затем **X**.

После загрузки: `[BOOTED]: Stage 2 (simon)` и промпт `Simon says~:`.

| Команда | Что делает |
|---|---|
| любой текст | echo: `echo answered …` |
| `ls` | файлы встроенной ФС |
| `hex0 payload.hex0` | собрать hex0 |
| `hex1 payload.hex1` | собрать hex1 |
| `do payload.hex0.bin` | запустить собранный файл |
| `debug` | kernel panic → `r` reboot / `s` shutdown |
| `reboot` | перезагрузка |
| `shutdown` | выключить (QEMU завершается) |

Подробности сборки и формат файлов ФС: [`builder/README.md`](builder/README.md).

## Железо (Lichee Pi 4A)

Это **другой** пайплайн: другой адрес загрузки (`0x40000000` в DDR), U-Boot SPL, прошивка флешки. QEMU-команды из `builder/` сюда не переносятся один в один.

```bash
cd builder-lichee
make          # → flash_me.bin
# sudo dd if=flash_me.bin of=/dev/sdX status=progress
# serial: screen /dev/ttyUSB0 115200
```

Источники и переменные — [`builder-lichee/config.mk`](builder-lichee/config.mk), текст — [`builder-lichee/README.md`](builder-lichee/README.md).

## Тесты

```bash
cd builder
make test
```

Проверяются boot-сообщения, CLI, `hex0`/`hex1` и запуск payload в QEMU.
