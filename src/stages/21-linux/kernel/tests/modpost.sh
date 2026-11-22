#!/bin/sh
set -eu
mkdir -p /build/modpost-probe
cd /build/modpost-probe
printf 'int aegis_modpost_probe(void) { return 42; }\n' > probe.c
gcc -c -O2 -ffreestanding -fno-stack-protector -fno-pie -fno-asynchronous-unwind-tables -fno-unwind-tables probe.c -o vmlinux.o
sha256sum -o object.sha256 vmlinux.o
/build/linux/scripts/mod/modpost -o symbols vmlinux.o
sha256sum -c object.sha256
if /build/linux/scripts/mod/modpost missing.o > missing.log 2>&1; then
    echo 'linux modpost FAIL accepted missing object'
    exit 1
fi
echo 'linux modpost real ELF read ok'
