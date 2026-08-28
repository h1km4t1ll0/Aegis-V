# Boards

Board addresses and boot mode live outside `src/stages/0-simon/src/*.S`. To port Simon, add a pair of files (`*.h` + `*.sh`) and build with `./src/scripts/image.sh <name>`.

```text
src/scripts/image.sh                              host seed (qemu | lichee)
src/boot-targets/qemu/qemu_virt.*                 QEMU virt (default)
src/boot-targets/lichee/lichee_th1520_fullflash.* Lichee Pi 4A / TH1520
src/boot-targets/lichee/firmware/                 U-Boot SPL for Lichee
src/stages/0-simon/                               prep: shared boot + Simon
src/stages/                                       ladder (00-hex0 …)
src/scripts/                                      pack.sh, image.sh, simon.py, kronos.py
build/                                            packed files.pl (gitignored)
```

## QEMU virt

```bash
./src/scripts/pack.sh
./src/scripts/image.sh qemu
python3 -u src/scripts/simon.py
```

Image: `src/stages/0-simon/artifacts/image.bin`. Checkpoints: `src/stages/<phase>/artifacts/image.bin`.

```bash
./src/scripts/image.sh run
```

## Lichee Pi 4A

Full-flash image:

```bash
./src/scripts/image.sh clean
./src/scripts/image.sh lichee
```

`TOOLCHAIN=clang` if you want llvm instead of gcc.

Outputs:

```text
build/lichee/artifacts/image.bin            raw payload
build/lichee/artifacts/flash_me_lichee.bin  whole-card image for dd
```

Write to the **card**, not a partition:

```bash
sudo dd if=build/lichee/artifacts/flash_me_lichee.bin of=/dev/sdX bs=4M status=progress conv=fsync
sync
```
