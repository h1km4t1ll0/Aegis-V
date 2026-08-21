#!/bin/bash
# Native-speed Mes VM on the host. Same eval-apply.c / gc.c / Scheme as
# qemu mes.bin — reproduces STACK FULL in minutes, not hours.
# Guile is the wrong tool: it will not hit Mes's stack.
#
# Default: native clang/gcc (macOS or Linux). Docker is optional:
#   HOST_MES_DOCKER=1 ./scripts/host_mes_debug.sh tccpp
#
# Usage:
#   ./scripts/host_mes_debug.sh              # hello + tco + probe + hi.c + tccpp.c
#   ./scripts/host_mes_debug.sh tccpp         # only mescc -S tccpp.c
#   ./scripts/host_mes_debug.sh tco           # only tco-probe.scm
#   ./scripts/host_mes_debug.sh -- ./mes.bin hello.scm

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MES="$ROOT/stages/07-mes/vendor/mes"
NYACC="$ROOT/stages/07-mes/vendor/nyacc"
MESDIR="$ROOT/stages/07-mes"
TCC="$ROOT/stages/08-tcc"
FS="$ROOT/build/host-fs"
IMAGE="${HOST_MES_IMAGE:-gcc:13-bookworm}"
USE_DOCKER="${HOST_MES_DOCKER:-0}"

# bash cannot `export %prefix=...`; mescc reads these via getenv.
MES_RUN_ENV=(
  "MES_PREFIX=$MES"
  "GUILE_LOAD_PATH=$NYACC/module"
  "MES_STACK=${MES_STACK:-1000000}"
  "MES_ARENA=${MES_ARENA:-35000000}"
  "MES_MAX_ARENA=${MES_MAX_ARENA:-35000000}"
  "includedir=$FS/include"
  "%prefix=$FS"
  "%arch=riscv64"
  "%kernel=linux"
  "PATH=$FS:${PATH}"
)

run_with_mes_env() {
  env "${MES_RUN_ENV[@]}" bash --noprofile --norc -c "$*"
}

stage_fs() {
  mkdir -p "$FS"
  ln -sfn "$MES/mes" "$FS/mes"
  ln -sfn "$MES/module" "$FS/module"
  ln -sfn "$MES/include" "$FS/include"
  ln -sfn "$MESDIR/hello.scm" "$FS/hello.scm"
  ln -sfn "$MESDIR/tco-probe.scm" "$FS/tco-probe.scm"
  ln -sfn "$MESDIR/probe.scm" "$FS/probe.scm"
  ln -sfn "$MESDIR/hi.c" "$FS/hi.c"
  ln -sfn "$MESDIR/mescc.scm" "$FS/mescc.scm"
  ln -sfn "$MESDIR/tcc-mescc-boot.scm" "$FS/tcc-mescc-boot.scm"
  ln -sfn "$MESDIR/tcc-all.scm" "$FS/tcc-all.scm"
  ln -sfn "$MESDIR/tcc-p0.scm" "$FS/tcc-p0.scm"
  ln -sfn "$MESDIR/tcc-p1.scm" "$FS/tcc-p1.scm"
  ln -sfn "$MESDIR/tcc-p2.scm" "$FS/tcc-p2.scm"
  ln -sfn "$MESDIR/tcc-p3.scm" "$FS/tcc-p3.scm"
  for f in tcc.c tcc.h libtcc.c tccpp.c tccgen.c tccelf.c tccasm.c tccrun.c \
           tcctools.c riscv64-gen.c riscv64-link.c riscv64-asm.c riscv64-tok.h \
           tcctok.h tcclib.h i386-asm.c elf.h stab.h stab.def libtcc.h \
           config.h bootstrap.h; do
    if [ -e "$TCC/$f" ]; then
      ln -sfn "$TCC/$f" "$FS/$f"
    fi
  done
}

docker_sh() {
  docker run --rm \
    -v "$ROOT:$ROOT" \
    -w "$FS" \
    -e MES_PREFIX="$MES" \
    -e GUILE_LOAD_PATH="$NYACC/module" \
    -e MES_STACK="${MES_STACK:-1000000}" \
    -e MES_ARENA="${MES_ARENA:-35000000}" \
    -e MES_MAX_ARENA="${MES_MAX_ARENA:-35000000}" \
    -e includedir="$FS/include" \
    -e "%prefix=$FS" \
    -e "%arch=riscv64" \
    -e "%kernel=linux" \
    "$IMAGE" \
    bash -lc "$*"
}

build_mes_native() {
  local cc="${CC:-}"
  if [ -z "$cc" ]; then
    if command -v clang >/dev/null 2>&1; then
      cc=clang
    else
      cc=gcc
    fi
  fi
  (
    cd "$MES"
    mkdir -p bin
    if [ ! -x bin/mes-gcc ] || [ src/gc.c -nt bin/mes-gcc ] \
        || [ src/eval-apply.c -nt bin/mes-gcc ] || [ src/mes.c -nt bin/mes-gcc ] \
        || [ include/mes/lib.h -nt bin/mes-gcc ] || [ src/reader.c -nt bin/mes-gcc ] || [ src/gc.c -nt bin/mes-gcc ]; then
      echo "[host-mes] compiling mes-gcc with $cc (SYSTEM_LIBC, same VM as qemu)"
      extra=""
      if [ "$(uname)" = Darwin ]; then
        extra="-Wl,-stack_size,0x2000000"
      fi
      make -f simple.make bin/mes-gcc CC="$cc" \
        CFLAGS="-D _GNU_SOURCE -D const= -O2 -g -D SYSTEM_LIBC=1 -D 'MES_VERSION=\"git\"' -D 'MES_PKGDATADIR=\"/usr/local/share/mes\"' -I include $extra"
    else
      echo "[host-mes] mes-gcc up to date"
    fi
  )
  cp -f "$MES/bin/mes-gcc" "$FS/mes.bin"
}

build_mes_docker() {
  docker_sh "set -e
    cd '$MES'
    mkdir -p bin
    elf=0
    if [ -x bin/mes-gcc ]; then
      sig=\$(od -An -t x1 -N 4 bin/mes-gcc | tr -d ' \n')
      if [ \"\$sig\" = '7f454c46' ]; then
        elf=1
      fi
    fi
    if [ \"\$elf\" != 1 ] || [ src/gc.c -nt bin/mes-gcc ] || [ src/eval-apply.c -nt bin/mes-gcc ] || [ src/mes.c -nt bin/mes-gcc ] || [ src/reader.c -nt bin/mes-gcc ]; then
      echo '[host-mes] compiling mes-gcc (Linux ELF, SYSTEM_LIBC, same VM as qemu)'
      if [ \"\$elf\" != 1 ]; then
        rm -f bin/mes-gcc
      fi
      make -f simple.make bin/mes-gcc
    else
      echo '[host-mes] mes-gcc up to date'
    fi
    cp -f bin/mes-gcc '$FS/mes.bin'
  "
}

run_native() {
  (cd "$FS" && run_with_mes_env "$*")
}

run_docker() {
  docker_sh "set -e
    export PATH='$FS':\$PATH
    $*
  "
}

stage_fs
if [ "$USE_DOCKER" = 1 ]; then
  build_mes_docker
  run_in_fs() { run_docker "$@"; }
else
  build_mes_native
  run_in_fs() { run_native "$@"; }
fi

cmd="${1:-all}"
case "$cmd" in
  --)
    shift
    run_in_fs "$*"
    ;;
  tco)
    run_in_fs "./mes.bin tco-probe.scm"
    ;;
  probe)
    run_in_fs "./mes.bin probe.scm"
    ;;
  hi)
    run_in_fs "./mes.bin -e main mescc.scm -- -S -o hi.s hi.c && echo mescc-ok hi.c"
    ;;
  tccpp)
    run_in_fs "./mes.bin -e main mescc.scm -- -S -o tccpp.s tccpp.c && echo mescc-ok tccpp.c"
    ;;
  all)
    run_in_fs "
      set -e
      echo '=== hello ==='
      ./mes.bin hello.scm
      echo '=== tco-probe ==='
      ./mes.bin tco-probe.scm
      echo '=== nyacc probe ==='
      ./mes.bin probe.scm
      echo '=== mescc hi.c ==='
      ./mes.bin -e main mescc.scm -- -S -o hi.s hi.c
      echo mescc-ok hi.c
      echo '=== mescc tccpp.c (STACK FULL lives here in qemu) ==='
      ./mes.bin -e main mescc.scm -- -S -o tccpp.s tccpp.c
      echo mescc-ok tccpp.c
    "
    ;;
  *)
    echo "usage: $0 [all|tco|probe|hi|tccpp|-- <cmd>]" >&2
    exit 2
    ;;
esac
