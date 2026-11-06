# 13 — musl 1.1.24 (built by the guest, runs on Tilck)

Inside QEMU, Kronos runs `/bin/tcc7b` (stage 11) to build musl 1.1.24 for riscv64 file by file, as it builds Tilck (stage 12): our `musl-gen` writes the headers musl's `Makefile` generates with sed, then one object per `SOURCES` line, `libc.a`, `crt1.o`/`crti.o`/`crtn.o`, two test programs and `musl-sysroot.tar`. The host only plants files from the pinned archive and this stage, then copies what the guest announces (`snap`). Under Tilck (`src/scripts/tilck.py --test`) the test programs print `hello from musl` and `musl proc ok`.

## Pinned source (`build/sources/`, gitignored)

| What | Pin | Archive | sha256 | License |
|---|---|---|---|---|
| musl | https://musl.libc.org/releases/musl-1.1.24.tar.gz | `musl-1.1.24.tar.gz` | `1370c9a812b2cf2a7d92802510cca0058cc37e66a7bedd70051f0a34015022a3` | MIT |

`src/scripts/kronos.py` checks the sha256, gunzips and reads the tar (byte copies only).

## KFS layout (`collect_kfs_musl()`)

- `/src/musl/<path>`: the archive's `src/`, `arch/riscv64/`, `arch/generic/`, `include/` and `crt/*.c`, with `src/override/` on top.
- `/src/musl/HEADERS`: the archive's `include/*.h`, `include/*/*.h`, `arch/generic/bits/*.h`, `arch/riscv64/bits/*.h` (the wildcards of `make install-headers`; Kronos has no getdents).
- `/src/musl/{SOURCES,cflags.rsp}` from `src/`; `/src/musl/ar.rsp` = `-ar rcs /build/musl/lib/libc.a` and the object of every `SOURCES` line (`/build/musl/obj/<path with / → __>.o`, the naming of `build_objs()`).
- `/src/musl-tools/{musl-gen.c,mkustar.c}`, `/src/musl-tests/{hello-musl.c,musl-proc.c}`.

`kronos.py` prints `[musl] KFS …` with the planted size (about 2.6 MB; the KFS total stays well below `0x3E00000`).

## Guest steps (`09-kronos/src/kmain.c` `build_musl()`, after `tilck done`)

1. `/bin/musl-gen /src/musl /build/musl` (`src/musl-gen.c`, a Kronos program): `obj/include/bits/alltypes.h` (the rules of `tools/mkalltypes.sed` over `arch/riscv64/bits/alltypes.h.in` + `include/alltypes.h.in`), `obj/include/bits/syscall.h` (`syscall.h.in`, then its `__NR_` lines as `SYS_`), `obj/src/internal/version.h` (`#define VERSION "1.1.24"`); then `make install-headers` into `/build/musl/include` (`include/`, `arch/generic/bits/` with `arch/riscv64/bits/` on top, the two generated headers), and `/build/musl/SYSROOT`, the list of the archive members. UART `musl-gen ok`.
2. `build_objs("/src/musl/SOURCES", "@/src/musl/cflags.rsp", "/src/musl/", "/build/musl/obj/", "musl")`: one `tcc7b -c` per line (the Tilck loop, generalised). Each failure prints `musl obj fail <path>`; all built: `musl objs ok`.
3. `/bin/tcc7b @/src/musl/ar.rsp` → `/build/musl/lib/libc.a`. The listfile comes first and holds `-ar`: mob expands `@file` arguments only up to `-ar`, so `-ar rcs libc.a @list` would treat `@list` as a file name. Needs stage 11's `tcctools.c` fix (UART `arlink ok`): before it, the guest-built `libc.a` read as having no symbols.
4. `crt/crt1.c`, `crt/crti.c`, `crt/crtn.c` (riscv64 has no `crt/riscv64/`) with `@cflags.rsp -DCRT` (as the `Makefile`) → `/build/musl/lib/`. UART `musl built`.
5. `tests/hello-musl.c`, `tests/musl-proc.c` → `/build/musl/{hello-musl,musl-proc}`, UART `musl progs built`, `snap mhello|mproc`; `kronos.py` copies them to `artifacts/{hello-musl,musl-proc}.elf`.
6. `/bin/mkustar -e /build/musl/musl-sysroot.tar /build/musl /build/musl/SYSROOT` (`src/mkustar.c`; `-e`: modes from the content, see Our files): `snap msysroot` → `artifacts/musl-sysroot.tar`. Then `musl done`.

A musl program (template of `musl_cc()` in kmain.c):

```
/bin/tcc7b -o OUT -nostdinc -I /build/musl/include -I /lib/tcc7/include -nostdlib -static -Wl,-Ttext=0x10000 \
  /build/musl/lib/crt1.o /build/musl/lib/crti.o SRC... /src/tcc7/lib/lib-arm64.c /build/musl/lib/libc.a /build/musl/lib/crtn.o
```

`lib-arm64.c` (tcc's runtime) gives the binary128 helpers (`__addtf3`, `__multf3`, `__extenddftf2`, …) that musl's long double code (printf's `fmt_fp`) calls; a GCC build gets them from libgcc. `-I /lib/tcc7/include` is for `tccdefs.h`, which tcc reads at run time (`CONFIG_TCC_PREDEFS=0`). `-Ttext=0x10000` static output is a valid Tilck user ELF (stage 12 README).

## `musl-sysroot.tar`

POSIX ustar (mtime 0, uid/gid 0, directories first), for the next plan: `include/` (the 218 headers of `make install-headers`), `lib/libc.a`, `lib/crt1.o`, `lib/crti.o`, `lib/crtn.o`. `kronos.py` checks that it reads as a tar with these members.

## Our files

- `src/SOURCES`: musl's `Makefile` rule for `libc.a` (`src/*/*.c` plus `src/*/riscv64/*.[csS]`, an arch file replacing the generic file of the same name), 1257 files, **without `src/complex/*.c`** (68 files): tcc has no `_Complex` ("_Complex is not yet supported" in `include/complex.h`), so `<complex.h>` functions are missing from this `libc.a`. No comments: every line is compiled.
- `src/cflags.rsp`: `CFLAGS_C99FSE` and the include order of the `Makefile` (`arch/riscv64`, `arch/generic`, `obj/src/internal`, `src/include`, `src/internal`, `obj/include`, `include`), then `-I /lib/tcc7/include` and `-fno-pic`. Flags tcc does not know (`-fomit-frame-pointer`, `-fno-stack-protector`, …) are not passed; `-Os` is meaningless for tcc.
- `src/override/src/include/*.h`, `src/override/src/include/{arpa,sys}/*.h` (19 files): musl's internal wrapper headers include the public ones as `#include "../../include/stdio.h"`, and Kronos does not resolve `..` in paths. Each is the archive file with that line as `#include_next <stdio.h>`: the wrapper is found through `-I /src/musl/src/include`, and the search continues with `src/internal`, `obj/include`, `include`, where the public header is. (`src/math/riscv64/*.c` and `src/fenv/riscv64/fenv-sf.c` also include `"../X.c"`, but only when `__riscv_flen` is undefined; tcc defines it as 64.) On a host, every object compiled with and without these overrides is byte-identical.
- `src/musl-gen.c`, `src/mkustar.c`: see the steps above. `mkustar [-e] OUT ROOT LIST`: a file's mode is the permission bits of its `stat()` (plan 4 phase 1, for the Tilck rungs; before, every file was 0644), with `-e` 0755 for an ELF `ET_EXEC` and 0644 for the rest (Kronos's `stat()` says 0777 for every file; `musl-sysroot.tar` has no executable and keeps its bytes); directories 0755. `mkustar`'s `put()` writes in a loop: under Tilck (stage 14's rungs) `write()` takes at most 12 KB per call, and a single `write()` died with "short write" (reproduced on a host with `write` capped at 12 KB; the loop gives the same bytes as an uncapped run). On a host, `musl-gen`'s three generated headers equal `sed -f tools/mkalltypes.sed …`, the `Makefile`'s `syscall.h` rule and `version.h`, and its `/build/musl/include` equals `make ARCH=riscv64 install-headers` (`diff -r` clean).
- `tests/hello-musl.c`: `printf("hello from %s\n", "musl")`.
- `tests/musl-proc.c`: `fork` + `waitpid`; `pipe` + `fork` + `dup2` + `execv` of itself in child mode (prints `child xyz` into the pipe, exits 3); `opendir`/`readdir` of `/initrd/bin`; `mkdir`/`fopen`/`fprintf`/`stat`/`fgets`/`unlink`/`rmdir` under `/tmp`; `malloc`/`realloc`/`free` up to 16 MB; `snprintf` with `%d %x %s %5.2f %lu`. Prints `musl proc ok` only when every check passed, otherwise `musl proc fail: <what> <value>`.

## Under Tilck

`tilck.py --test` puts the two programs in the initrd as `/initrd/bin/hello-musl` and `/initrd/bin/musl-proc`; stage 12's `proc` (pid 1) runs them after its own tests (fork + execve + wait4, exit code 0 expected), before `tests done`.

**Tilck gap found:** `access()`/`faccessat()` are stubs that return 0 for any path (`kernel/fs/fs_syscalls.c`), so `musl-proc` checks a removed directory with `stat()`.

## Success

Kronos UART `musl-gen ok`, `musl objs ok`, `musl built`, `musl progs built`, `mkustar ok`, `musl done`; `kronos.py` prints `[musl] musl-sysroot.tar …`; `tilck.py --test` UART `hello from musl`, `musl proc ok`, `tests done`.
