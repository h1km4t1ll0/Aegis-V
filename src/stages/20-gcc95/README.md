# 20 — GCC 9.5 on Tilck (rungs `gmp9`, `gcc9a`, `gcc9`)

Plan `docs/2026-11-06-gcc95-plan.md`. Starting from `19-gcc464/artifacts/rootfs-gcc4.tar` (GCC 4.6.4 with g++ in `/usr`, binutils 2.30, musl 1.1.24, make 3.82, bash 2.05b, tar 1.12, gzip 1.2.4, bzip2, gawk 3.0.4, coreutils 5.0, sed, grep, diffutils, m4, flex, findutils, patch; static links only), Tilck builds upstream GCC 9.5.0 for C and C++: first gcc 4.6.4 rebuilds the math libraries GCC 9.5 pins (rung `gmp9`), then g++ 4.6.4 builds GCC 9.5 with `--disable-bootstrap` (rung `gcc9a`), then gcc9a runs GCC 9.5's `make bootstrap` (rung `gcc9`).

## Pinned sources (`build/sources/`)

| File | Size | sha256 | Checked against |
|---|---|---|---|
| `gcc-9.5.0.tar.gz` | 128 130 874 | `15b34072105272a3eb37f6927409f7ce9aa0dd1498efebc35f851d6e6f029a4d` | sha512 of `ftp.gnu.org/gnu/gcc/gcc-9.5.0/sha512.sum` |
| `gmp-6.1.0.tar.bz2` | 2 383 840 | `498449a994efeba527885c10405993427995d3f86b8768d8cdf8d9dd7c6b73e8` | gcc.gnu.org/pub/gcc/infrastructure = ftp.gnu.org |
| `mpfr-3.1.4.tar.bz2` | 1 279 284 | `d3103a80cdad2407ed581f3618c4bed04e0c92d1cf771a65ead662cc397f7775` | the same |
| `mpc-1.0.3.tar.gz` | 669 925 | `617decc6ea09889fb08ede330917a00b16809b8db88c29c31bfbb49cbf88ecc3` | the same |

The versions are GCC 9.5.0's `contrib/download_prerequisites`. No ISL (`--without-isl`, no Graphite). The host checks each sha256 (`RUNGS[…]["distfiles"]`), the guest again (`gmp9/sources`, `checksum-transcriber` + `sha256sum -c`).

## `src/paxx.c`: the pax extractor

`gcc-9.5.0.tar.gz` is a POSIX pax archive (GNU tar `--format=posix`): every member has an `x` header with `mtime`/`atime`/`ctime`, and 556 members carry their name in a `path` key (548 names are over 100 bytes). tar 1.12 knows neither pax headers nor the ustar `prefix` field: it would write the `x` headers as files `PaxHeaders.N/…` and cut the long names. `paxx` (about 400 lines of C, libc only, built by gcc 4.6.4) extracts such an archive from stdin:

```
gzip -dc ARCHIVE | paxx [-C DIR] [-s N] [-X EXCLUDES]
```

- `-C DIR` extracts into DIR (made if missing); `-s N` drops the first N name components (`-s 1`: `gcc-9.5.0/` off, as GNU tar's `--strip-components`; a hard link target too); `-X FILE` is the exclude list.
- Exclude list: one pattern per line, `fnmatch()` without `FNM_PATHNAME` (so `*` also matches `/`, as tar 1.12's `-X`) and with `FNM_LEADING_DIR` (a pattern matches a directory and everything below it, so `gcc/testsuite` needs no `/*`); a line `!PATTERN` keeps a member an exclude pattern matched; empty and `#` lines are ignored. The pattern sees the name after `-s`.
- Members: pax `x` (next member) and `g` (all later members unless an `x` header says otherwise) headers with `path`, `linkpath`, `size` and `mtime` (whole seconds; other keys are ignored); ustar `prefix/name`; types `0`/NUL (file), `5` (directory), `2` (symlink), `1` (hard link, written as a copy of the already extracted target). Any other type, a bad header checksum, an absolute name or a `..` component stops paxx with the member's name (`paxx: NAME: unsupported type 'c'`). Files and directories get the header's permission bits, files also its mtime (the tarball's generated files must stay newer than their inputs for make).
- Input is read in 256 KB chunks (a Tilck pipe gives whatever is there); after the end blocks paxx reads stdin to its end, so `gzip -dc` gets no SIGPIPE.
- At the end: `paxx: N files, D dirs, L links, P pax headers` and `paxx: E excluded`.

Additions to the plan's interface: `-s N` (the plan's `-C DIR` alone would give `DIR/gcc-9.5.0/`), `mtime`, the `!` lines (the plan keeps `libstdc++-v3/testsuite/Makefile.in` out of an excluded directory), and the second summary line.

## The GCC 9.5 source tree (the plan's excludes, `src/gmp9-trial-excludes`)

`gzip -dc gcc-9.5.0.tar.gz | paxx -C /build/gcc-src -s 1 -X EXCLUDES`. Left out (the plan's list): `gcc/testsuite`, `libgo`, `gcc/go`, `gotools`, `libphobos`, `gcc/d`, `gcc/ada`, `libada`, `gnattools`, `gcc/fortran`, `libgfortran`, `gcc/objc`, `gcc/objcp`, `libobjc`, `gcc/brig`, `libbrig`, `libhsail-rt`, `libsanitizer`, `libvtv`, `liboffloadmic`, `libgomp`, `libitm`, `libquadmath`, `libstdc++-v3/testsuite/*` but `libstdc++-v3/testsuite/Makefile.in` (libstdc++'s `configure` writes `testsuite/Makefile`), `*.gmo`, `*.info`. Kept: 7704 files, 520 directories, 288 452 KB on the host (`du -sk`), 86 653 members excluded, 94 878 pax headers read. Of the kept names, 14 come from a pax `path` key, all under `libstdc++-v3/include/ext/pb_ds/detail/`; 7 of them are over 100 bytes below `gcc-9.5.0/` (the 7 headers rung gcc4 extracted again with `ustar-x`). There are no symlinks or hard links in the tarball. This is the plan's list as rung gmp9's trial extraction checks it (`src/gmp9-trial-excludes`, sent to rung gmp9 as `steps/gcc9a/excludes`, byte for byte the file of gmp9's runs 3 and 4). Rung gcc9a's own `gcc9a/excludes` keeps more (rung gcc9a, "Source tree": `gcc/testsuite/selftests`, `gcc/go`, `gcc/d`).

## Rung `gmp9` (plan phase 1)

gcc 4.6.4 (rung gcc4's `/usr/bin/gcc`) builds and tests paxx, extracts `gcc-9.5.0.tar.gz` with it as a trial, then builds gmp 6.1.0, mpfr 3.1.4 and mpc 1.0.3, each with its own `configure`, `make`, `make check`, `make install` into `/usr`, in place of rung gmp's tcc-built gmp 4.3.2, mpfr 2.4.2 and mpc 0.8.1. `/usr` goes back as `artifacts/rootfs-gmp9.tar`, the rootfs of rung `gcc9a`.

```bash
python3 -u src/scripts/tilck.py --rung gmp9
```

`RUNGS["gmp9"]` (`src/scripts/tilck.py`): rootfs `19-gcc464/artifacts/rootfs-gcc4.tar`, steps `gmp9/` (`run.kaem` is the one line `/bin/sh /initrd/steps/run.sh`; `run.sh`, `sources`, `tests/paxw.c`, `tests/libs-test.c`) plus `gmp9_step_files()`: `src/paxx.c`, `src/gmp9-trial-excludes` (as `steps/gcc9a/excludes`), stage 13's `src/mkustar.c` and rung gcc4's `src/mkustar-long.patch`; the four tarballs above (initrd 348 770 304 bytes), timeout 24 h (run 1 had 12 h, below), out `artifacts/`. Marks: `paxx ok`, `paxx gcc ok`, `gmp configured`, `gmp built`, `gmp check ok`, `gmp installed`, the same four for `mpfr` and `mpc`, `gmp mpfr mpc link ok`, `gmp9 ok`, `rootfs sha256 `, `rung done gmp9`.

### `gmp9/run.sh`

1. `/initrd/steps` → `/steps`; `checksum-transcriber sources`, `sha256sum -c` (the four tarballs); `gcc -v`.
2. gcc 4.6.4 builds `paxx`, `paxw` (`tests/paxw.c`) and stage 13's `mkustar` with rung gcc4's `mkustar-long.patch` (`/usr/include/c++/4.6.4/ext/pb_ds/` has names over 100 bytes), all `gcc -static -O2 -g0`.
3. paxx test (plan task 1). The test archive is written in the guest by `paxw make`, not by a host tar: a `g` header (`comment`, `mtime`), `x` headers with `path` (a name of 148 bytes), `linkpath` (a symlink target of 146 bytes), `size` (the ustar size field says 0) and a fractional `mtime`, a 151-byte name in ustar `prefix` + `name`, directories (one 0750), files of 0, 11, 513, 700 and 1500 bytes (one 0755, one 0600), a symlink, a hard link (0444), and members for the exclude lines `skip/*`, `!skip/keep.txt`, `*.gmo`, `gone`. It goes through `gzip -c`, `gzip -dc` and a pipe into `paxx -C out -s 1 -X excludes`, which must print `paxx: 6 files, 2 dirs, 3 links, 4 pax headers` and `paxx: 4 excluded`; `paxw check out` then checks every name, type, size, content byte, permission, the mtimes (global and fractional `x`), both symlink targets and reads through them, the hard link's copy, the excluded members' absence and that no `PaxHeaders.*` or `t/` appeared. `paxw bad-type` (a character device `t/dev/null0`) and `paxw bad-sum` (a wrong checksum on `t/badsum.txt`) must make paxx exit non-zero with `paxx: t/dev/null0: unsupported type '3'` and `paxx: t/badsum.txt: bad header checksum`. All of it → `paxx ok`. The same program was checked on the host first (clang, macOS): bsdtar and Python's `tarfile` read the archive as intended.
4. Trial extraction of `gcc-9.5.0.tar.gz` (plan task 1, with rung gcc9a's excludes) into `/build/gcc-src`: paxx's two lines must be `paxx: 7704 files, 520 dirs, 0 links, 94878 pax headers` and `paxx: 86653 excluded`; `find` must count 7704 files and 521 directories (with the top); `gcc/BASE-VER` is `9.5.0`; the tree digest (`find . -type f | sort | xargs -n 200 md5sum | md5sum`) must be `74830983c2bc5f6eb5c9a8e15b48002d`, the value the host gets both from Python's `tarfile` reading the tarball (same names, same bytes as a host-built paxx's tree) and from `md5 -r` over that tree; the 7 long-named pb_ds headers are non-empty; `libstdc++-v3/testsuite/` holds `Makefile.in` alone. → `paxx gcc ok`, and the tree is removed (rung gcc9a extracts it itself).
5. Rung gmp's `gmp.h`, `mpfr.h`, `mpf2mpfr.h`, `mpc.h`, `libgmp.{a,la}`, `libmpfr.{a,la}`, `libmpc.{a,la}`, their info pages and `share/doc/mpfr/` are removed from `/usr`, so the new ones cannot mix with them.
6. gmp, mpfr, mpc in this order (mpfr needs gmp in `/usr`, mpc both), each `bzip2 -dc`/`gzip -dc | tar xf -` (plain ustar, no long names: tar 1.12 is enough), then:
   - `/bin/sh ./configure --prefix=/usr --build=riscv64-unknown-linux-musl --host=riscv64-unknown-linux-musl --disable-shared --enable-static CC="gcc -static" CFLAGS="-O2 -g0" MAKEINFO=true`, plus for gmp `--disable-assembly CC_FOR_BUILD="gcc -static"`, for mpfr `--with-gmp=/usr`, for mpc `--with-gmp=/usr --with-mpfr=/usr` → `LIB configured`;
   - `make -j1 MAKEINFO=true` → `LIB built`;
   - `make check` (`check()` below) → `LIB check ok`;
   - `make install` → `LIB installed`.
   A step's output goes to `/build/logs/NAME.log`; on success its last two lines, `gmp9 MARK: N s` (guest clock) and the mark; on failure `gmp9 NAME FAIL`, the log's last 60 lines and the newest `config.log`'s last 40 before its variable dump, exit 1. Every step and check runs under `watched` (since run 2): the command goes to the background and the script prints `gmp9 heartbeat LOG: N s, P PASS, F FAIL, last: …` every 300 s of guest time (the log's `PASS`/`FAIL` lines so far and its last line, cut to 100 bytes), so the UART shows where a long `make` or `make check` is; the exit status comes back through a file (`/tmp/watched.rc`).
7. `tests/libs-test.c` (`gcc -static -O2 … -lmpc -lmpfr -lgmp`): 2^100 by gmp, π to 25 digits by mpfr, i·i = −1 by mpc, the versions `6.1.0`, `3.1.4`, `1.0.3` and `GMP_LIMB_BITS` 64 → `gmp mpfr mpc link ok`; with steps 3, 4 and the three checks passed → `gmp9 ok`. Before it, the number of undefined `alloca` references in the three libraries (rung gcc4 linked tcc's `alloca.o` for the tcc-built ones).
8. `/usr` packed by step 2's `mkustar` into `/dev/outbox` (`filelist`, `mkustar`, `catm`), `rootfs sha256 HEX`, `rung done gmp9`.

`check LIB`: `make -j1 check MAKEINFO=true` into `/build/logs/LIB-check.log`. The three use automake's parallel harness (`# TOTAL: N`, `# PASS: N`, … per directory); `run.sh` prints their sums, every `SKIP:`/`XFAIL:`/`FAIL:`/`XPASS:`/`ERROR:` line, the seconds and make's exit status, and `LIB check ok` only if make returned 0 and no test failed. A failed check prints `LIB check FAIL` and the head of each `test-suite.log` with a failure, and the script goes on (the other libraries and the packing still run; the host misses the mark).

### Configure choices

- `CC="gcc -static"`: gcc 4.6.4 links dynamically without `-static` and Tilck refuses dynamic executables (19-gcc464 README, "Static linking"). In `LDFLAGS` libtool would take `-static` as its own flag and not pass it to gcc (17-binutils README); in `CC` it reaches every link, libtool's and configure's. For gmp also `CC_FOR_BUILD` (its table generators `gen-fib`, `gen-bases`, … are not linked by libtool).
- `CFLAGS="-O2 -g0"` (the plan's). gmp adds `-std=gnu99` to `CC` and `-DNO_ASM` (from `--disable-assembly`) to `CFLAGS` itself: `gmp.h` says `__GMP_CC "gcc -static -std=gnu99"`, `__GMP_CFLAGS "-O2 -g0 -DNO_ASM"`, `GMP_LIMB_BITS 64`; `mpn/` has 189 C files and no assembly.
- gmp: `--disable-assembly` (generic C; gmp 6.1.0 has no riscv `mpn` code anyway) and `--host=riscv64-unknown-linux-musl`. The three tarballs' own `config.sub` (gmp's `configfsf.sub` 2015-08-20, mpfr's 2015-08-20, mpc's 2014-12-03) accept `riscv64-unknown-linux-musl` (checked on the host), so unlike rung gmp no `config.sub` from binutils is needed. `--build` is given, so `config.guess` is not run (it cannot guess Tilck: `uname -s` is `Tilck`).
- Run as `/bin/sh ./configure` (Tilck's `#!` argument limit is gone since plan 5 phase 3, but rung gcc1's habit stays).
- libtool drops the `-static` of `CC` from the link lines of the test programs (`libtool: link: gcc -O2 -g0 -o tset_q …`); they still run on Tilck, which refuses dynamic executables: with no `libc.so` and no `libgcc_s` in `/usr`, ld gets only archives and writes no `PT_INTERP`.

### Bug found: Tilck's FPU context (12-tilck README, "FPU context per task")

Run 2's mpfr `make check`: 157 of 160 passed, `tget_set_d64` skipped (decimal64 is off by default), two failed:

```
FAIL: tget_flt   Error for mpfr_get_flt(2^128*(1-2^(-25)),RNDN)  expected inf, got 3.40282346e+38
FAIL: tget_d     Wrong result for 3 * 2^(-1075), rnd_mode 0  got 9.88131291682493088353e-324 instead of 4.94065645841246544176e-324
                 (and 31 more subnormal cases)
```

mpfr's answers were right; the references were wrong. `tget_d`'s reference is `d * f` computed by the FPU (`k * DBL_MIN` times `2^-n`): every wrong one is the exact value rounded toward zero (7.5 ulp → 7, 0.53 ulp → 0), and `FLT_MAX` instead of infinity is the overflow of a float conversion rounded toward zero. gcc 4.6.4 emits `fmul.d`/`fcvt.s.d` with the dynamic rounding mode (the fork's `riscv.md`), so the `frm` field of `fcsr` was 1 (RTZ) when the test ran. Nothing in the test sets it; mpfr's configure does: its "gcc float-conversion bug" test (`acinclude.m4`) calls `fesetround(FE_TOWARDZERO)` and exits. A diagnostic boot on `rootfs-gcc4.tar` (a probe built by gcc 4.6.4 that prints `fcsr` with its first FP instruction, then optionally sets `frm`) showed the leak: after `probe set 1`, the next process started with `fcsr 0x23 frm 1` and computed `3 * 2^-1075` as 1 ulp; a fork child saw the parent's live `frm`, not a copy. Root cause in Tilck (riscv): the first FP instruction of a task enabled the FPU without loading the task's zeroed context, a fork child had no context of its own, and a task that blocked in a syscall had its FPU state neither saved nor restored. Fixed in `12-tilck/src/override/` (`kernel/arch/riscv/cpu.c`, `kernel/arch/riscv/process_riscv.c`, `kernel/switch.c`) with the test `tests/hard.c` `fpustate ok` (`tilck.py --test` mark; the old kernel: `hard fail: fpustate exec 21`, `hard fail: fpustate fork parent 300`). After the rebuild (`kronos.py --tcc-self-k`) only `tilck.elf` and `hard.elf` changed; every other artifact of stages 10–14 is byte-identical; `tilck.py --test` passes all marks; the probe then printed `fcsr at start 0x0 frm 0` for every process and the fork child `frm 2` (the parent's).

gmp's 170 tests and mpc's 64 passed in that same run: neither looks at subnormal or overflowed hardware results.

### Runs

Host clock = seconds since QEMU start (`tilck.py`); guest clock = the `gmp9 MARK: N s` / `LIB check: N s` lines (Tilck's clock runs slower than the host's under QEMU).

- Run 1 (2026-11-06, `artifacts/gmp9-gate-run1.log`, `uart-gmp9-run1.log`, old kernel, 12 h timeout, no heartbeat yet): `paxx ok` at 140 s, `paxx gcc ok` at 272 s (extraction 89 s guest, tree md5 11 s), `gmp configured` at 498 s, `gmp built` at 10 864 s host (1028 s guest), then the 12 h host timeout during gmp's `make check`. The host Mac slept (the keep-awake ended with the session): the guest clock had moved 1028 s while the host's moved about 10 400 s. Not a guest hang; since run 2 every step prints a heartbeat (above) and the timeout is 24 h.
- Run 2 (2026-11-13, `gmp9-gate-run2.log`, `uart-gmp9-run2.log`, old kernel): `paxx ok` 142 s, `paxx gcc ok` 274 s, `gmp configured` 517 s, `gmp built` 1891 s, `gmp check ok` 3180 s (170 of 170, 8 directories, 1025 s guest), `gmp installed` 3193 s, `mpfr configured` 3308 s, `mpfr built` 4342 s; mpfr `make check` 157 pass, 1 skip, 2 fail (`tget_flt`, `tget_d`: the FPU bug above), `mpfr check FAIL`; the script went on: mpfr installed, mpc configured (51 s guest), built (124 s), `make check` 64 of 64 (410 s), installed, `gmp mpfr mpc link ok`, no `gmp9 ok`; the host stopped at `rung exit 0` with marks missing. The outbox dump had sha256 `060faa7e…1aaa`.
- Tilck fixed and rebuilt (12-tilck README), `tilck.py --test` green with `fpustate ok` (`12-tilck/artifacts/test-fpu.log`).
- Run 3 (2026-11-13, `gmp9-gate-run3.log`, `uart-gmp9-run3.log`, fixed kernel): all marks: `paxx ok` 107 s, `paxx gcc ok` 205 s, `gmp configured` 388 s, `gmp built` 1404 s, `gmp check ok` 2382 s, `gmp installed` 2398 s, `mpfr configured` 2482 s, `mpfr built` 3237 s, `mpfr check ok` 4188 s, `mpfr installed` 4203 s, `mpc configured` 4255 s, `mpc built` 4404 s, `mpc check ok` 4908 s, `mpc installed` 4916 s, `gmp mpfr mpc link ok` and `gmp9 ok` 4918 s, `rootfs sha256 ` and `rung done gmp9` 5090 s; `[gmp9] rootfs sha256 match 060faa7e30b6fdbf1c237892e817460ab43c61fa3c5e202536e046f271ff1aaa (214691840 bytes)`; the rung took 5092 s. Guest clock: paxx gcc extraction 58 s, tree md5 8 s; gmp configure 122 s, make 703 s, check 744 s (170 of 170), install 10 s; mpfr configure 56 s, make 521 s, check 656 s (159 pass, `tget_set_d64` skipped: decimal64 is off without `--enable-decimal-float`), install 11 s; mpc configure 36 s, make 102 s, check 349 s (64 of 64), install 6 s; `gmp9 undefined alloca: 0`; `gmp 6.1.0, mpfr 3.1.4, mpc 1.0.3, GMP_LIMB_BITS 64`; `mkustar ok 1177 files`; packing and dumping `/usr` about 170 s (host). The same sha256 as run 2: the libraries the old kernel built were right, only the references of the two tests were wrong.

- Rung gcc4 rerun on the fixed kernel (12-tilck README): the same `rootfs-gcc4.tar`, sha256 `1caf88b7…b9b4`, every mark (`19-gcc464/artifacts/gcc4-gate-fpufix.log`, 7310 s), so this rung's input did not change.
- Run 4 (2026-11-17, `gmp9-gate-run4.log`, `uart-gmp9-run4.log`, fixed kernel, the reproducibility run): all marks: `paxx ok` 108 s, `paxx gcc ok` 208 s, `gmp configured` 392 s, `gmp built` 1460 s, `gmp check ok` 2642 s, `gmp installed` 2656 s, `mpfr configured` 2741 s, `mpfr built` 3516 s, `mpfr check ok` 4476 s, `mpfr installed` 4489 s, `mpc configured` 4541 s, `mpc built` 4690 s, `mpc check ok` 5212 s, `mpc installed` 5219 s, `gmp mpfr mpc link ok` and `gmp9 ok` 5221 s, `rootfs sha256 ` and `rung done gmp9` 5446 s; `[gmp9] rootfs sha256 match 060faa7e30b6fdbf1c237892e817460ab43c61fa3c5e202536e046f271ff1aaa (214691840 bytes)`, the rung took 5447 s; byte for byte run 3's tar. Guest clock: paxx gcc 68 s, tree md5 10 s; gmp configure 142 s, make 859 s, check 907 s (170/170); mpfr 67, 619, 766 s (159 pass, 1 skip); mpc 41, 118, 412 s (64/64); the same test counts as run 3.

Time per part (run 3 / run 4, host clock): rootfs unpack, sha256 of the four tarballs, builds of paxx/paxw/mkustar and the paxx test about 107 s; trial extraction of GCC 9.5 with its checks about 100 s; gmp about 2000–2270 s (of it `make check` about 980–1180 s); mpfr about 1800–1830 s (`make check` about 950–960 s); mpc about 710–730 s (`make check` about 500–520 s); packing and dumping `/usr` about 170–225 s; in all 5092 s and 5447 s, about 1.5 h, as the plan estimated.

### `rootfs-gmp9.tar`

Rung gcc4's `/usr` with the new libraries (1177 files, 214 691 840 bytes): `usr/include/gmp.h` (83 893 bytes), `mpfr.h` (51 307), `mpf2mpfr.h`, `mpc.h`; `usr/lib/libgmp.a` (1 689 796), `libmpfr.a` (1 477 536), `libmpc.a` (314 020) with their `.la`; the info pages and `usr/share/doc/mpfr/` of the new versions (`COPYING.LESSER` instead of 2.4.2's `COPYING.LIB`, `examples/version.c` new). All static, built by gcc 4.6.4 at `-O2 -g0`, generic C. For rung gcc9a: `--with-gmp=/usr --with-mpfr=/usr --with-mpc=/usr`, no `LIBS=/build/tccrt/alloca.o` (no undefined `alloca` left).

## Rung `gcc9a` (plan phase 2)

g++ 4.6.4 (rung gcc4's `/usr/bin/g++`, with gcc 4.6.4 for the C parts) builds upstream GCC 9.5.0 for C and C++ with `--disable-bootstrap` into `/usr/gcc9a`, against rung gmp9's gmp 6.1.0, mpfr 3.1.4 and mpc 1.0.3. The installed gcc9a then compiles and runs the C and C++ tests. `/usr` goes back as `artifacts/rootfs-gcc9a.tar`, the rootfs of rung `gcc9`.

```bash
python3 -u src/scripts/tilck.py --rung gcc9a
```

`RUNGS["gcc9a"]` (`src/scripts/tilck.py`):
- rootfs `artifacts/rootfs-gmp9.tar`;
- steps `gcc9a/`: `run.kaem` (the one line `/bin/sh /initrd/steps/run.sh`), `run.sh`, `sources`, `excludes`, `tests/{hello.c,hello.cpp,sanity.c,sanity.cpp}`;
- `gcc9a_step_files()`: `src/paxx.c`, stage 13's `src/mkustar.c`, rung gcc4's `src/mkustar-long.patch`;
- distfile `gcc-9.5.0.tar.gz` (initrd 343 339 008 bytes);
- timeout 36 h (a run takes about 3.2 h), out `artifacts/`.

Marks: `gcc9a source ok`, `gcc9a configured`, `gcc9a support built`, `cc1 built`, `cc1plus built`, `xgcc built`, `libgcc built`, `libstdc++ built`, `gcc9a all built`, `gcc9a installed`, `gcc version 9.5.0`, `hello from gcc 9.5`, `hello from g++ 9.5`, `gcc9a sanity ok`, `rootfs sha256 `, `rung done gcc9a`.

### `gcc9a/run.sh`

1. `/initrd/steps` → `/steps`. Then `checksum-transcriber sources`, `sha256sum -c`, and `gcc -v`, `g++ -v` (4.6.4).
2. gcc 4.6.4 builds `paxx` and stage 13's `mkustar` with `mkustar-long.patch` (`-static -O2`).
3. `gzip -dc gcc-9.5.0.tar.gz | paxx -C /build/gcc-src -s 1 -X excludes`. paxx prints `paxx: 7970 files, 528 dirs, 0 links, 94878 pax headers` and `paxx: 86379 excluded` (283 103 KB, about 68 s on the guest clock). The script checks `gcc/BASE-VER` = `9.5.0`. `patches/*.patch` would follow with `-p1`; there are none. → `gcc9a source ok`.
4. `configure` in `/build/gcc9a` (by `/bin/sh`): the plan's line with `--prefix=/usr/gcc9a`, plus `--enable-version-specific-runtime-libs` ("Runtime libraries" below). Variables: `CC=gcc CXX=g++ CFLAGS="-O2 -g0" CXXFLAGS="-O2 -g0" LDFLAGS=-static CFLAGS_FOR_TARGET="-O2 -g0" CXXFLAGS_FOR_TARGET="-O2 -g0" LDFLAGS_FOR_TARGET=-static AR=ar RANLIB=ranlib MAKEINFO=true`. → `TARGET_CONFIGDIRS = libgcc libatomic libstdc++-v3`, `gcc9a configured`.
5. make, one subsystem per step, each under `watched` (rung gmp9's heartbeat: `gcc9a heartbeat LOG: N s, L lines, last: …` every 300 guest seconds):
   - support: `all-libiberty all-libcpp all-libdecnumber all-libbacktrace maybe-all-build-libiberty maybe-all-build-libcpp maybe-all-build-fixincludes maybe-all-zlib maybe-all-intl maybe-all-fixincludes configure-gcc`. Afterwards it prints `gcc/Makefile`'s flags and a few `auto-host.h` lines. → `gcc9a support built`
   - `make -C gcc cc1` → `cc1 built`; `make -C gcc cc1plus` → `cc1plus built`
   - `all-gcc` (xgcc, xg++, cpp, collect2, lto-wrapper, gcov*, specs, fixincludes' headers, the selftest runs) → `xgcc built`, then `xgcc -v`
   - `all-target-libgcc` → `libgcc built`
   - `all-target-libstdc++-v3` → `libstdc++ built`
   - `make` (what `all` has left: libatomic, libcc1 with nothing to build) → `gcc9a all built`

   The host steps (support to all-gcc) give `CFLAGS="-O2 -g0" CXXFLAGS="-O2 -g0"` on make's command line. GCC 9.5's `gcc/configure` strips `-O` from both unless `CC` is a bootstrap's `prev-gcc/xgcc` (`gcc/Makefile`: `CFLAGS = CXXFLAGS = -g0`, as in 4.6.4). The top level passes its own flags down, but a plain `make -C gcc` would build cc1 at `-O0`. Each step's log is in `/build/logs/STEP.log`. It prints `gcc9a time STEP: N s` (guest clock) and the mark; on failure it prints `gcc9a STEP FAIL`, the log's last 60 lines and the newest `config.log`'s tail, and exits 1.
6. `make install` → `gcc9a installed`. Then `ls -l` of `bin/`, cc1, cc1plus and every `.a`; `readelf -l` of cc1, cc1plus, gcc, g++ must show 0 `INTERP` (all static); and the size of `/usr/gcc9a`.
7. Tests, all `-static`, built by `/usr/gcc9a/bin/gcc` or `g++`. Every test runs; any failure prints `gcc9a test FAIL WHAT`, then `gcc9a tests FAIL N`, and exits 1 before the packing (no rootfs, no `rung done`; since run 2, see run 1).
   - `gcc -v` must say `gcc version 9.5.0`.
   - `g++ -print-file-name=` of `libstdc++.a`, `libsupc++.a`, `libatomic.a`, `libgcc.a`, `crtbeginT.o` must be in `/usr/gcc9a/lib/gcc/riscv64-unknown-linux-musl/9.5.0/`; `libc.a` and `crt1.o` must be musl's in `/usr/lib`.
   - `tests/hello.c` → `hello from gcc 9.5.0`.
   - `tests/hello.cpp` at `-O2` (rung gcc4's test: `std::cout`, `std::string`, `std::vector`, an exception two calls down caught by `const std::exception &`) → `hello from g++ 9.5.0`.
   - `tests/sanity.c` at `-O2 -Wall`, gcc9a's default `-std=gnu11` → `gcc9a sanity c ok`. It holds rung gcc1's checks (long long, double, switch, recursion, struct by value) plus `__STDC_VERSION__ >= 201112L`, `_Static_assert`, `_Generic` over six types, an anonymous struct in an anonymous union, `_Alignof`/`_Alignas(64)`, compound literals with designated initialisers.
   - `tests/sanity.cpp` at `-O2 -Wall`, default `-std=gnu++14` → `gcc9a sanity c++ ok`. It checks `__cplusplus >= 201402L`, a C++14 `constexpr` function with a loop in a `static_assert`, binary literals and digit separators, a deduced return type, a capturing, a generic and a comparator lambda (`std::sort`), `std::unique_ptr`/`std::make_unique` with virtual calls, `std::map` with `std::to_string`, range-for and `std::out_of_range` caught as `std::logic_error`.
   - Both sanity tests passing → `gcc9a sanity ok`. Every test binary must have no `INTERP`.
8. `/build/gcc9a` and `/build/gcc-src` are removed (Tilck's `/` is in RAM). `/usr` is packed by step 2's `mkustar` into `/dev/outbox`, then `rootfs sha256 HEX`, `rung done gcc9a`.

### Source tree (`gcc9a/excludes`)

The plan's list (above) with three changes, each from a guest error. Three short diagnostic boots on `rootfs-gmp9.tar` (scripts under `/tmp`, nothing in the repo) ran the extraction, `configure`, `make configure-gcc`, `make configure-libcc1` and `make -n -k -C gcc all`:

- `!gcc/testsuite/selftests` (21 files, 12 KB). `make -n` said ``No rule to make target `/build/gcc-src/gcc/testsuite/selftests', needed by `s-selftest-c'``. GCC 9 runs `cc1 -fself-test=$(srcdir)/testsuite/selftests` (and cc1plus's) as part of `all-gcc`, and make needs the directory. In a release build (`--enable-checking=release`, the default) the run only prints "self-tests are not enabled in this build".
- `gcc/go` kept (2.6 MB). With it left out, `configure` gave `TARGET_CONFIGDIRS = libgcc libbacktrace libatomic libstdc++-v3 libffi zlib`. The top level turns off a disabled language's target libraries by reading its `gcc/LANG/config-lang.in` (`target-libgo target-libffi target-libbacktrace` for Go), the way rung gcc1 kept `gcc/java` for `target-zlib`.
- `gcc/d` kept (5.3 MB). D's `config-lang.in` claims `target-zlib target-libbacktrace`, and `gcc/genhooks.c` includes `d/d-target.def` (``No rule to make target `d/d-target.def', needed by `build/genhooks.o'``).

Keeping only the two `config-lang.in` files and `d-target.def` (the second boot) fixed `TARGET_CONFIGDIRS`. But `gcc/configure` takes every language with a `config-lang.in` into `gcc/Makefile`: its `Make-lang.in`, `lang.opt` and `gtfiles` (``No rule to make target `/build/gcc-src/gcc/go/Make-lang.in'``, `d/d-tree.h`, `go/go-lang.c`, …). So both front ends stay whole, as in the full tarball. They are not built, but cc1's option tables list the Go and D options, as an upstream build's do. `libgo`, `gotools` and `libphobos` stay out.

With this list (third boot, and every run): `TARGET_CONFIGDIRS = libgcc libatomic libstdc++-v3`. The dry run only misses what the support step builds. Rung gmp9 keeps checking its trial extraction with the plan's list (`src/gmp9-trial-excludes`, above).

### Configure choices

- **Static links** (19-gcc464 README, "Static linking": gcc 4.6.4 and gcc9a link dynamically without `-static`, and Tilck refuses dynamic executables):
  - `LDFLAGS=-static` reaches every host program: `gcc/Makefile` has `LDFLAGS = -static-libstdc++ -static-libgcc -static` (the top level's `STAGE1_LDFLAGS` in front), and `LDFLAGS_FOR_BUILD = -static` (the default when build = host) covers the generators.
  - GCC's host side has no libtool link that could drop the flag: libtool appears only in libbacktrace (a convenience library), libcc1 (empty here) and the target libraries.
  - `LDFLAGS_FOR_TARGET=-static` is for the target libraries' configure tests, which xgcc links.
  - What runs on Tilck is static: cc1, cc1plus, gcc, g++ (0 `INTERP`) and every test binary.
- **Plugins** are off by themselves. `GCC_ENABLE_PLUGINS` finds `objdump: conftest: not a dynamic object` for both the plain and the `-rdynamic` link (`enable_plugin = no`, `PLUGINLIBS` empty). libcc1 is configured (`--enable-shared` from `Makefile.def`) and builds nothing (`ENABLE_PLUGIN` off).
- **binutils 2.30** is enough so far:
  - `checking assembler for .attribute support... no` (`HAVE_AS_RISCV_ATTRIBUTE` undefined): GCC 9.5 then does not emit `.attribute`.
  - `.sleb128/.uleb128... no` (`HAVE_AS_LEB128 0`), `eh_frame optimization... buggy`, TLS, CFI, `.hidden`, COMDAT, `--build-id`, PIE and `--sysroot` yes.
  - No `as` or `ld` error in libgcc, libstdc++, libatomic or the tests.
- `-O2 -g0` for host and target (the plan's flags). `--disable-symvers` (no perl). `--enable-threads=posix`.
- **Runtime libraries: `--enable-version-specific-runtime-libs`** (since run 2):
  - Run 1 (without it) installed `libstdc++.a`, `libsupc++.a`, `libstdc++fs.a` and `libatomic.a` into `/usr/gcc9a/lib` and the headers into `/usr/gcc9a/include/c++/9.5.0`. g++ then linked gcc 4.6.4's `/usr/lib/libstdc++.a` with 9.5's headers: ``undefined reference to `std::__cxx11::basic_string<…>::_M_append(char const*, unsigned long)'``, `operator delete(void*, unsigned long)`, `std::runtime_error::runtime_error(char const*)`, ….
  - A diagnostic boot on run 1's tar showed the cause: `g++ -print-file-name=libstdc++.a` was `/usr/lib/libstdc++.a`, and `-print-search-dirs` listed `libraries: /usr/gcc9a/lib/gcc/riscv64-unknown-linux-musl/9.5.0/`, the tooldir `…/../../../../riscv64-unknown-linux-musl/lib/`, then `/lib64/lp64d/`, `/usr/lib64/lp64d/`, `/lib/`, `/usr/lib/` (each also with `riscv64-unknown-linux-musl/9.5.0/`), with no `/usr/gcc9a/lib`. `-Wl,-t` showed `(/usr/lib/libstdc++.a)functexcept.o` etc. The same link with `-L/usr/gcc9a/lib` printed `hello from g++ 9.5.0`.
  - The riscv port defines `STARTFILE_PREFIX_SPEC` (`config/riscv/riscv.h`: `/lib64/lp64d/ /usr/lib64/lp64d/ /lib/ /usr/lib/`), and `gcc.c` then uses it in place of the usual startfile prefixes. Those include `$(libdir)` as `…/9.5.0/../../../`; other ports search `PREFIX/lib`, riscv does not. With `--prefix=/usr` (rung gcc9) that is `/usr/lib` and works; with `/usr/gcc9a` it does not.
  - `--enable-version-specific-runtime-libs` (supported by libstdc++-v3 and libatomic, and by `gcc/configure` for the C++ include path) installs those libraries and the C++ headers into `/usr/gcc9a/lib/gcc/riscv64-unknown-linux-musl/9.5.0/` (headers in its `include/c++/`), the driver's first library directory. Since run 2 `-print-file-name` gives that directory for all four libraries, and the tests check it.
  - It is a configure option, not a test-only `-L`, so rung gcc9 can take the same line. There (`--prefix=/usr`) 9.5's runtime goes to `/usr/lib/gcc/riscv64-unknown-linux-musl/9.5.0/` and is found before gcc 4.6.4's `/usr/lib/libstdc++.a`, `libsupc++.a` (and `.la`, `libstdc++.a-gdb.py`) and `/usr/include/c++/4.6.4`. Those stay in `/usr` until rung gcc9 removes them with the other 4.6.4 directories (plan task 7). Without the option, 9.5's `make install` would overwrite `/usr/lib/libstdc++.a`, but gcc9a (prefix `/usr/gcc9a`) would still find 4.6.4's.

No patches: GCC 9.5 builds as released.

### Tilck note: file mappings and `close()`

Every run prints `[pid] Un-map unknown chunk at [0x40000000, 0x40001000)` four times during the support step (once per configure of libiberty, build-libiberty, intl and zlib), and once in `configure-gcc`'s `configure-intl` in the diagnostic boots. These configures carry autoconf 2.69's `AC_FUNC_MMAP` (GCC 4.6.4's are older and do not): `mmap(0, page, …, MAP_SHARED, fd2, 0)`, `close(fd2)`, `munmap()`.

Tilck removes a process's mappings of a file when its last fd of that handle is closed (12-tilck README, "Shared open file descriptions"; upstream removed them at every close). The `munmap()` then finds nothing, logs the line and returns 0, as Linux does for an unmapped range. On Linux the mapping outlives `close()`. The test goes on to a `MAP_FIXED` mapping of another file at the same address and passes either way, so the configure result does not depend on it. Nothing GCC builds or runs here maps a file and uses it after `close()` (ggc-page maps anonymous memory; libcpp reads files). Left as it is; an open question for the Tilck plan.

### Runs

Host clock = seconds since QEMU start (`tilck.py`); guest clock = the `gcc9a time STEP: N s` lines.

- Run 1 (2026-11-17 19:12–22:31, `artifacts/gcc9a-gate-run1.log`, `uart-gcc9a-run1.log`, without `--enable-version-specific-runtime-libs`):
  - Marks to `hello from gcc 9.5` at 11 626 s; `hello.cpp` and `sanity.cpp` failed to link (above), `sanity.c` passed.
  - That run.sh still packed `/usr` (sha256 `ee9567ee…20b6`, 297 092 608 bytes, not kept) and printed `rung done gcc9a`; the host failed the rung for the missing marks. Since run 2 a failed test exits 1 before the packing.
  - Guest clock: configure 51 s, support 917, cc1 5596, cc1plus 661, xgcc 149, libgcc 169, libstdc++ 1348, rest 179, install 66.
- Run 2 (2026-11-17 22:35 – 11-18 01:48, `gcc9a-gate-run2.log`, `uart-gcc9a-run2.log`): every mark.
  - Host clock: `gcc9a source ok` 190 s, `configured` 254 s, `support built` 1452 s, `cc1 built` 8253 s, `cc1plus built` 9006 s, `xgcc built` 9178 s, `libgcc built` 9379 s, `libstdc++ built` 10 953 s, `gcc9a all built` 11 167 s, `installed` 11 249 s, `gcc version 9.5.0` 11 250 s, `hello from gcc 9.5` 11 251 s, `hello from g++ 9.5` 11 262 s, `gcc9a sanity ok` 11 280 s, `rootfs sha256` and `rung done gcc9a` 11 548 s.
  - `[gcc9a] rootfs sha256 match 5538cd20c62dd2eab9ec4a6726ac6d60a9390c9b53e85e1566eba12afaed41a9 (297091584 bytes)`, the rung took 11 549 s.
  - Guest clock: configure 51 s, support 938, cc1 4690, cc1plus 520, xgcc 119, libgcc 138, libstdc++ 1088, rest 148, install 56.
- Run 3 (2026-11-18 01:48–04:59, `gcc9a-gate-run3.log`, `uart-gcc9a-run3.log`, the reproducibility run, same files as run 2): every mark.
  - Host clock: `source ok` 185 s, `configured` 244 s, `support built` 1378 s, `cc1 built` 8145 s, `cc1plus built` 8882 s, `xgcc built` 9054 s, `libgcc built` 9247 s, `libstdc++ built` 10 813 s, `all built` 11 027 s, `installed` 11 109 s, `hello from g++ 9.5` 11 122 s, `gcc9a sanity ok` 11 139 s, `rung done gcc9a` 11 407 s.
  - `[gcc9a] rootfs sha256 match 5538cd20c62dd2eab9ec4a6726ac6d60a9390c9b53e85e1566eba12afaed41a9 (297091584 bytes)`, the rung took 11 409 s; `cmp` with run 2's tar: byte for byte the same.
  - Guest clock: configure 41 s, support 782, cc1 4676, cc1plus 509, xgcc 119, libgcc 133, libstdc++ 1083, rest 147, install 56.

Time per part (run 2, host clock; run 3 within 2–8 %):
- rootfs unpack, sha256, paxx/mkustar build, extraction: 190 s
- configure: 64 s
- support libraries: 1198 s
- cc1 with the generators and the common back end: 6800 s, about 1.9 h, the bulk
- cc1plus: 754 s
- rest of `gcc/` with fixincludes and the selftest runs: 172 s
- libgcc: 200 s
- libstdc++: 1574 s
- libatomic etc.: 215 s
- install: 81 s
- tests: 31 s
- packing and dumping `/usr`: 268 s
- in all about 3.2 h, under the plan's 5–8 h.

g++ 4.6.4 compiled all of GCC 9.5's C++ without an internal error. Its only output besides the commands was warnings such as `cp/parser.c:22758:62: warning: 'list' may be used uninitialized`. The C and C++ tests of the resulting gcc9a passed; whether 4.6.4 miscompiled anything subtle is for rung gcc9's `make bootstrap` compare and fixed-point check.

### `rootfs-gcc9a.tar`

Rung gmp9's `/usr` (unchanged) plus `usr/gcc9a/`: 2043 files, 142 directories, 297 091 584 bytes. The longest name is 147 bytes; mkustar's ustar prefix holds it.

`usr/gcc9a/` has 866 files, 81 709 911 bytes, all built at `-O2 -g0`, all executables static:
- `libexec/gcc/riscv64-unknown-linux-musl/9.5.0/`: `cc1` 15 034 136 bytes, `cc1plus` 16 323 296, `collect2`, `lto-wrapper`, `install-tools/`.
- `bin/`: `gcc` 2 029 904, `g++`/`c++` 2 030 000, `cpp`, `gcc-ar`/`gcc-nm`/`gcc-ranlib`, `gcov`, `gcov-dump`, `gcov-tool` and the `riscv64-unknown-linux-musl-*` names.
- `lib/gcc/riscv64-unknown-linux-musl/9.5.0/`:
  - libraries: `libgcc.a` 4 025 850, `libgcov.a` 834 220, `libstdc++.a` 9 739 544, `libstdc++fs.a` 1 429 204, `libsupc++.a` 583 950, `libatomic.a` 107 636;
  - `crt*.o`, `include/`, `include-fixed/`;
  - `include/c++/` with 787 headers;
  - with `--disable-shared` there is no `libgcc_eh.a` and no `libgcc_s`.
- `share/gcc-9.5.0/python/` (libstdc++ printers) and man pages.

For rung gcc9: `CC=/usr/gcc9a/bin/gcc CXX=/usr/gcc9a/bin/g++`. Links need `-static`. gcc9a finds its own runtime, and musl's `libc.a` and `crt1.o` in `/usr/lib`.


## Completed bootstrap (2026-11-19)

Rung `gcc9` run 3 completed successfully in 68110 seconds inside QEMU/Tilck.
GCC reported `Comparison successful`; the additional fixed-point test reported
`fixedpoint 100 files, 0 differ, 0 failed`. Installed GCC and G++ 9.5.0
compiled and ran both hello programs and the C/C++ sanity tests.

The guest-produced `artifacts/rootfs-gcc9.tar` is 265129984 bytes; SHA256:
`76981c616a3987b348ebdd03a70e311f0a81d80a17c727851b854cb62b7e9a46`.
The host verified the same hash. Bootstrap details are recorded in
`artifacts/gcc9-gate-run3.log` (local build artifact). Earlier failed runs
remain documented separately and do not describe the final result.
