#!/bin/bash
# Pack stages/ into files.pl. Names inside the image stay the same.
# No network, no python.

set -e

INCLUDE_TCC=1
for arg in "$@"; do
  case "$arg" in
    --without-tcc)
      INCLUDE_TCC=0
      ;;
    --with-tcc)
      INCLUDE_TCC=1
      ;;
    *)
      echo "usage: $0 [--with-tcc|--without-tcc]" >&2
      exit 2
      ;;
  esac
done

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p "$ROOT/build"

S="$ROOT/stages"
HEX0="$S/00-hex0"
HEX1="$S/01-hex1"
HEX2="$S/02-hex2"
M0="$S/03-m0"
CC="$S/04-cc"
M2="$S/05-m2"
TOOLS="$S/06-mescc-tools"
MESDIR="$S/07-mes"
MES="$MESDIR/vendor/mes"
NYACC="$MESDIR/vendor/nyacc"
TCC="$S/08-tcc"
CPU=riscv64

if [ ! -d "$MES" ] || [ ! -d "$NYACC" ]; then
  echo "missing vendored sources under stages/07-mes/vendor" >&2
  exit 1
fi
if [ "$INCLUDE_TCC" -eq 1 ] && [ ! -f "$TCC/tcc.c" ]; then
  echo "missing TCC sources under stages/08-tcc" >&2
  exit 1
fi

emit() {
  echo "src $1"
  tr -d '\r' < "$2"
  printf '\0'
}

emit_tree() {
  local root="$1"
  local prefix="$2"
  shift 2
  find "$root" \( -type f -o -type l \) "$@" | LC_ALL=C sort | while IFS= read -r f; do
    [ -f "$f" ] || continue
    rel="${f#"$root"/}"
    if [ -n "$prefix" ]; then
      emit "$prefix/$rel" "$f"
    else
      emit "$rel" "$f"
    fi
  done
}

MES_C_SOURCES="
include/mes/lib-mini.h
include/mes/lib-cc.h
include/mes/lib.h
include/mes/cc.h
lib/linux/${CPU}-mes-m2/crt1.c
lib/mes/__init_io.c
lib/linux/${CPU}-mes-m2/_exit.c
lib/linux/${CPU}-mes-m2/_write.c
lib/mes/globals.c
lib/m2/cast.c
lib/stdlib/exit.c
lib/mes/write.c
include/linux/${CPU}/syscall.h
lib/linux/${CPU}-mes-m2/syscall.c
lib/stub/__raise.c
lib/linux/brk.c
lib/linux/malloc.c
lib/string/memset.c
lib/linux/read.c
lib/mes/fdgetc.c
lib/stdio/getchar.c
lib/stdio/putchar.c
lib/stub/__buffered_read.c
include/errno.h
include/fcntl.h
lib/linux/_open3.c
lib/linux/open.c
lib/mes/mes_open.c
lib/string/strlen.c
lib/mes/eputs.c
lib/mes/fdputc.c
lib/mes/eputc.c
include/time.h
include/sys/time.h
include/m2/types.h
include/sys/types.h
include/sys/utsname.h
include/mes/mes.h
include/mes/builtins.h
include/mes/constants.h
include/mes/symbols.h
lib/mes/__assert_fail.c
lib/mes/assert_msg.c
lib/string/strncmp.c
lib/posix/getenv.c
lib/mes/fdputs.c
lib/mes/ntoab.c
lib/ctype/isdigit.c
lib/ctype/isxdigit.c
lib/ctype/isspace.c
lib/ctype/isnumber.c
lib/mes/abtol.c
lib/stdlib/atoi.c
lib/string/memcpy.c
lib/stdlib/free.c
lib/stdlib/realloc.c
lib/string/strcpy.c
lib/mes/itoa.c
lib/mes/ltoa.c
lib/mes/fdungetc.c
lib/posix/setenv.c
lib/linux/access.c
include/linux/m2/kernel-stat.h
include/sys/stat.h
lib/linux/chmod.c
lib/linux/ioctl3.c
include/sys/ioctl.h
lib/m2/isatty.c
include/signal.h
lib/linux/fork.c
lib/m2/execve.c
lib/m2/execv.c
include/sys/resource.h
lib/linux/wait4.c
lib/linux/waitpid.c
lib/linux/gettimeofday.c
lib/linux/clock_gettime.c
lib/m2/time.c
lib/linux/_getcwd.c
include/limits.h
lib/m2/getcwd.c
lib/linux/dup.c
lib/linux/dup2.c
lib/string/strcmp.c
lib/string/memcmp.c
lib/linux/uname.c
lib/linux/unlink.c
src/builtins.c
src/core.c
src/display.c
src/eval-apply.c
src/gc.c
src/hash.c
src/lib.c
src/m2.c
src/math.c
src/mes.c
src/module.c
src/posix.c
src/reader.c
src/stack.c
src/string.c
src/struct.c
src/symbol.c
src/variable.c
src/vector.c
"

(
  echo "src hex1.hex0"
  cat "$HEX0/hex1_riscv64.hex0"
  printf '\0'

  echo "src hex2.hex1"
  cat "$HEX1/hex2_riscv64.hex1"
  printf '\0'

  echo "src M0.hex2"
  cat "$HEX2/M0_riscv64.hex2"
  printf '\0'

  echo "src M1.M0"
  cat "$M0/M1_riscv64.M0"
  printf '\0'

  echo "src cc.M1"
  cat "$CC/riscv64_defs.M1"
  cat "$CC/cc_riscv64.M1"
  printf '\0'

  echo "src rt.M1"
  cat "$CC/riscv64_defs.M1"
  cat "$CC/libc-core.M1"
  printf '\0'

  echo "src catm.hex2"
  cat "$HEX2/catm_riscv64.hex2"
  printf '\0'

  echo "src testA.hex0"
  cat "$HEX0/hello_hex0.hex0"
  printf '\0'

  echo "src testB.hex1"
  cat "$HEX1/hello_hex1.hex1"
  printf '\0'

  echo "src testC.hex2"
  cat "$HEX2/hello_hex2.hex2"
  printf '\0'

  echo "src testD.M0"
  cat "$M0/hello_m0.M0"
  printf '\0'

  echo "src testE.M1"
  cat "$M0/hello_m1.M1"
  printf '\0'

  echo "src testF.c"
  tr -d '\r' < "$CC/hello_cc.c"
  printf '\0'

  echo "src M2.c"
  tr -d '\r' < "$M2/linux_bootstrap.c"
  printf '\n'
  tr -d '\r' < "$M2/bootstrap_libc.c"
  printf '\n'
  tr -d '\r' < "$M2/cc.h"
  printf '\n'
  tr -d '\r' < "$M2/bootstrappable.c"
  printf '\n'
  tr -d '\r' < "$M2/cc_globals.c"
  printf '\n'
  tr -d '\r' < "$M2/cc_reader.c"
  printf '\n'
  tr -d '\r' < "$M2/cc_strings.c"
  printf '\n'
  tr -d '\r' < "$M2/cc_types.c"
  printf '\n'
  tr -d '\r' < "$M2/cc_emit.c"
  printf '\n'
  tr -d '\r' < "$M2/cc_core.c"
  printf '\n'
  tr -d '\r' < "$M2/cc_macro.c"
  printf '\n'
  tr -d '\r' < "$M2/cc.c"
  printf '\0'

  echo "src BE.c"
  tr -d '\r' < "$M2/linux_bootstrap.c"
  printf '\n'
  tr -d '\r' < "$M2/bootstrap_libc.c"
  printf '\n'
  tr -d '\r' < "$M2/bootstrappable.c"
  printf '\n'
  tr -d '\r' < "$TOOLS/stringify.c"
  printf '\n'
  tr -d '\r' < "$TOOLS/blood-elf.c"
  printf '\0'

  echo "src M1.c"
  tr -d '\r' < "$M2/linux_bootstrap.c"
  printf '\n'
  tr -d '\r' < "$M2/bootstrap_libc.c"
  printf '\n'
  tr -d '\r' < "$M2/bootstrappable.c"
  printf '\n'
  tr -d '\r' < "$TOOLS/stringify.c"
  printf '\n'
  tr -d '\r' < "$TOOLS/M1-macro.c"
  printf '\0'

  echo "src H2.c"
  tr -d '\r' < "$M2/linux_bootstrap.c"
  printf '\n'
  tr -d '\r' < "$M2/bootstrap_libc.c"
  printf '\n'
  tr -d '\r' < "$M2/bootstrappable.c"
  printf '\n'
  tr -d '\r' < "$TOOLS/hex2_stubs.c"
  printf '\n'
  tr -d '\r' < "$TOOLS/hex2_defs.c"
  printf '\n'
  tr -d '\r' < "$TOOLS/hex2_linker.c"
  printf '\n'
  tr -d '\r' < "$TOOLS/hex2_word.c"
  printf '\n'
  tr -d '\r' < "$TOOLS/hex2.c"
  printf '\0'

  if [ "$INCLUDE_TCC" -eq 1 ]; then
    echo "src tcc.c"
    cat "$TCC/bootstrap.h"
    tr -d '\r' < "$TCC/tcc.c"
    printf '\0'
    for f in tcc.h libtcc.c tccpp.c tccgen.c tccelf.c tccasm.c tccrun.c tcctools.c \
      riscv64-gen.c riscv64-link.c riscv64-asm.c riscv64-tok.h tcctok.h tcclib.h \
      i386-asm.c elf.h stab.h stab.def libtcc.h config.h; do
      emit "$f" "$TCC/$f"
    done
  fi

  echo "src MES.c"
  printf '%s\n' '#define __linux__ 1' '#define __riscv64__ 1'
  cat "$MESDIR/config.h"
  printf '\n'
  for rel in $MES_C_SOURCES; do
    printf '\n/* ==== %s ==== */\n' "$rel"
    tr -d '\r' < "$MES/$rel"
    printf '\n'
  done
  printf '\0'

  echo "src mes_rt.M1"
  cat "$MES/lib/m2/${CPU}/${CPU}_defs.M1"
  printf '\n'
  cat "$MES/lib/${CPU}-mes/${CPU}.M1"
  printf '\n'
  cat "$MES/lib/linux/${CPU}-mes-m2/crt1.M1"
  printf '\0'

  echo "src mes_as.M1"
  cat "$MES/lib/m2/${CPU}/${CPU}_defs.M1"
  printf '\n'
  cat "$MES/lib/${CPU}-mes/${CPU}.M1"
  printf '\n'
  printf '\0'

  emit mescc.scm "$MESDIR/mescc.scm"
  emit hello.scm "$MESDIR/hello.scm"
  emit probe.scm "$MESDIR/probe.scm"
  emit hi.c "$MESDIR/hi.c"

  emit_tree "$MES/mes/module" mes/module ! -path '*/nyacc/*'
  emit_tree "$MES/module" module
  emit_tree "$MES/include" include
  emit include/arch/syscall.h "$MES/include/linux/${CPU}/syscall.h"
  emit include/arch/kernel-stat.h "$MES/include/linux/${CPU}/kernel-stat.h"
  emit include/arch/signal.h "$MES/include/linux/${CPU}/signal.h"
  emit include/mes/config.h "$MESDIR/config.h"

  emit_tree "$NYACC/module/nyacc" mes/module/nyacc \( -name '*.scm' -o -name '*.mes' \)
) > "$ROOT/build/files.pl"
