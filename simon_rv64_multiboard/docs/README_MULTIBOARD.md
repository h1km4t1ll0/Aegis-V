# Simon multi-board build

Теперь платные адреса и режим запуска вынесены из `kernel/*.S` в отдельные конфиги.
Чтобы перенести Simon на другую RISC-V плату, обычно надо менять только файлы в `boards/`.

## Структура

```text
config.mk                         общий выбор платы
boards/lichee_th1520_fullflash.mk build-настройки Личи
boards/lichee_th1520_fullflash.h  адреса/CSR/UART/heap Личи
boards/banana_pi_bpi_f3_k1x.mk    build-настройки Banana Pi
boards/banana_pi_bpi_f3_k1x.h     адреса/CSR/UART/heap Banana Pi
boards/qemu_virt.*                шаблон для QEMU
kernel/boot.S                     общий stage1
kernel/simon.S                    общий Simon
stages/                           seed bootstrap (см. README.md)
scripts/                          pack, QEMU, hex0, fullflash
firmware/                         U-Boot SPL для Личи
build/                            образы и промежуточные .bin
```

## QEMU

```bash
./scripts/pack.sh
make BOARD=qemu_virt TOOLCHAIN=gcc CROSS_COMPILE=riscv64-elf- image_qemu.bin
python3 -u scripts/run_m2_qemu_test.py
```

Образ: `build/image_qemu.bin`.

## Выбор платы

По умолчанию стоит:

```make
BOARD ?= lichee_th1520_fullflash
```

Можно собрать явно:

```bash
make BOARD=lichee_th1520_fullflash TOOLCHAIN=clang
make BOARD=banana_pi_bpi_f3_k1x TOOLCHAIN=clang
```

## Личи / Lichee Pi 4A

Полная сборка full-flash:

```bash
make clean
make BOARD=lichee_th1520_fullflash TOOLCHAIN=clang fullflash
```

На выходе:

```text
build/image_simon_lichee.bin   raw payload
build/flash_me_lichee.bin      полный образ для dd в начало SD
```

Лить старым способом:

```bash
sudo dd if=build/flash_me_lichee.bin of=/dev/sdX bs=4M status=progress conv=fsync
sync
```

Важно: `/dev/sdX` — вся SD-карта, не раздел.

## Banana Pi BPI-F3 / K1-X

Сборка raw payload:

```bash
make clean
make BOARD=banana_pi_bpi_f3_k1x TOOLCHAIN=clang
```

На выходе:

```text
build/image_simon_work.bin
```

Кладём в bootfs как раньше:

```bash
sudo mount /dev/sdX5 /mnt/bootfs
sudo cp build/image_simon_work.bin /mnt/bootfs/image_simon_work.bin
sync
sudo umount /mnt/bootfs
```

Запуск в U-Boot:

```text
wdt dev PMIC_WDT
wdt stop
ext4load mmc 0:5 0x10000000 image_simon_work.bin
go 0x10000000
```

## Что менять для новой платы

Создай два файла:

```text
boards/my_board.mk
boards/my_board.h
```

В `.mk` меняется:

```make
DDR_ADDR := 0x...
YOUR_RAW_BIN := image_simon_my_board.bin
SUPPORTS_FULLFLASH := 0 или 1
```

В `.h` меняется:

```c
#define BOARD_TRAP_MODE BOARD_TRAP_M или BOARD_TRAP_S
#define UART0_BASE      ...
#define UART_LSR        ...
#define LSR_RX_READY    ...
#define LSR_TX_IDLE     ...
#define IMAGE_SCAN_BASE ...
#define PROC_BASE       ...
#define FILE_NAME_BASE  ...
#define FD_BASE         ...
#define FILE_DATA_BASE  ...
#define HEAP_BASE       ...
#define HEAP_END        ...
```

Главная развилка:

```c
#define BOARD_TRAP_MODE BOARD_TRAP_M
```

если payload стартует напрямую в M-mode.

```c
#define BOARD_TRAP_MODE BOARD_TRAP_S
```

если payload стартует через OpenSBI/U-Boot в S-mode.

## Проверочная цепочка в Simon

```text
hex0 hex1.hex0
do hex1.bin hex2.hex1 hex2.bin
do hex2.bin M0.hex2 M0.bin
do M0.bin testD.M0 testD.hex2
do hex2.bin testD.hex2 testD.bin
do testD.bin

do M0.bin M1.M0 M1.hex2
do hex2.bin M1.hex2 M1.bin
do M1.bin testE.M1 testE.hex2
do hex2.bin testE.hex2 testE.bin
do testE.bin
```

Ожидаемый финал:

```text
hello from M0
hello from M1
Simon says~:
```
