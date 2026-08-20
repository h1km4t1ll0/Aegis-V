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

## Автотест Mes/NYACC на плате

После загрузки Simon закрой serial-терминал и запусти runner на Linux-компьютере:

```bash
python3 -u scripts/run_board_nyacc_test.py --port /dev/ttyUSB0 --baud 115200
```

Он ждёт `Simon says~:`, поочерёдно собирает все стадии до GNU Mes/NYACC, проверяет
`Hello,M2-mes!` и `pprint-ok`, а затем останавливается. TCC не запускается. Более сильная
проверка разбора `hi.c` через mescc/NYACC:

```bash
python3 -u scripts/run_board_nyacc_test.py --port /dev/ttyUSB0 --mescc-smoke
```

`--user-code-base` по умолчанию равен `0x82000000`: эта база зашита в ранний
`hex2_riscv64.hex1` и совпадает с `USER_CODE_BASE` в профилях QEMU и Lichee.

Посмотреть последовательность без подключения к UART:

```bash
python3 scripts/run_board_nyacc_test.py --dry-run
```

## Проверка доступной DDR на Lichee

В Simon есть разрушительная команда:

```text
memtest <start-hex> <end-hex> <step-hex>
```

Она записывает в каждое выбранное 64-битное слово зависящий от адреса шаблон,
сразу читает его обратно, а затем делает второй проход чтения. Второй проход
нужен, чтобы обнаруживать не только битые данные, но и алиасинг адресов. `end`
не входит в диапазон.

Запуск с хоста, например для разреженной проверки от начала свободного heap до
4 ГиБ адресного пространства с шагом 1 МиБ:

```bash
python3 -u scripts/run_board_memory_test.py \
  --port /dev/ttyUSB0 \
  --start 0x44000000 \
  --end 0x100000000 \
  --step 0x100000 \
  --json-report lichee-memory.json
```

Возможные результаты:

```text
[memtest] PASS
[memtest] FAULT address=0x... cause=0x... phase=...
[memtest] MISMATCH address=0x... expected=0x... actual=0x...
```

При `FAULT` runner возвращает код 1 и показывает точный проверявшийся адрес из
`mtval`/`stval`, а также диапазон доступного объёма DDR, посчитанный от физической
базы DDR TH1520 `0x0`. Адрес `0x40000000` — это место загрузки Simon, а не начало
DDR. Ширина диапазона равна шагу проверки. Для уточнения
границы плату надо перезагрузить и повторить тест вокруг найденного адреса с
меньшим шагом, например `--step 0x1000`, затем `--step 0x8`. Шаг `0x8` проверяет
каждое 64-битное слово и позволяет получить точный объём; большой шаг нужен
только для быстрого поиска примерной границы.

Общий объём выводится только для непрерывного прохода, начинающегося с
`0x44000000`. Изолированное окно около предполагаемой границы проверяет только
это окно и само по себе не исключает алиасинг на адрес ниже начала окна.

Важно: тест намеренно перезаписывает все выбранные шагом слова (при шаге `0x8` —
весь диапазон). Нижняя граница в runner ограничена `0x44000000`, чтобы не затереть
Simon и встроенные файлы, но выше могут находиться данные U-Boot или другие
зарезервированные области платы. Запускай тест сразу после загрузки Simon и
обязательно перезагружай плату перед NYACC или следующим проходом.

## Тест многопоточности планировщика

Команда `threadtest` создаёт четыре kernel-потока. Каждый поток 4096 раз атомарно
увеличивает общий счётчик и после каждой итерации вызывает добровольное
переключение контекста. Дополнительно проверяются индивидуальные счётчики всех
потоков и точное число переключений.

Запуск с хоста:

```bash
python3 -u scripts/run_board_thread_test.py --port /dev/ttyUSB0
```

Ожидаемый UART-маркер:

```text
[threadtest] PASS mode=normal threads=4 iterations=4096 yield_every=1 counter=16384 switches=16384 hart=0
```

Нагрузочный профиль выполняет 4 194 304 атомарных инкремента, проверяет
контрольную сумму каждого потока и делает 65 536 переключений контекста:

```bash
python3 -u scripts/run_board_thread_test.py --port /dev/ttyUSB0 --stress
```

Ожидаемый маркер:

```text
[threadtest] PASS mode=stress threads=4 iterations=1048576 yield_every=64 counter=4194304 switches=65536 hart=0
```

Это тест кооперативной многопоточности и сохранения контекста на одном hart.
Текущий full-flash запускает только boot hart и не поднимает secondary hart'ы,
поэтому этот результат нельзя считать тестом четырёхъядерного SMP.

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
./scripts/pack.sh --without-tcc
make clean
make BOARD=lichee_th1520_fullflash TOOLCHAIN=gcc \
  CROSS_COMPILE=/opt/riscv/bin/riscv64-unknown-elf- fullflash
```

На выходе:

```text
build/image_simon_lichee.bin   raw payload
build/flash_me_lichee.bin      полный образ для dd в начало SD
```

Образ без TCC занимает меньше места, но сохраняет всю цепочку до Mes/NYACC. Fullflash теперь
имеет размер 8 МиБ: U-Boot/SPL лежит с `0x000000`, uImage Simon — с `0x100000`.

Лить старым способом:

```bash
sudo dd if=build/flash_me_lichee.bin of=/dev/sdX bs=4M status=progress conv=fsync
sync
```

Важно: `/dev/sdX` — вся SD-карта, не раздел.

После загрузки U-Boot выбери SD (`mmc 1` в штатном U-Boot Lichee) и запусти raw uImage из fullflash:

```text
mmc dev 1
mmc read 0x30000000 0x800 0x1c00
bootm 0x30000000
```

Точное число блоков сборка печатает после строки `U-Boot raw flash:`; его нужно использовать вместо
примерного `0x1c00`.

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
