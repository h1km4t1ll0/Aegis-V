#!/bin/bash
# Pack src/stages/ into files.pl. Names inside the image stay the same.
# No network, no python.

set -e

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
mkdir -p "$ROOT/build/simon/src"

S="$ROOT/src/stages"
HEX0="$S/00-hex0/src"
HEX0T="$S/00-hex0/tests"
HEX1="$S/01-hex1/src"
HEX1T="$S/01-hex1/tests"
HEX2="$S/02-hex2/src"
HEX2T="$S/02-hex2/tests"
M0="$S/03-m0/src"
M0T="$S/03-m0/tests"
CC="$S/04-cc/src"
CCT="$S/04-cc/tests"
M2="$S/05-m2/src"
TOOLS="$S/06-tools/src"
TCC="$S/07-tcc/src"
PNUTDIR="$S/08-pnut/src"
PNUT="$PNUTDIR/vendor/pnut"
CPU=riscv64

if [ ! -f "$TCC/tcc.c" ]; then
  echo "missing TinyCC sources under src/stages/07-tcc/src" >&2
  exit 1
fi
if [ ! -f "$PNUT/pnut.c" ] || [ ! -f "$PNUT/exe.c" ]; then
  echo "missing vendored pnut under src/stages/08-pnut/src/vendor/pnut" >&2
  exit 1
fi

emit() {
  echo "src $1"
  tr -d '\r' < "$2"
  printf '\0'
}

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
  cat "$HEX0T/hello_hex0.hex0"
  printf '\0'

  echo "src testB.hex1"
  cat "$HEX1T/hello_hex1.hex1"
  printf '\0'

  echo "src testC.hex2"
  cat "$HEX2T/hello_hex2.hex2"
  printf '\0'

  echo "src testD.M0"
  cat "$M0T/hello_m0.M0"
  printf '\0'

  echo "src testE.M1"
  cat "$M0T/hello_m1.M1"
  printf '\0'

  echo "src testF.c"
  tr -d '\r' < "$CCT/hello_cc.c"
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

  # Guest pnut #include "tcc.c" from TCC.c compiled to an empty ELF (same
  # bytes as hi.c). Splice quoted includes here so pnut never needs open().
  # Do not also emit the individual tcc sources: that doubled files.pl and
  # overlapped STAGE2_BASE (PAYLOAD_OFFSET 8MB).
  echo "src pnut_inc.c"
  printf '%s\n' '#include "hi.c"'
  printf '\0'

  echo "src pnut_stubs.c"
  printf '%s\n' \
    '#define ONE_SOURCE 1' \
    'typedef int FILE;' \
    'typedef long size_t;' \
    'typedef unsigned char uint8_t;' \
    'typedef unsigned int uint32_t;' \
    '#define NULL 0' \
    '#define volatile' \
    '#define __attribute__(x)' \
    'int main(int argc, char **argv) { return 77; }'
  printf '\0'

  echo "src pnut_8k.c"
  python3 - <<'PY'
import sys
sys.stdout.write("/* " + ("x" * 5000) + " */\n")
sys.stdout.write("int main(int argc, char **argv) { return 88; }\n")
PY
  printf '\0'

  echo "src pnut_tcc.c"
  python3 - "$TCC" <<'PY'
import os
import re
import sys

tcc_dir = sys.argv[1]
inc_re = re.compile(
    r'^[ \t]*#[ \t]*include[ \t]*"([^"]+)"[ \t]*(?:/\*.*\*/)?[ \t]*$'
)
seen_h = set()
# X-macro headers: TCC includes them more than once with a different DEF().
MULTI_INC = {
    "tcctok.h",
    "i386-tok.h",
    "riscv64-tok.h",
    "il-opcodes.h",
    "stab.def",
}


def splice(path):
    name = os.path.basename(path)
    ext = os.path.splitext(name)[1]
    if ext in (".h", ".def") and name in seen_h and name not in MULTI_INC:
        return "/* already: %s */\n" % name
    if ext in (".h", ".def") and name not in MULTI_INC:
        seen_h.add(name)
    chunks = ["/* ==== %s ==== */\n" % name]
    with open(path, "r", encoding="latin1", newline="\n") as fh:
        for line in fh:
            line = line.replace("\r", "")
            m = inc_re.match(line.rstrip("\n"))
            if m:
                inc_path = os.path.join(tcc_dir, m.group(1))
                if os.path.isfile(inc_path):
                    chunks.append(splice(inc_path))
                    continue
            chunks.append(line)
    return "".join(chunks)


sys.stdout.write(
    "\n".join(
        [
            "#define ONE_SOURCE 1",
            "typedef int FILE;",
            "typedef long size_t;",
            "typedef long ssize_t;",
            "typedef long off_t;",
            "typedef long time_t;",
            "typedef long clock_t;",
            "typedef int jmp_buf;",
            "typedef unsigned char uint8_t;",
            "typedef unsigned short uint16_t;",
            "typedef unsigned int uint32_t;",
            "typedef unsigned long uint64_t;",
            "typedef signed char int8_t;",
            "typedef short int16_t;",
            "typedef int int32_t;",
            "typedef long int64_t;",
            "typedef unsigned long uintptr_t;",
            "typedef long intptr_t;",
            "#define __intptr_t_defined 1",
            "#define __MES_INTPTR_T 1",
            "#define __int8_t_defined 1",
            "typedef int mode_t;",
            "typedef int pid_t;",
            "typedef int sigjmp_buf;",
            "#define NULL 0",
            "#define EOF (-1)",
            "#define SEEK_SET 0",
            "#define SEEK_CUR 1",
            "#define SEEK_END 2",
            "#define O_RDONLY 0",
            "#define O_WRONLY 1",
            "#define O_RDWR 2",
            "#define O_CREAT 64",
            "#define O_TRUNC 512",
            "#define O_BINARY 0",
            "#define assert(x)",
            "#define va_list int",
            "#define va_start(a,b)",
            "#define va_end(a)",
            "#define va_arg(a,t) ((t)0)",
            "#define __attribute__(x)",
            "#define volatile",
            "#define restrict",
            "#define __restrict",
            "#define offsetof(t,m) 0",
            "#define HAVE_FLOAT 0",
            "",
        ]
    )
)
libc_path = os.path.join(
    os.path.dirname(os.path.dirname(tcc_dir)), "08-pnut", "src", "pnut_tcc_libc.c"
)
if not os.path.isfile(libc_path):
    libc_path = os.path.join(tcc_dir, "..", "..", "08-pnut", "src", "pnut_tcc_libc.c")
with open(libc_path, "r", encoding="latin1", newline="\n") as fh:
    sys.stdout.write(fh.read().replace("\r", ""))
    sys.stdout.write("\n")
src = splice(os.path.join(tcc_dir, "tcc.c"))
# pnut-exe (M2) may treat `#if HAVE_FLOAT` as true even when the macro is 0,
# then chokes on float literals like 4294967296.0. Force the blocks off.
src = re.sub(r"#if HAVE_FLOAT\b", "#if 0", src)
src = re.sub(r"#elif HAVE_FLOAT\b", "#elif 0", src)
# M2-built pnut does not eat C integer suffixes; `1ULL` becomes IDENTIFIER `ULL`.
src = re.sub(
    r"(0[xX][0-9a-fA-F]+|\d+)(?:[uU][lL][lL]|[lL][lL][uU]|[uU][lL]|[lL][uU]|[lL][lL]|[uU]|[lL])\b",
    r"\1",
    src,
)
# pnut ignores leftover #include; real TCC does not. Drop them so tcc-pnut
# can compile this amalgamation.
src = re.sub(r'^[ \t]*#[ \t]*include[ \t]*[<"].*$', "/* include stripped */", src, flags=re.M)
sys.stdout.write(src)
PY
  printf '\0'

  emit simon_crt.s "$PNUTDIR/simon_crt.s"
  emit tcc_crt.s "$PNUTDIR/tcc_crt.s"
  emit tcc_hi.rsp "$PNUTDIR/tcc_hi.rsp"
  emit tcc_self.rsp "$PNUTDIR/tcc_self.rsp"
  emit tcc_host_libc.c "$PNUTDIR/tcc_host_libc.c"

  # Real TCC 0.9.26 tree for tcc-pnut self-host (not the pnut amalgamation).
  for f in tcc.c libtcc.c tccpp.c tccgen.c tccelf.c tccasm.c \
           riscv64-gen.c riscv64-link.c riscv64-asm.c \
           tcc.h libtcc.h tcclib.h config.h bootstrap.h \
           tcctok.h riscv64-tok.h elf.h stab.h stab.def; do
    emit "$f" "$TCC/$f"
  done
  for f in stddef.h stdarg.h stdio.h stdlib.h string.h errno.h math.h \
           fcntl.h setjmp.h time.h unistd.h assert.h inttypes.h stdint.h \
           limits.h ctype.h; do
    emit "$f" "$PNUTDIR/tcc-inc/$f"
  done
  emit "sys/time.h" "$PNUTDIR/tcc-inc/sys_time.h"

  echo "src hi.c"
  printf '%s\n' 'int main(int argc, char **argv) { (void)argc; (void)argv; return 0; }'
  printf '\0'

  # Concatenate pnut-exe. Splice quoted .c includes here (pack time).
  # Angle-bracket libc headers stay; M2 eats them unless --expand-includes.
  # No host compiler.
  pnut_splice() {
    local f="$1"
    local line
    while IFS= read -r line || [ -n "$line" ]; do
      line="${line%$'\r'}"
      case "$line" in
        *'#include "x86.c"'*)
          ;;
        *'#include "riscv64.c"'*)
          if [ -z "$PNUT_DID_RV" ]; then
            PNUT_DID_RV=1
            pnut_splice "$PNUT/riscv64.c"
          fi
          ;;
        *'#include "exe.c"'*)
          if [ -z "$PNUT_DID_EXE" ]; then
            PNUT_DID_EXE=1
            pnut_splice "$PNUT/exe.c"
          fi
          ;;
        *'#include "env.c"'*)
          if [ -z "$PNUT_DID_ENV" ]; then
            PNUT_DID_ENV=1
            pnut_splice "$PNUT/env.c"
          fi
          ;;
        *'#include "elf.c"'*)
          if [ -z "$PNUT_DID_ELF" ]; then
            PNUT_DID_ELF=1
            pnut_splice "$PNUT/elf.c"
          fi
          ;;
        *'#include "doc/'*|*'#include "debug.c"'*|*'#include "sh.c"'*|*'#include "awk.c"'*|*'#include "arm.c"'*|*'#include "mach-o.c"'*)
          ;;
        *'int fd = -1;'*)
          printf '%s\n' 'int fd;'
          ;;
        *'char string_pool[STRING_POOL_SIZE];'*)
          printf '%s\n' 'char *string_pool;'
          ;;
        *'intptr_t heap[HEAP_SIZE];'*)
          printf '%s\n' 'intptr_t *heap;'
          ;;
        *'int code[CODE_SIZE];'*)
          printf '%s\n' 'int *code;'
          ;;
        *'int labels[LABELS_ARR_SIZE];'*)
          printf '%s\n' 'int *labels;'
          ;;
        *'intptr_t include_stack[INCLUDE_STACK_DEPTH * INCLUDE_ENTRY_SIZE];'*)
          printf '%s\n' 'intptr_t *include_stack;'
          ;;
        *'int if_macro_stack[IFDEF_DEPTH_MAX];'*)
          printf '%s\n' 'int *if_macro_stack;'
          ;;
        *'int macro_stack[MACRO_RECURSION_MAX];'*)
          printf '%s\n' 'int *macro_stack;'
          ;;
        *'char write_buf[1];'*)
          printf '%s\n' 'char *write_buf;'
          ;;
        *'char read_buf[1];'*)
          printf '%s\n' 'char *read_buf;'
          ;;
        *'int val_32[2];'*)
          printf '%s\n' 'int *val_32;'
          ;;
        *'#elif defined(target_i386_linux)'*)
          printf '%s\n' '#elif defined(target_i386_linux)'
          ;;
        *'init_ident_table();'*)
          printf '%s\n' '  pnut_alloc_bss();' '  init_ident_table();'
          ;;
        *)
          printf '%s\n' "$line"
          ;;
      esac
    done < "$f"
  }
  echo "src PNUT.c"
  # FILE is an M2 primitive only in --bootstrap-mode; pnut needs full M2.
  # ast/bool are #define aliases in pnut; M2 type_name does not expand them.
  printf '%s\n' \
    'typedef int FILE;' \
    'typedef int ast;' \
    'typedef int bool;'
  tr -d '\r' < "$M2/linux_bootstrap.c"
  printf '\n'
  tr -d '\r' < "$M2/bootstrap_libc.c"
  printf '\n'
  tr -d '\r' < "$PNUTDIR/pnut_host.c"
  printf '\n'
  printf '%s\n' \
    '#define target_riscv64_linux 1' \
    '#define target_exe 1' \
    '#define ONE_PASS_GENERATOR 1' \
    '#define BOOTSTRAP_TCC 1' \
    '#define NO_TERNARY_SUPPORT 1' \
    '#define intptr_t long' \
    '#define O_RDONLY 0' \
    '#define O_WRONLY 1' \
    '#define O_CREAT 64' \
    '#define O_TRUNC 512' \
    '#define const' \
    '#define extern'
  pnut_splice "$PNUT/pnut.c"
  printf '\n'
  tr -d '\r' < "$PNUTDIR/pnut_bss.c"
  printf '\0'
  echo "src CHECK.c"
  printf '%s\n' 'typedef int FILE;'
  tr -d '\r' < "$M2/linux_bootstrap.c"
  printf '\n'
  tr -d '\r' < "$PNUTDIR/checkelf.c"
  printf '\0'
) > "$ROOT/build/simon/src/files.pl"
python3 "$ROOT/src/scripts/lib/phases.py" unpack
