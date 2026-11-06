# 15 — early tools on Tilck (live-bootstrap recipes)

Rung `tools1` of the Tilck ladder (14-seed README, "Rungs on Tilck"): starting from `14-seed/artifacts/rootfs-tcc.tar` (tcc-m3, musl, kaem, the mescc-tools-extra tools, `filelist`, `mkustar`), Tilck builds with live-bootstrap's own recipes `make-3.82`, `patch-2.5.9`, `gzip-1.2.4`, `tar-1.12`, `sed-4.0.9`, `bzip2-1.0.8`, in that order (live-bootstrap `steps/manifest`), tests them and hands `/usr` back as `artifacts/rootfs-tools1.tar`, the rootfs of the next rung.

Rung `tools2` (section "Rung tools2" below, recipes in `steps-tools2/`) then builds coreutils 5.0, oyacc 6.6 and bash 2.05b on top of it.

```bash
python3 -u src/scripts/tilck.py --rung tools1
```

## Pinned sources

Recipes: live-bootstrap https://github.com/fosslinux/live-bootstrap `b1ceced7ea8a819a26f23796f6dd7ae8496a33a4` (14-seed README; GPL-3.0-or-later). Tarballs: the URL and sha256 of each `steps/<pkg>/sources`; the host downloads them to `build/sources/` (gitignored) and checks the sha256, `tilck.py` (`RUNGS["tools1"]["distfiles"]`) checks them again before putting them in the initrd, and the guest checks them a third time with the recipe's own `checksum-transcriber sources` + `sha256sum -c sources.SHA256SUM`.

| File | URL | sha256 |
|---|---|---|
| `make-3.82.tar.bz2` | https://mirrors.kernel.org/gnu/make/make-3.82.tar.bz2 | `e2c1a73f179c40c71e2fe8abf8a8a0688b8499538512984da4a76958d0402966` |
| `patch-2.5.9.tar.gz` | https://mirrors.kernel.org/gnu/patch/patch-2.5.9.tar.gz | `ecb5c6469d732bcf01d6ec1afe9e64f1668caba5bfdb103c28d7f537ba3cdb8a` |
| `gzip-1.2.4.tar.gz` | https://mirrors.kernel.org/gnu/gzip/gzip-1.2.4.tar.gz | `1ca41818a23c9c59ef1d5e1d00c0d5eaa2285d931c0fb059637d7c0cc02ad967` |
| `tar-1.12.tar.gz` | https://mirrors.kernel.org/gnu/tar/tar-1.12.tar.gz | `c6c37e888b136ccefab903c51149f4b7bd659d69d4aea21245f61053a57aa60a` |
| `sed-4.0.9.tar.gz` | https://mirrors.kernel.org/gnu/sed/sed-4.0.9.tar.gz | `c365874794187f8444e5d22998cd5888ffa47f36def4b77517a808dec27c0600` |
| `bzip2-1.0.8.tar.gz` | https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz | `ab5a03176ee106d3f0fa90e381da478ddae405918153cca248e682cd0c4a2269` |

## Recipes (`steps/<pkg>/`)

Copies of live-bootstrap's `steps/<pkg>/` (`pass1.kaem`, `sources`, `mk/`, `patches/`, `files/`); not copied: `<pkg>.checksums` and `pass2.sh`. `diff -r` against a checkout of the pin shows exactly the changes below.

- All six: the final `# Checksums` block (`sha256sum -c ${pkg}.checksums`) is removed: it checks live-bootstrap's own binaries (its tcc 0.9.27 with mes libc on x86), ours are other bytes.
- `tcc` is the rung's `/usr/bin/tcc` (tcc-m3 of rung tcc, musl). Every link line of the recipes already passes `-static` (`make` in `pass1.kaem`, `LDFLAGS = -static` in the `mk/main.mk` of `patch`, `gzip`, `tar`, `sed`, `LDFLAGS="-static"` for `bzip2`), so no recipe changes for that.
- `make-3.82`: `catm config.h` (empty) → `cp ../../files/config.h config.h`, our `files/config.h` with `STDC_HEADERS HAVE_STRING_H HAVE_UNISTD_H HAVE_LIMITS_H HAVE_ALLOCA_H HAVE_STRCASECMP HAVE_STRNCASECMP`. With an empty one, `make.h` declares `char *alloca ()` and `int strncasecmp (…, int)` against musl's prototypes (tcc: "incompatible types for redefinition"), `remake.c` has no `ULONG_MAX`, and `string.h`/`stdlib.h` are not included, so `strchr` & co. return an implicit `int`, cut to 32 bits on riscv64. `glob/fnmatch.c` and `glob/glob.c` are compiled with `-I. -DHAVE_CONFIG_H` added (same reasons: they read `config.h` only under `HAVE_CONFIG_H`).
- `patch-2.5.9`: `mk/main.mk` `CPPFLAGS` + `-DHAVE_STRERROR -DHAVE_DECL_STRERROR` (otherwise `error.c` uses `sys_errlist`/`sys_nerr`, which musl does not have: link error).
- `gzip-1.2.4`: `files/stat_override.c` without `#include <linux/syscall.h>` and `<linux/x86/syscall.h>` (mes libc headers; musl has neither, and `sys/stat.h` is all the file needs).
- `tar-1.12`: the same two lines out of `files/stat_override.c`; `mk/main.mk` `CPPFLAGS` + `-DSTDC_HEADERS -DHAVE_STRING_H -DHAVE_UNISTD_H -DHAVE_ALLOCA_H -DHAVE_STRERROR` (`system.h`'s `char *alloca ()` against musl's, `sys_errlist`, implicit `int` for `xstrdup`'s `strcpy`, `lseek`); new `patches/no-same-owner.patch`, applied in `pass1.kaem` after `stat_override.c`: tar does not chown extracted files and directories when it runs as root (only with `--same-owner`). Tilck has only uid 0, its `chown()` to any other owner failed with `EPERM`, and the uid 1000 files of the sed and bzip2 tarballs made `tar xf` exit 2 in their recipes (Tilck's `chown()` succeeds for root since plan 4 phase 1, 12-tilck README; the patch stays).
- `sed-4.0.9`: `mk/main.mk` `CPPFLAGS` + `-DSTDC_HEADERS -DHAVE_STDLIB_H -DHAVE_STRING_H -DHAVE_UNISTD_H` (implicit `int` `malloc`/`calloc`/`realloc`/`getenv` otherwise); `make -f Makefile LIBC=mes` → `LIBC=musl`: live-bootstrap's `main.mk` then builds `lib/alloca.c` instead of `lib/getline.c`, whose `getline` conflicts with musl's.
- `bzip2-1.0.8`: `patches/mes-libc.patch` is neither copied nor applied (its line is gone from `pass1.kaem`): musl has `utime`, `fchown`, `fchmod` (and Tilck implements `utimensat`, `fchmod`, `fchown`), and the `utime.h` the patch creates (found first through `-I .`) defines `fchown`/`fchmod` as macros, which breaks musl's `unistd.h`. `patches/coreutils.patch` is kept.

What stays as in live-bootstrap and is harmless here: `rm -r ../src` and the Makefile's `rm -f libbz2.a` (mescc-tools-extra's `rm` has no options: it prints `unable to delete file: -r !!!`, exits 0); `make: warning: Clock skew detected` in sed and bzip2, the two packages our `tar` extracts (it sets the archive's mtimes, which make finds later than Tilck's clock; not looked into further); the remaining implicit declarations are of `int` functions, except `strchr` in `sed/fmt.c`, whose result is only compared with `NULL`.

## `steps/run.kaem`

1. Copies every recipe file from `/initrd/steps/<pkg>/` to `/steps/<pkg>/` (the recipes write `build/`, `src/` and `sources.SHA256SUM` next to themselves, and `/initrd` is read-only), one file per `cp`: with several sources, mescc-tools-extra's `cp` appends each basename to the same destination string, and the second copy fails ("Provide a directory destination for multiple source files").
2. Copies the six tarballs from `/initrd/distfiles/` to `/distfiles/` and sets `DISTFILES=/distfiles`: on Tilck's FAT32 initrd `lseek(fd, 0, SEEK_END)` returns the current offset (`kernel/fs/fat32.c` `fat_seek()` handles `SEEK_END` only for a negative offset), so `sha256sum`, which takes a file's size with `fseek`/`ftell`, hashed `/initrd/distfiles/*` as empty files (`Received: e3b0c442…`). ramfs seeks correctly. (`fat_seek()` is fixed since plan 4 phase 1, 12-tilck README; the copy stays.)
3. For each package, what live-bootstrap's `seed/script-generator.c` (`output_build()`) writes for a kaem build: `pkg=NAME`, `cd ${pkg}`, `kaem --file pass1.kaem`, `cd ..`, from `/steps`.
4. Tests: `make --version`, `patch --version`, `gzip --version`, `tar --version`, `sed --version`, `bzip2 --help`. tar + gzip there and back: `/usr/include/elf.h`, `/usr/bin/make`, `run.kaem` in `/build/tg/d`; `sha256sum -o` of the three files, `tar cf d.tar d`, `sha256sum -o` of `d.tar`, `gzip d.tar`; in `/build/tg2` `gunzip d.tar.gz`, `sha256sum -c` of the tar, `tar xf d.tar`, `sha256sum -c` of the files (there is no `cmp`) → `tar gzip ok`. `make -f tests/test.mk` in `/build/mk`: a pattern rule compiles `tests/hello.c`, a second rule links it, `all` runs it, it prints `make ok`.
5. Packs `/usr` as rung tcc does: `filelist`, `mkustar`, `cp` to `/dev/outbox`, `rootfs sha256 HEX` (`sha256-prefix` is a copy of `14-seed/steps/tcc/sha256-prefix`), `rung done tools1`.

`tilck.py` marks, in order: `GNU Make 3.82`, `patch 2.5.9`, `gzip 1.2.4`, `tar (GNU tar) 1.12`, `GNU sed version 4.0.9`, `Version 1.0.8` (bzip2), `tar gzip ok`, `make ok`, `rootfs sha256 `, `rung done tools1`, `rung exit 0`. `tilck.py` now looks for fail marks only in complete UART lines: a read that ended inside `rung exit 0` was taken for the fail mark `rung exit ` once.

## Tools in `rootfs-tools1.tar`

`usr/bin/`: `make`, `patch`, `gzip`, `gunzip`, `tar`, `sed`, `bzip2`, `bunzip2` (static riscv64 ELF, musl) on top of rung tcc's 18 programs; 259 files in all. `make` runs recipe lines without a shell (there is no `sh` yet): lines with shell syntax would fail.

## Success

`tilck.py --rung tools1`: every mark above, `[tools1] rootfs sha256 match 45f382c7fe05dd3f9982ee5016c02452d0c368e1dcfafb0baecb04a5d86367c7 (8059904 bytes)`, the same sha256 in two runs. Since plan 4 phase 1 (real modes in the tars, and the rebuilt `mkustar`): `bc280dca8eff8a6767a64777a0aa6907a65791f03fa2cc06bc634deafc3fbca0` (8062976 bytes, all 26 programs 0755), the whole run 46 s. Times from QEMU start: `make --version` of the make recipe 13 s, all six built at 37 s, `tar gzip ok` 40 s, `make ok` 40.6 s, done 52.6 s; the whole run 54 s. Built the same way on the host by a cross tcc with the same sysroot, `gzip` and `sed` are byte-identical to the guest's (`make`, `patch`, `bzip2` differ in a few dozen bytes). Since plan 4 phase 2 (tcc's `lr`/`sc` fix, 11-tcc7 README; musl's `malloc` reuses freed chunks now): `c7068b3659e4c8e633e13a4f151e4f1b719bee285d305c616ac5a2083a962130` (8062976 bytes), 35 s. Since plan 4 phase 3 (Tilck: shared file offsets, `abort()` in a handler; 12-tilck README): the same sha256, 39 s. Then with tcc's epilogue fix (11-tcc7 README): `bfdf32403015be80f7deac79748f3bfdff43af7a93f2a4270797d6ac9e8d32bc` (8103936 bytes), 34 s.

# Rung tools2

On top of `artifacts/rootfs-tools1.tar`, Tilck builds live-bootstrap's `coreutils-5.0`, `oyacc-6.6`, `bash-2.05b` (that order, `steps/manifest`) with the same frame as rung tools1, tests them and hands `/usr` back as `artifacts/rootfs-tools2.tar`.

```bash
python3 -u src/scripts/tilck.py --rung tools2
```

`RUNGS["tools2"]`: rootfs `artifacts/rootfs-tools1.tar`, steps `steps-tools2/` (its `run.kaem` is the script), the three tarballs below.

| File | URL | sha256 |
|---|---|---|
| `coreutils-5.0.tar.bz2` | https://mirrors.kernel.org/gnu/coreutils/coreutils-5.0.tar.bz2 | `c25b36b8af6e0ad2a875daf4d6196bd0df28a62be7dd252e5f99a4d5d7288d95` |
| `oyacc-6.6.tar.gz` | https://github.com/ibara/yacc/releases/download/oyacc-6.6/oyacc-6.6.tar.gz | `eb0866e740b79bd3a23e0ca47885eb3148aab18d77a4bedba96e979d8b4ebfe1` |
| `bash-2.05b.tar.gz` | https://mirrors.kernel.org/gnu/bash/bash-2.05b.tar.gz | `ba03d412998cc54bd0b0f2d6c32100967d3137098affdc2d32e6e7c11b163fe4` |

## Recipes (`steps-tools2/<pkg>/`)

Copies of live-bootstrap's `steps/<pkg>/` as for tools1; not copied besides `<pkg>.checksums` and `pass2.sh`: coreutils' `mk/pass2.mk` and `patches-pass2/` (pass 2 only), bash's `patches/extern.patch` and `patches/size.patch` (not applied by `pass1.kaem`), oyacc's `patches/meslibc.patch` (below). `diff -r` against the pin shows exactly the changes below; the `# Checksums` block is gone from all three, as in tools1.

- `coreutils-5.0`, `mk/main.mk`: `CFLAGS` + `-DHAVE_SYS_TYPES_H=1` (`lib/inttostr.h` declares `offtostr (off_t, …)`; musl's `inttypes.h` does not bring `off_t`), `-DHAVE_STRERROR=1 -DHAVE_DECL_STRERROR=1` (else `lib/error.c` uses `sys_errlist`/`sys_nerr`: link error), `-DHAVE_DECL_DIRFD=1` (was 0: `lib/dirfd.h` then declares `dirfd (DIR const *)` against musl's `dirfd (DIR *)`). Out of `LIB_SRC`: `getline` (musl's `ssize_t getline`/`getdelim` against `lib/getline.c`'s `int` ones), `dirfd` (musl has it), `strtoimax` and `strtoumax` (musl defines them in the same object as `strtol`: "link symbol 'strtoumax' defined twice").
- `coreutils-5.0`: new `patches/musl-getline.patch` (`lib/getline.h` does not redeclare `getline`/`getdelim` returning `int` when `__GLIBC__ < 2`) and `patches/install-no-chown.patch` (`install` calls `chown (path, -1, -1)` when neither `-o` nor `-g` is given; Tilck's `vfs_chown()` accepted only owner and group 0 and returned `EPERM` for -1 (until plan 4 phase 1, 12-tilck README; the patch stays), so every `install` failed; the patch skips the call when there is nothing to change, as `lib/makepath.c` and later coreutils do), applied in `pass1.kaem` after live-bootstrap's patches.
- `coreutils-5.0`: kept although written for mes libc: `mbstate.patch` (its `mbstate_t` typedef is the only one in `quotearg.c`: musl's comes with `wchar.h`, which `quotearg.c` includes only under `HAVE_WCHAR_H`), and the three `strcoll` → `strcmp` patches (harmless: C locale).
- `oyacc-6.6`: `mk/main.mk` `LIBS = -lgetopt` → `LIBS =` (musl has `getopt` in `libc.a`, there is no `libgetopt.a`); `patches/meslibc.patch` is neither copied nor applied (its line is gone from `pass1.kaem`): musl has `paths.h`, `mkstemp` and `sig_atomic_t`, and the patch only replaces them (`mkstemp` → `mktemp`). `tcc.patch` (`char *__progname;` instead of `extern`) is kept: it links with musl's `__progname`.
- `bash-2.05b`, `mk/common.mk`: `-DTERMIO_TTY_DRIVER` → `-DTERMIOS_TTY_DRIVER` (musl has no `termio.h`); `-DHAVE_TZNAME` → `-DHAVE_TM_ZONE` (`lib/sh/strftime.c` declares `extern int timezone` against musl's `long`; with `HAVE_TM_ZONE` it uses `tm_gmtoff`/`tm_zone`, which musl has); `-DHAVE_SYS_SIGLIST` → `-DHAVE_STRSIGNAL` (musl has no `sys_siglist`, but `strsignal`); `-Dendpwent\(x\)=0` dropped (musl's `pwd.h` declares `void endpwent (void)`, which the macro turned into `void 0;`); `+ -DHAVE_STDDEF_H` (`ptrdiff_t` in `lib/sh/snprintf.c`) and `-DHAVE_STRCASECMP` (`externs.h` declares `strncasecmp (…, int)` against musl's `size_t`).
- `bash-2.05b`, `mk/main.mk`: `-lgetopt` dropped from the `mksyntax` and `mksignames` links (as oyacc); `strtoimax` and `strtoumax` out of `SHLIB_FILES` (and `strtoumax` out of `SHLIB_ODD_FILES`): musl's, as for coreutils.

What stays as in live-bootstrap and is harmless here: coreutils' `-DENOTEMPTY=1 -DEPERM=1 -DMB_LEN_MAX=16`, which musl's headers redefine where they are included ("redefined" warnings); `-DLC_COLLATE=\"C\"`: `hard_locale (LC_COLLATE)` passes a string for an `int` and returns 0 (no `HAVE_SETLOCALE`). The pointer-to-`int` truncations tcc warns about (64-bit) are either of addresses below 2 GB in these static non-PIE programs (bash's `getcwd` into an `xmalloc`ed buffer and `ttyname`, and its own `shell_version_string` returning a static buffer, both called without a prototype because `config.h` is empty) or of values that are not used as pointers (`obstack_free` in coreutils' `remove.c`, `hard_locale`); bash's `BUILDVERSION` is a string assigned to an `int` (`bash --version` says `2.05b.0(0)`). `rm -r ../src` in coreutils' recipe still runs mescc-tools-extra's `rm` (`unable to delete file: -r !!!`, exit 0); from oyacc on `rm` is coreutils'.

Tilck bugs this rung found, fixed in `12-tilck/src/override/` (README there): `ramfs_futimens()` refused to set the times of a read-only file (tar 1.12 of coreutils 5.0 and bash 2.05b: "Could not change access and modification times: Permission denied"); `sys_utimensat()` gave the file uninitialized times (make: "has modification time … in the future", the "Clock skew detected" of tools1, gone now); execve() took at most one page and 32 arguments (`make: execvp: tcc: Argument list too long` for coreutils' compile lines), now 128 KB and no argument count limit (`#!` scripts: still 32 arguments); `read()`/`write()` of a regular file stopped at 12 KB (bash's `mkbuiltins` read `set.def` short and crashed). After the Tilck rebuild, rungs tcc and tools1 give the same rootfs sha256 as before.

## `steps-tools2/run.kaem`

Same frame as `steps/run.kaem`: recipe files to `/steps/<pkg>/` one `cp` each, the three tarballs to `/distfiles` (`DISTFILES=/distfiles`), `pkg=NAME`, `cd ${pkg}`, `kaem --file pass1.kaem`, `cd ..`. Then `ls --version`, `bash --version`, and two scripts run by the new bash, each printing its mark only if all its checks give the expected output (otherwise the failures and exit 1, which stops kaem):

- `tests/sh-test.sh` (`bash sh-test.sh` in `/build/sh`): pipeline `printf | sed | tr`, nested `$(…)`, `$?` after `false` and `true`, `for` + `case`, a function with `local` (called in the shell with its output redirected to a file, and in `$(…)`), `for`, `echo >` + `echo >>` + `cat`, `while read a b … done < file` → `bash ok`.
- `tests/coreutils-test.sh` (`bash coreutils-test.sh` in `/build/cu`): `mkdir -p`, `cat`, `cp`, `mv`, `rm`, `ls` (also of two directories), `install -m 755` and `chmod 640` (checked with `ls -l | cut -c1-10`), `/usr/bin/test` (`-d -f ! -e -gt`, and a false one), `expr`, `head`, `tail`, `wc -l`, `sort`, `uniq -c`, `tr`, `cut` → `coreutils ok`.

`/usr` is packed as in tools1, but written to `/dev/outbox` with `catm`: `cp` is coreutils' now (it may seek over what it takes for holes). `tilck.py` marks: `ls (coreutils) 5.0`, `GNU bash, version 2.05b.0(`, `bash ok`, `coreutils ok`, `rootfs sha256 `, `rung done tools2`, `rung exit 0`.

## Tools in `rootfs-tools2.tar`

`usr/bin/`: tools1's 26 programs plus coreutils' 61 (`install`, `basename`, `cat`, `chmod`, `cksum`, `cp`, `csplit`, `cut`, `dirname`, `echo`, `expand`, `expr`, `factor`, `false`, `fmt`, `fold`, `head`, `hostname`, `id`, `join`, `kill`, `link`, `ln`, `logname`, `mkfifo`, `mkdir`, `mknod`, `nl`, `od`, `paste`, `pathchk`, `pr`, `printf`, `ptx`, `pwd`, `readlink`, `rmdir`, `seq`, `sleep`, `sort`, `split`, `sum`, `tail`, `tee`, `tr`, `tsort`, `unexpand`, `uniq`, `unlink`, `wc`, `whoami`, `tac`, `test`, `touch`, `true`, `yes`, `ls`, `md5sum`, `mv`, `rm`, `sha1sum`; they replace mescc-tools-extra's `cp`, `chmod`, `mkdir`, `rm`), `yacc`, `yyfix` (a shell script), `bash` and `sh` (the same binary); 87 programs, 320 files. Still missing for `configure` scripts: `grep`, `awk`, `m4`, `cmp`/`diff`, `find`. Only `/usr` is packed; `rung-init` makes `/bin/sh` → `/usr/bin/bash` in the rungs that start from this rootfs (14-seed README). Since plan 4 phase 1 the modes travel with the files (`yyfix` and `yacc` 0555, the other programs 0755, the rest 0644), and `access()` checks them (`test -x`; before, it was always true).

## Success (tools2)

`tilck.py --rung tools2`: `[tools2] got 'ls (coreutils) 5.0' at 67.0s`, `'GNU bash, version 2.05b.0(' at 67.0s` (`2.05b.0(0) (bootstrap)`), `'bash ok' at 67.4s`, `'coreutils ok' at 68.0s`, `'rootfs sha256 ' at 81.3s`, `'rung done tools2' at 81.3s`; `[tools2] rootfs sha256 match f5342a2492b2160c6c7a35a5f24f94847393bde306494478e5f2c9bc7ee4bc07 (17133056 bytes)`, the same sha256 in two runs; the whole run 82–83 s. With the new Tilck, rungs tcc and tools1 still give `27f9fced…` and `45f382c7…`. Since plan 4 phase 1 (Tilck for long builds, real modes in the tars): `[tools2] rootfs sha256 match 52dbbfa44cc416f68fa964a7c899d20244a8751bcd2fc78ca5ce9c9f719186c6 (17136128 bytes)` in five runs in a row, 90–99 s each; `usr/bin/yyfix` `-r-xr-xr-x`. Since plan 4 phase 2 (tcc's `lr`/`sc` fix): `[tools2] rootfs sha256 match 3004307eb8b336e2de969f7d5e6f9765ae46a7ebfb596abd870a5bff3cd477de (17136128 bytes)`, 77 s. Since plan 4 phase 3 (12-tilck README): the same sha256, 77 s; then with tcc's epilogue fix (11-tcc7 README): `[tools2] rootfs sha256 match 92553b3278fb97ba004d10a96eaa8dc00b182c3a6f508e7f3ebe06921d22d825 (17266688 bytes)`, 76 s.
