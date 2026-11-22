#!/bin/sh
set -eu
mkdir -p /build/sorttable-probe
cd /build/sorttable-probe
gcc -c -mno-relax /steps/tests/sorttable.S -o fixture.o
ld --no-relax -T /steps/tests/sorttable.ld fixture.o -o fixture
gcc -static -O2 /steps/tests/sorttable-check.c -o check
/build/linux/scripts/sorttable fixture
./check fixture
sha256sum -o sorted.sha256 fixture
/build/linux/scripts/sorttable fixture
sha256sum -c sorted.sha256
if /build/linux/scripts/sorttable missing > missing.log 2>&1; then
    echo 'linux sorttable FAIL accepted missing file'
    exit 1
fi
echo 'linux sorttable sparse ELF and idempotence ok'
