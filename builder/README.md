# builder — сборка и запуск в QEMU

Корневой обзор проекта: [../README.md](../README.md). Стадии boot / simon / hex0: [parts.rst](./parts.rst).

## Зависимости

* `riscv64-unknown-elf-gcc` (Linux/WSL) или `riscv64-elf-gcc` (macOS) / `riscv-none-elf-gcc` (Windows)
* `qemu-system-riscv64`
* `xxd`

## Запуск

**Linux / WSL / macOS:**

```bash
make build-and-run  # сборка + запуск
make                # только сборка
make run            # только запуск уже собранного образа
make test           # pytest через QEMU
```

На macOS, если компилятор из Homebrew:

```bash
make CROSS_COMPILE=riscv64-elf- build-and-run
```

**Windows (PowerShell):** Makefile рассчитан на Linux-утилиты (`chmod`, `stat`, `rm`) — нужен WSL:

```powershell
wsl make build-and-run
```

Выход из QEMU: `Ctrl+A`, затем `X`. Образ грузится как firmware: `-M virt -bios image_qemu.bin -nographic`.

### Переменные

| Переменная | По умолчанию | Описание |
|---|---|---|
| `CROSS_COMPILE` | `riscv64-unknown-elf-` | Префикс кросс-компилятора |
| `BOOT_SRC` | `src/boot.S` | Stage 1 |
| `SIMON_SRC` | `src/simon.S` | Stage 2 |
| `FS_FILES` | `payload.hex0 payload.hex1` | Файлы, дописываемые в образ |
| `OUTPUT_IMAGE` | `image_qemu.bin` | Имя выходного образа |

```bash
make FS_FILES="payload.hex0"
```

## Образ

```
image_qemu.bin = boot.bin + simon.hex0 + payload.hex0 + payload.hex1
```

* **boot** — загрузчик (stage 1): декодирует `simon.hex0` и передаёт управление simon.
* **simon** — минимальное ядро (stage 2): ФС из хвоста образа, hex0/hex1, `do`, UART-CLI.
* **payload** — тестовая stage 3: «Hello, World!» и цикл.

## Формат файлов ФС

Каждый файл в образе начинается со строки `src <имя>` и заканчивается нулевым байтом:

```
src myfile.hex0
<hex0 содержимое>
\0
```

Примеры: [payload.hex0](./payload.hex0), [payload.hex1](./payload.hex1).

## Утилиты

* `bin-to-hex0.sh` — бинарник → hex0
* `hex0.sh` — hex0 → бинарник
* `elf2hex1.sh` — ELF → черновик hex1 (метки прыжков правятся руками)
* `add_null.sh` — нулевой байт в конец файла
