# Aegis-V

Минимальный **bootstrap** под RISC-V: лестница от hex0 до TCC 0.9.26, которую можно прочитать и проверить человеком. Образ поднимается в QEMU (`virt`) и на платах Lichee Pi 4A / Banana Pi BPI-F3.

Живое дерево: [`simon_rv64_multiboard/`](simon_rv64_multiboard/).

Истоки: [зимняя школа ylab](https://github.com/ylab-nsu/ws25-bootstrap), [live-bootstrap](https://github.com/fosslinux/live-bootstrap), [stage0-posix-riscv64](https://github.com/oriansj/stage0-posix-riscv64).

## Сборка (QEMU)

Зависимости: `riscv64-elf-gcc` (macOS Homebrew) или `riscv64-unknown-elf-gcc` (Linux), `qemu-system-riscv64`, `make`.

```bash
cd simon_rv64_multiboard
./scripts/pack.sh
make BOARD=qemu_virt TOOLCHAIN=gcc CROSS_COMPILE=riscv64-elf- image_qemu.bin
python3 -u scripts/run_m2_qemu_test.py
```

Образ: `build/image_qemu.bin`. Промпт: `Simon says~:`.

На Linux префикс обычно `riscv64-unknown-elf-`. Выход из QEMU вручную: **Ctrl+A**, затем **X**.

## Лестница

```
hex0 → hex1 → hex2 → M0 → cc → M2-Planet
     → blood-elf / M1 / hex2
     → GNU Mes + mescc
     → TCC 0.9.26
```

`boot` — единственный бинарник «из хостового компилятора». Дальше simon собирает всё сам из seed в образе.

## Куда смотреть

| Хочу… | Иди сюда |
|---|---|
| Собрать и прогнать лестницу | этот файл, секция «Сборка» |
| Карта стадий и каталогов | [`simon_rv64_multiboard/README.md`](simon_rv64_multiboard/README.md) |
| Платы (QEMU / Lichee / Banana) | [`simon_rv64_multiboard/docs/README_MULTIBOARD.md`](simon_rv64_multiboard/docs/README_MULTIBOARD.md) |
| Ядро | [`simon_rv64_multiboard/kernel/`](simon_rv64_multiboard/kernel/) |
| Seed стадий | [`simon_rv64_multiboard/stages/`](simon_rv64_multiboard/stages/) |

Старые деревья `builder/` и `builder-lichee/` — архив раннего QEMU/Lichee, не текущая лестница.
