# 17 — binutils 2.30 on Tilck (rung `binutils`)

Rung `binutils` of the Tilck ladder (14-seed README, "Rungs on Tilck"): starting from `16-buildtools/artifacts/rootfs-build1.tar` (bash 2.05b, coreutils 5.0, make 3.82, sed, grep, gawk, diffutils, m4, flex, findutils, tcc-m on musl), Tilck runs binutils 2.30's own top-level `./configure`, `make` and `make install` with tcc, checks the new `as`, `ld`, `ar`, `nm`, `ranlib`, and hands `/usr` back as `artifacts/rootfs-binutils.tar`, the rootfs of rung `gmp`.

```bash
python3 -u src/scripts/tilck.py --rung binutils
```

`RUNGS["binutils"]` (`src/scripts/tilck.py`): rootfs `16-buildtools/artifacts/rootfs-build1.tar`, steps `steps/` (`run.kaem` is the one line `/bin/sh /initrd/steps/run.sh`), the tarball below, timeout 6 h (the gate run took 15.5 min), out `artifacts/` (made if missing).

## Pinned source

| File | URL | sha256 |
|---|---|---|
| `binutils-2.30.tar.bz2` | https://mirrors.kernel.org/gnu/binutils/binutils-2.30.tar.bz2 | `efeade848067e9a03f1918b1da0d37aaffa0b0127a06b5e9236229851d9d0c09` |

live-bootstrap (`b1ceced7`, `steps/binutils-2.30/sources`) pins the `.tar.xz` of the same release; the plan pins the `.tar.bz2` (rung build1 has `bzip2`, no `xz`). The host checks the sha256 (`RUNGS["binutils"]["distfiles"]`), the guest again (`checksum-transcriber sources` + `sha256sum -c` in `build()`).

## `steps/run.sh`

Rung build1's `run.sh` (16-buildtools README) with its `build()` (live-bootstrap's `helpers.sh`, cut down) unchanged, and in place of its packages and tests:

1. `build binutils-2.30` (`steps/binutils-2.30/pass1.sh`, below); every step's output goes to `/build/logs/NAME.log`.
2. The version lines of `as`, `ld`, `ar`, `nm`, `ranlib`, `objdump`, `strip`, `readelf` (`--version | head -1`), then `tests/binutils-test.sh` in `/build/t`.
3. `/usr` packed as in rung build1: `filelist`, `mkustar`, `catm /dev/outbox`, `sha256sum -o`, `rootfs sha256 HEX`, `rung done binutils`.

## `steps/binutils-2.30/pass1.sh`

- `src_prepare`: the default (no patches, no `mk/`). Not taken over from live-bootstrap's `pass1.sh`: its removal and regeneration of the bison/flex output (`ld/ldgram.c`, `ld/ldlex.c`, `binutils/arparse.c`, …), the opcodes tables, `bfd/bfd-in2.h` and `libbfd.h`, the info pages, `po/*.gmo`, `libiberty/crc32.c`'s table, and the `autoreconf` of every directory; its three patches are for that `autoreconf`. The generated files of the release tarball are used as they are (plan policy; tar keeps their mtimes, so `make` regenerates nothing). Its `CFLAGS=-DBUILDFIXED=1 -DDYNAMIC_CRC_TABLE=1` (needed only because it deletes zlib's generated tables) and `CPPFLAGS=-DPLUGIN_LITTLE_ENDIAN` (plugins are off) are not used either.
- `src_configure`: the plan's configure line, `./configure --prefix=/usr --build=riscv64-unknown-linux-musl --host=… --target=… --disable-nls --disable-werror --disable-shared --enable-static --disable-gold --disable-plugins --enable-deterministic-archives CC="tcc -static" AR="tcc -ar" RANLIB=true`, in the source tree (live-bootstrap configures each directory on its own; the plan's one top-level configure configures them during `make`). Additions: `LD=tcc` (libtool's `LT_PATH_LD` stops with "no acceptable ld found" otherwise; as findutils in rung build1), `MAKEINFO=true`, `export lt_cv_sys_max_cmd_len=32768` (live-bootstrap's; skips libtool's probe, and `tcc -ar` must get all objects of a library in one command, it rewrites the archive each time), `export LDFLAGS="-static -Wc,-static"`: every program here is linked by libtool, which takes `-static` as its own flag, from `CC` and `LDFLAGS` alike, and leaves it out of the `tcc` command; tcc-m without it writes a dynamic executable (14-seed README). The first green build of this rung installed a dynamic `as`, `ld`, …: `ERROR: Pid 8045 tried to execute a dynamic executable: /usr/bin/as`, the test killed (`Subprocess error 137`). `-Wc,-static` is libtool's way to pass a flag to the compiler; tcc ignores `-Wc,…` in configure's own tests. `--build` is needed anyway: `config.guess` cannot guess Tilck (`uname -s` = `Tilck`); binutils 2.30's `config.sub` knows `riscv64-unknown-linux-musl`.
- `src_compile`: `make -j1 all-libiberty`, `all-zlib`, `all-bfd`, `all-opcodes`, `all-binutils`, `all-gas`, `all-ld` (each configures its directory first), then `make all` for the rest (`gprof`, `intl`, `etc`), each with `MAKEINFO=true AR="tcc -ar" RANLIB=true tooldir=/usr` (`AR`: the Makefiles say `AR = ar`; `tooldir=/usr` is live-bootstrap's: `ld`'s `ldscripts` in `/usr/lib`, no `/usr/riscv64-unknown-linux-musl/bin`). A step prints the last two lines of its log and `binutils NAME ok`, or `binutils NAME FAIL`, the last 50 lines of its log and of the newest `config.log`, and exits 1.
- `src_install`: `make install` with the same variables.

No source patch was needed: binutils 2.30 builds with tcc-m and musl as it is (a host rehearsal with a cross tcc of the same sources: no implicit declarations, only `__PRETTY_FUNCTION__ redefined` and "function might return no value" warnings).

## Tests

- The eight version lines, each `GNU … (GNU Binutils) 2.30` (`GNU assembler`, `GNU ld`, `GNU ar`, `GNU nm`, `GNU ranlib`, `GNU objdump`, `GNU strip`, `GNU readelf`).
- `tests/hello.s` (by hand: `write(1, msg, 16)` and `exit(0)` with `ecall`, `.option norelax` because it sets no `gp`) → `as` → `ld -static` → run, output `binutils asm ok` → `binutils asm ok`.
- `tests/hello.c` → `tcc -c` → `ld -static -o hello /usr/lib/crt1.o /usr/lib/crti.o hello.o /usr/lib/libc.a /usr/lib/tcc/libtcc1.a /usr/lib/crtn.o` (GNU ld on tcc's objects and musl's tcc-built `libc.a`, the plan's order works without `--start-group`) → run, output `binutils hello ok` → `binutils hello ok`.
- `nm /usr/lib/libc.a | grep -c ' T '` > 1000 and `ar t /usr/lib/libc.a` > 1000 members (`nm: 1553 T symbols in libc.a, ar t: 1257 members`); `ranlib` of a copy keeps the member count, and `hello` linked against the copy runs; `ar rcs libh.a hello.o`, `ar t libh.a` = `hello.o` → `binutils ar ok`.

A failure prints `binutils test FAIL: …` and exits 1. `tilck.py` marks: `binutils configure ok`, `binutils libiberty ok`, `binutils zlib ok`, `binutils bfd ok`, `binutils opcodes ok`, `binutils binutils ok`, `binutils gas ok`, `binutils ld ok`, `binutils all ok`, `binutils install ok`, the eight version lines, `binutils asm ok`, `binutils hello ok`, `binutils ar ok`, `rootfs sha256 `, `rung done binutils`, `rung exit 0`.

## Fixed elsewhere (found by this rung)

- tcc (11-tcc7 README, `riscv64-gen.c` `gfunc_epilog()`, test `epilog.c`): every function epilogue gave its frame back (`mv sp, s0`) before loading the saved `ra`/`s0`; a timer interrupt in that window made Tilck's trap entry write 0 over the saved `s0`. The first run of this rung hung about 10 minutes in (`make` in `ld/`): an endless store-page-fault loop at the trap entry (`scause 0xf`, garbage `sp`), read with QMP `info registers` and `x`, and traced through the task's kernel stack to a load through `s0 - 56` after `sys_fstat64()` returned (12-tilck README, "Kernel stack": plan 3's rare hang). tcc7b, Tilck, musl, the seed and rungs tcc, tools1, tools2, build1 were rebuilt (new sha256s in the 14-seed, 15-early-tools and 16-buildtools READMEs).
- Tilck, before this rung (12-tilck README, plan 4 phase 3): a `fork()` child and a `dup()`ed fd share the file offset (`sharedoff ok`), so `make > log 2>&1` and `config.log` are whole; `abort()` in a signal handler kills with SIGABRT (`abort ok`); `killcwd ok` (not reproduced, kept as a guard).

## Tools in `rootfs-binutils.tar`

`usr/bin/`: rung build1's 108 programs plus `addr2line`, `ar`, `as`, `c++filt`, `elfedit`, `gprof`, `ld`, `ld.bfd` (a copy of `ld`), `nm`, `objcopy`, `objdump`, `ranlib`, `readelf`, `size`, `strings`, `strip` (124 programs; static riscv64 `ET_EXEC`, no `PT_INTERP`, 0.6–12.6 MB each: built with `-g`, not stripped); `usr/lib/libbfd.a`, `libopcodes.a` (+ `.la`), `usr/lib/ldscripts/` (`elf64lriscv.*`, `elf32lriscv.*`), `usr/include/{bfd,bfdlink,ansidecl,symcat,dis-asm}.h` (a native build installs libbfd), `usr/share/info/{as,bfd,binutils,gprof,ld}.info`, 19 binutils pages in `usr/share/man/man1/` (from the tarball, `MAKEINFO=true` builds none); 454 files, 173 776 896 bytes.

How to use them on the next rungs: `as` and `ld` are native (`riscv64-unknown-linux-musl`, default emulation `elf64lriscv`), `ld -static` links musl's `crt1.o crti.o … libc.a /usr/lib/tcc/libtcc1.a crtn.o` from `/usr/lib`; `ar` and `ranlib` are the real ones now (deterministic archives by default), so `AR="tcc -ar"`/`RANLIB=true` are no longer needed; `tcc` still links by itself. Still missing: `mktemp`, `bison`, `makeinfo`, `msgfmt`, `libm.a`.

## Success

`tilck.py --rung binutils` (seconds since QEMU start, host clock): `[binutils] got 'binutils configure ok' at 120.6s` (rootfs unpack, the sources check, `bzip2 -dc | tar xf -` of the 280 MB tree and the top-level configure), `'binutils libiberty ok' at 177.3s`, `'binutils zlib ok' at 202.9s`, `'binutils bfd ok' at 341.0s`, `'binutils opcodes ok' at 388.8s`, `'binutils binutils ok' at 557.0s`, `'binutils gas ok' at 571.8s`, `'binutils ld ok' at 683.2s`, `'binutils all ok' at 750.6s`, `'binutils install ok' at 785.2s`, `'GNU assembler (GNU Binutils) 2.30' at 788.9s` … `'GNU readelf (GNU Binutils) 2.30' at 789.2s`, `'binutils asm ok' at 789.5s`, `'binutils hello ok' at 789.7s`, `'binutils ar ok' at 792.6s`, `'rootfs sha256 ' at 925.8s`, `'rung done binutils' at 925.8s`, `'rung exit 0' at 925.8s`; `[binutils] rootfs sha256 match 10bd8e33cf62b8ca4915ab88b6f2c0563699278ffd14d166997264ee00d39564 (173776896 bytes)`; the rung took 927 s. Per step: configure of each directory plus its `make` — libiberty 57 s, zlib 26 s, bfd 138 s, opcodes 48 s, binutils 168 s, gas 15 s, ld 111 s, the rest of `all` 67 s, `make install` 35 s; packing and dumping the 174 MB `/usr` 133 s.
