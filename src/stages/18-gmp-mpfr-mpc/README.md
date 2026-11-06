# 18 — gmp 4.3.2, mpfr 2.4.2, mpc 0.8.1 on Tilck (rung `gmp`)

Rung `gmp` of the Tilck ladder (14-seed README, "Rungs on Tilck"): starting from `17-binutils/artifacts/rootfs-binutils.tar` (binutils 2.30, bash 2.05b, make 3.82, coreutils, grep, gawk, sed, m4, diffutils, findutils, flex, tcc-m on musl), Tilck builds GCC 4.6.4's three math libraries, each with its release tarball's own `./configure`, `make`, `make check` and `make install` into `/usr`, and hands `/usr` back as `artifacts/rootfs-gmp.tar`, the rootfs of GCC 4.6.4 (plan 5).

```bash
python3 -u src/scripts/tilck.py --rung gmp
```

`RUNGS["gmp"]` (`src/scripts/tilck.py`): rootfs `17-binutils/artifacts/rootfs-binutils.tar`, steps `steps/` (`run.kaem` is the one line `/bin/sh /initrd/steps/run.sh`), the four tarballs below, timeout 10 h (the gate run took 47 min), out `artifacts/`.

## Pinned sources

Versions: GCC 4.6.4's `contrib/download_prerequisites` (`build/sources/gcc-4.6.4-c0d20242.tar`): `MPFR=mpfr-2.4.2`, `GMP=gmp-4.3.2`, `MPC=mpc-0.8.1`, fetched from `ftp://gcc.gnu.org/pub/gcc/infrastructure/`. Downloaded (2026-10-30) from the same directory over https and, independently, from each project's own site; the bytes are the same (sha256 and size):

| File | URL (pin) | Second source | sha256 |
|---|---|---|---|
| `gmp-4.3.2.tar.bz2` | https://gcc.gnu.org/pub/gcc/infrastructure/gmp-4.3.2.tar.bz2 | https://ftp.gnu.org/gnu/gmp/gmp-4.3.2.tar.bz2 | `936162c0312886c21581002b79932829aa048cfaf9937c6265aeaa14f1cd1775` (1 897 483 bytes) |
| `mpfr-2.4.2.tar.bz2` | https://gcc.gnu.org/pub/gcc/infrastructure/mpfr-2.4.2.tar.bz2 | https://www.mpfr.org/mpfr-2.4.2/mpfr-2.4.2.tar.bz2 | `c7e75a08a8d49d2082e4caee1591a05d11b9d5627514e678f02d66a124bcf2ba` (1 077 886 bytes) |
| `mpc-0.8.1.tar.gz` | https://gcc.gnu.org/pub/gcc/infrastructure/mpc-0.8.1.tar.gz | https://www.multiprecision.org/downloads/mpc-0.8.1.tar.gz | `e664603757251fd8a352848276497a4c79b7f8b21fd8aedd5cc0598a38fee3e4` (544 950 bytes) |
| `binutils-2.30.tar.bz2` | as rung binutils (17-binutils README) | | `efeade848067e9a03f1918b1da0d37aaffa0b0127a06b5e9236229851d9d0c09` |

The host checks the sha256 (`RUNGS["gmp"]["distfiles"]`), the guest again (`checksum-transcriber sources` + `sha256sum -c` in `build()`; binutils' in `steps/riscv-config/`).

## `steps/run.sh`

Rung binutils' `run.sh` (17-binutils README) with the same `build()` (live-bootstrap's `helpers.sh`, cut down), and:

1. Exported for every step: `SHELL=/bin/sh CONFIG_SHELL=/bin/sh`, `LDFLAGS="-static -Wc,-static"` (every program here, the test programs too, is linked by libtool: 17-binutils README), `lt_cv_sys_max_cmd_len=32768` (skips libtool's probe, as rung binutils; with GNU `ar` a split archive command would be fine too).
2. Helpers for the recipes: `step LIB NAME CMD…` (output to `/build/logs/LIB-NAME.log`; its last two lines, the guest's seconds, `LIB NAME ok`; or `LIB NAME FAIL`, the tails of the log and of the newest `config.log`, exit 1) and `check LIB`: `make check MAKEINFO=true` to `/build/logs/LIB-check.log`, then automake's summary lines (`All N tests passed`, `N of M tests failed`, `… not run`), `LIB check: N tests passed in D directories`, the seconds and make's exit status, and `LIB check ok` only if `make check` returned 0; otherwise `LIB check FAIL`, the `FAIL:` lines, the log's tail and exit 1.
3. `config.sub` and `config.guess` from the pinned binutils 2.30 tarball (`bzip2 -dc … | tar xf - binutils-2.30/config.sub binutils-2.30/config.guess` in `/steps/riscv-config`; both `timestamp='2018-01-01'`, sha256 `62e047d532769b06024f781c4c5a44acca3e109141640b07ddd96ef3661670dc` and `dc3db3b9855a642625713f541dd57fa93d240b3bee6ccfe4635a45c310688dc1`, printed to the UART): the three tarballs' own (2008–2009) stop with ``Invalid configuration `riscv64-unknown-linux-musl': machine `riscv64-unknown-linux' not recognized`` (they know neither riscv64 nor `linux-musl`; `none-unknown-linux-musl` is rejected the same way).
4. `build gmp-4.3.2`, `build mpfr-2.4.2`, `build mpc-0.8.1` (`steps/<lib>/pass1.sh`, below), in this order: mpfr needs gmp in `/usr`, mpc both.
5. `tests/libs-test.c` → `tcc -static … -lmpc -lmpfr -lgmp` → run: 2^100 by gmp, pi to 25 digits by mpfr, i·i = −1 by mpc, the three version strings, `gmp mpfr mpc link ok` (what GCC 4.6.4's configure needs: the headers and the three static libraries from `/usr`).
6. `/usr` packed as in rung binutils: `filelist`, `mkustar`, `catm /dev/outbox`, `sha256sum -o`, `rootfs sha256 HEX`, `rung done gmp`.

## Recipes (`steps/<lib>/pass1.sh`)

No source patch was needed: the three libraries build and pass their own `make check` with tcc-m and musl as they are. Each recipe has `src_prepare` (the default, then the binutils `config.sub`/`config.guess`), `src_configure`, `src_compile` (`step LIB make make -j1 MAKEINFO=true`, then `check LIB`) and `src_install` (`step LIB install make install MAKEINFO=true`). The generated files of the tarballs (`configure`, `Makefile.in`, the info pages) are used as they are (plan policy); `MAKEINFO=true` everywhere (no makeinfo; the `.info` files come from the tarballs).

- **gmp 4.3.2**: `src_prepare` replaces `configfsf.sub` and `configfsf.guess`, not `config.sub`/`config.guess`: gmp's own are wrappers that map gmp's CPU names and call the `configfsf.*` ones. `./configure --prefix=/usr --build=riscv64-unknown-linux-musl --host=none-unknown-linux-musl --disable-shared --enable-static CC=tcc CC_FOR_BUILD="tcc -static" MAKEINFO=true`. gmp 4.3.2 has no `--disable-assembly` (added in gmp 5.1); CPU `none` is its documented generic C build (`doc/gmp.texi`, "Build Options", "Generic C Build": `./configure --host=none-unknown-freebsd3.5`): `configure.in`'s `none-*-*` branch, `abilist="long longlong"`, configure picks `ABI="long"` (64-bit limb, `GMP_LIMB_BITS 64` in `gmp.h`), `MPN_PATH=" generic"` (only `mpn/generic/*.c`, no `.asm`, no `m4` run). An unknown CPU (`--host=riscv64-…`) would give the same `MPN_PATH`, but `none` says so on purpose. With `--build` ≠ `--host` configure takes this as a cross build: it needs `CC_FOR_BUILD` for its table generators (`gen-fib`, `gen-bases`, `gen-fac_ui`, `gen-psqr`), which must be `-static` as every program here (they are not linked by libtool and get no `LDFLAGS`); its only run test, `vsnprintf`, is assumed to work ("probably"). `CFLAGS` stay empty (configure has no flags for a compiler it does not know; `__GMP_CFLAGS ""` in `gmp.h`). No `libm.a` was needed (configure's `AC_CHECK_LIBM` finds none and links without).
- **mpfr 2.4.2**: `./configure --prefix=/usr --build=riscv64-unknown-linux-musl --host=riscv64-unknown-linux-musl --disable-shared --enable-static --with-gmp=/usr CC=tcc MAKEINFO=true` (a native build: configure runs its probes on Tilck; `CFLAGS=-g`; no `-lm`, "checking for main in -lm... no"). mpfr also installs `mpf2mpfr.h` and `share/doc/mpfr/`.
- **mpc 0.8.1**: `./configure --prefix=/usr --build=… --host=riscv64-unknown-linux-musl --disable-shared --enable-static --with-gmp=/usr --with-mpfr=/usr CC=tcc MAKEINFO=true`. mpc 0.8.1 does not use C99 `_Complex` (its own `mpc_t` of two `mpfr_t`), so tcc's missing `_Complex` and musl's missing `src/complex/` (13-musl README) do not matter.

A host rehearsal (macOS, the cross tcc of plan 3 with rung tcc's musl, `cross_compiling=yes`) built the three libraries and all their test programs with no error; the only warnings were gmp's `tests/mpn/t-matrix22.c:161: assignment of read-only location` and mpc's `read_data.c:201: function might return no value`.

## Tests

`make check` of each library, unchanged (deterministic: without `GMP_CHECK_RANDOMIZE` the test suites seed their random state with a constant):

- gmp: `All 9 tests passed` (`tests`), `All 12 tests passed` (`tests/mpn`), `All 58 tests passed` (`tests/mpz`), `All 11 tests passed` (`tests/mpq`), `All 26 tests passed` (`tests/mpf`), `All 7 tests passed` (`tests/rand`), `All 3 tests passed` (`tests/misc`) → `gmp check: 126 tests passed in 7 directories` (`tests/devel` has no tests; `cxx` and `mpbsd` are off by default).
- mpfr: `All 148 tests passed` (none skipped).
- mpc: `All 57 tests passed`.

Then `gmp mpfr mpc link ok` (above). `tilck.py` marks: `gmp configure ok`, `gmp make ok`, `gmp check ok`, `gmp install ok`, the same four for `mpfr` and `mpc`, `gmp mpfr mpc link ok`, `rootfs sha256 `, `rung done gmp`, `rung exit 0`.

## Libraries in `rootfs-gmp.tar`

Rung binutils' `/usr` plus `usr/include/{gmp,mpfr,mpf2mpfr,mpc}.h`, `usr/lib/libgmp.a` (1 225 106 bytes), `libmpfr.a` (2 801 824), `libmpc.a` (648 762) with their `.la` files, `usr/share/info/{gmp.info,gmp.info-1,gmp.info-2,mpfr.info,mpc.info}`, `usr/share/doc/mpfr/` (26 new files; 480 files, 179 458 560 bytes). Static only, built with tcc-m (`-g`, not stripped), generic C. For GCC 4.6.4: `--with-gmp=/usr --with-mpfr=/usr --with-mpc=/usr` (or nothing: they are in the default search path); link order `-lmpc -lmpfr -lgmp`.

## Success

`tilck.py --rung gmp` (seconds since QEMU start, host clock): `[gmp] got 'gmp configure ok' at 209.7s` (rootfs unpack, the binutils `config.*` extraction, gmp's unpack and configure), `'gmp make ok' at 788.0s`, `'gmp check ok' at 1300.4s`, `'gmp install ok' at 1307.7s`, `'mpfr configure ok' at 1365.2s`, `'mpfr make ok' at 1525.7s`, `'mpfr check ok' at 2093.0s`, `'mpfr install ok' at 2101.2s`, `'mpc configure ok' at 2134.4s`, `'mpc make ok' at 2179.7s`, `'mpc check ok' at 2616.7s`, `'mpc install ok' at 2619.9s`, `'gmp mpfr mpc link ok' at 2620.7s`, `'rootfs sha256 ' at 2815.3s`, `'rung done gmp' at 2815.3s`, `'rung exit 0' at 2815.3s`; `[gmp] rootfs sha256 match 1461af4edc9ec3745bda59d5cd70cb5582f437d7db5bc7d8e28d6e1d5931ae53 (179458560 bytes)`; the rung took 2817 s. Per step by the host clock: gmp `make` 578 s, `make check` 512 s, `make install` 7 s; mpfr unpack + `configure` 58 s, `make` 160 s, `make check` 567 s; mpc unpack + `configure` 33 s, `make` 45 s, `make check` 437 s; packing and dumping `/usr` 195 s. The guest's own clock (`LIB STEP: N s` lines) runs about 1.4 times slower than the host's: gmp configure 74 s, make 400 s, check 355 s; mpfr 37, 111, 393 s; mpc 22, 31, 303 s. A second run (the gate run, same files but for a comment in `run.sh`): the same marks and test counts, the same sha256 `1461af4e…ae53`, 2629 s (`'gmp check ok' at 1224.8s`, `'mpfr check ok' at 1947.6s`, `'mpc check ok' at 2445.3s`).
