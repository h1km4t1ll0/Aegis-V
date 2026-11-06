# 14 — seed for Tilck (built by the guest on Kronos)

After musl (stage 13), Kronos's `/bin/tcc7b` builds, against the guest-built musl, the programs a Tilck rung starts with: `tcc-m` (tcc mob with stage 11's changes and our `config.h`), `libtcc1.a`, `kaem` and the mescc-tools-extra tools, live-bootstrap's `checksum-transcriber` and `simple-patch`, stage 13's `mkustar`, and our `rung-init`. `seed-tree` (a Kronos program) lays them out with musl's sysroot and tcc's headers under `/build/seed/usr`, stage 13's `mkustar` packs `seed-rootfs.tar`. The host only plants bytes of the pinned archives and of this stage, then copies what the guest announces (`snap`) to `artifacts/`.

## Pinned sources (`build/sources/`, gitignored, `git archive --format=tar COMMIT`, no prefix)

stage0-posix `45d90f5955b6907dc6cdea9ebafce558359edcd3` (https://github.com/oriansj/stage0-posix, the submodule `seed/stage0-posix` of live-bootstrap `b1ceced7`) records the commits of its submodules; we archive the three we use (the superproject itself has no file we need):

| What | Pin | Archive | sha256 | License |
|---|---|---|---|---|
| mescc-tools (`kaem`) | https://git.savannah.nongnu.org/git/mescc-tools.git `5adfbf3364261a77109878a56b100aeeb6ef9ac4` | `mescc-tools-5adfbf33.tar` | `d4b110d50198758ec47051e2c462c9b6823ed6c52653ffc84d9d2d790df226d1` | GPL-3.0-or-later |
| mescc-tools-extra | https://github.com/oriansj/mescc-tools-extra `a151c245e512076971a3c85bb1502cf92cfa83b6` | `mescc-tools-extra-a151c245.tar` | `ebecaeed63a12535fb2e17464f3764dc246671301e64ed8b1e67ac3cda5e56db` | GPL-3.0-or-later |
| M2libc (`bootstrappable.[ch]` only) | https://github.com/oriansj/M2libc `68a23cfd05d5a355ba7a30c770d684cbe86fcc4e` | `M2libc-68a23cfd.tar` | `b091be0f0534fb7bc8104e5f5b52d9e691a9e1f6fc3344d394c0362bddaaf6b1` | GPL-3.0-or-later |
| live-bootstrap | https://github.com/fosslinux/live-bootstrap `b1ceced7ea8a819a26f23796f6dd7ae8496a33a4` | `live-bootstrap-b1ceced7.tar` | `203292a25e8f1ab86c982eddc5cc5990a8eddbd459252bb9191a5338e60cf30c` | GPL-3.0-or-later (`simple-patch.c`: MIT) |
| tcc mob | stage 11 | `tcc-mob-e23dabed.tar` | stage 11 README | LGPL-2.1 |

`src/scripts/kronos.py` (`SEED_TARS`) checks each sha256 before planting. Making an archive twice gives the same bytes (checked for M2libc and live-bootstrap).

## KFS layout (`collect_kfs_seed()`)

- `/src/seed/tcc/`: the 20 files `tcc.c` includes with `ONE_SOURCE` for riscv64 (list `TCCM_SRC`, from `tcc -MD` on a host), from the archive with stage 11's `src/override/` on top, and our `src/config.h`. A second copy, because `tcc.h` includes `"config.h"` from its own directory first, and `/src/tcc7/config.h` is stage 11's.
- `/src/seed/mescc-tools/Kaem/{kaem.c,variable.c,kaem_globals.c,kaem_globals.h,kaem.h}`, `/src/seed/M2libc/bootstrappable.{c,h}`, `/src/seed/mescc-tools-extra/<tool>.c` (the sources `PROGS` names), `/src/seed/live-bootstrap/{checksum-transcriber,simple-patch}.c` (from `steps/*/src/`); `src/override/` on top (it must replace a planted file).
- `/src/seed/rung-init.c`, `/src/seed-tools/seed-tree.c`.
- `/src/seed/rsp/<name>.rsp`: one tcc7b listfile per `PROGS` line, the whole command line (below); `/src/seed/SEED` lists them.
- `/src/seed/{LIBTCC1,libtcc1.rsp}` from `src/`; `/src/seed/ar.rsp` = `-ar rcs /build/seed/usr/lib/tcc/libtcc1.a` and the objects of `LIBTCC1` (the naming of `build_objs()`).
- `/src/seed/TCCINC`: tcc's headers (`include/*` of the archive + override, planted by stage 11 under `/lib/tcc7/include/`); `/src/seed/BUILT`: `lib/tcc/libtcc1.a` and every `PROGS` output under `usr/`, relative to `usr/`.

`kronos.py` prints `[seed] KFS …` (about 1.5 MB).

## Guest steps (`09-kronos/src/kmain.c` `build_seed()`, after `musl done`)

1. `build_objs("/src/seed/LIBTCC1", "@/src/seed/libtcc1.rsp", "/src/tcc7/lib/", "/build/seed/obj/", "libtcc1")`: UART `libtcc1 objs ok`; `/bin/tcc7b @/src/seed/ar.rsp` → `/build/seed/usr/lib/tcc/libtcc1.a`, UART `libtcc1 built`.
2. `/bin/tcc7b @RSP` for each line of `/src/seed/SEED`; a non-zero exit prints `seed progs fail <rsp>`; all built: `seed progs ok`. `snap rinit` (`/build/seed/rung-init`).
3. `/bin/seed-tree /build/seed/LIST /build/seed  /build/musl usr /build/musl/SYSROOT  /lib/tcc7 usr/lib/tcc /src/seed/TCCINC  - usr /src/seed/BUILT`: copies musl's sysroot (stage 13's `SYSROOT` list) to `usr/`, tcc's headers to `usr/lib/tcc/include/`, checks the built files, writes the listing; UART `seed-tree ok N files`.
4. `/bin/mkustar -e /build/seed/seed-rootfs.tar /build/seed /build/seed/LIST` (`-e`: modes from the content, Kronos's `stat()` says 0777 for every file): UART `mkustar ok N files`, `seed built`, `snap seed`. Then `seed done`.

`kronos.py` copies the snaps to `artifacts/rung-init.elf` and `artifacts/seed-rootfs.tar` and checks them (`check_user_elf()`, `check_seed()`): every `usr/bin/*` and `rung-init` is an ELF64 riscv64 `ET_EXEC` without `PT_INTERP`/`PT_DYNAMIC`, each `PT_LOAD` with `p_align` 0x1000 and page-congruent offset/address, below `0x2000000000` (stage 12's user ELF rules); the tar has every `PROGS` program, `usr/lib/{libc.a,crt1.o,crti.o,crtn.o}`, `usr/lib/tcc/libtcc1.a` (an `ar` archive), `usr/lib/tcc/include/tccdefs.h`, musl's headers.

A program line (`kronos.py` `SEED_CC_HEAD`, `SEED_CC_TAIL`; stage 13's template, with `libtcc1.a` instead of `lib-arm64.c`):

```
-o /build/seed/OUT -nostdinc -I /build/musl/include -I /lib/tcc7/include -nostdlib -static -Wl,-Ttext=0x10000
  /build/musl/lib/crt1.o /build/musl/lib/crti.o  FLAGS+SOURCES of the PROGS line
  /build/musl/lib/libc.a /build/seed/usr/lib/tcc/libtcc1.a /build/musl/lib/crtn.o
```

The order of the libraries is tcc's own for a static link (`crt1 crti OBJS -lc libtcc1.a crtn`): `libtcc1.a` gives musl's long double code its binary128 helpers and tcc-m its `__clear_cache` (`armflush.c`, called by `tccrun.c`).

## `tcc-m` (`/usr/bin/tcc` of the seed)

`src/config.h` = stage 11's with the paths of the seed: `CONFIG_TCCDIR "/usr/lib/tcc"`, `CONFIG_TCC_SYSINCLUDEPATHS "{B}/include:/usr/include"`, `CONFIG_TCC_LIBPATHS "{B}:/usr/lib"` (mob's default form: `tcc_add_support()` looks for `libtcc1.a` in the library paths, so with plain `"/usr/lib"`, as first built, rung tcc failed with `tcc: error: file 'libtcc1.a' not found`), `CONFIG_TCC_CRTPREFIX "/usr/lib"`, `CONFIG_TCC_PREDEFS 0` (`tccdefs.h` is read from `/usr/lib/tcc/include` at run time), `CONFIG_TCC_STATIC 1`, `TCC_LIBTCC1 "libtcc1.a"`. So on the seed `tcc -static -o X X.c` uses `/usr/lib/tcc/include` then `/usr/include`, links `/usr/lib/crt1.o crti.o`, `/usr/lib/libc.a`, `/usr/lib/tcc/libtcc1.a`, `crtn.o`; `tcc -c`, `tcc -ar rcs` work as in live-bootstrap's recipes.

- **Static by default: no.** mob has no setting for it (`CONFIG_TCC_STATIC` only removes `dlopen` from tcc itself); without `-static` tcc-m writes a dynamic executable, which Tilck cannot run. live-bootstrap's recipes pass `-static` (`make-3.82/pass1.kaem`, `LDFLAGS = -static` in `gzip`, `tar`, `sed`, `patch`, `LDFLAGS="-static"` for `bzip2`), as we will.
- **Link address: no `-Ttext` needed.** mob's riscv64 static layout starts at `ELF_START_ADDR` 0x10000 with `ELF_PAGE_SIZE` 0x1000; on a host cross tcc of the same sources a musl hello linked with and without `-Wl,-Ttext=0x10000` is byte-identical (4 `PT_LOAD`, `p_align` 0x1000), a valid Tilck user ELF.

## `libtcc1.a`

`src/LIBTCC1`: what mob's `lib/Makefile` puts in a native riscv64 `libtcc1.a` (`OBJ-riscv64`, `CONFIG_backtrace`/`bcheck` off): `lib-arm64.c stdatomic.c atomic.S builtin.c alloca.S alloca-bt.S armflush.c dsohandle.c tcov.c`, each `tcc7b @libtcc1.rsp -I /src/tcc7/lib -c`, then `tcc7b -ar rcs`.

## Programs (`src/PROGS`: output, then flags and sources)

| `usr/bin/` | Sources | Notes |
|---|---|---|
| `tcc` | `/src/seed/tcc/tcc.c` `-DONE_SOURCE=1` | tcc-m |
| `kaem` | `Kaem/{kaem,variable,kaem_globals}.c` + `bootstrappable.c` | mescc-tools' `makefile` |
| `catm` `cp` `chmod` `match` `mkdir` `rm` `replace` `sha256sum` `untar` `ungz` `unbz2` `unxz` | `<tool>.c` + `bootstrappable.c` (`catm.c` alone) | mescc-tools-extra's `makefile` (`-D_GNU_SOURCE`) |
| `checksum-transcriber`, `simple-patch` | `steps/*/src/*.c` of live-bootstrap + `bootstrappable.c` | |
| `mkustar` | stage 13's `src/mkustar.c` | for packing `/usr` in a rung |

and `rung-init` (`/build/seed/rung-init`, not in the tar).

Built as ordinary C on musl, as mescc-tools' and mescc-tools-extra's `makefile`s build them with gcc: each with `M2libc/bootstrappable.c` (`require`, `match`, `int2str`, …), M2libc's libc is not used. Changes for that:

- `src/override/mescc-tools/Kaem/kaem.h`: `#include "../M2libc/bootstrappable.h"` → `#include "M2libc/bootstrappable.h"` (found through `-I /src/seed`): Kronos does not resolve `..` in paths.
- `simple-patch.c` calls `open`/`read`/`write`/`close` with only `string.h`, `stdio.h`, `stdlib.h` (M2libc's `stdio.h` brings them): `-include fcntl.h -include unistd.h`.
- `src/override/live-bootstrap/simple-patch.c`: it reads each file with one `read()` and writes the result with one `write()`. Tilck's `read`/`write` move at most `IO_COPYBUF_SIZE` (3 pages, 12 KB) per call, a legal short count, so on Tilck a file over 12 KB fails with "Could not read file", and a longer result is cut short without any error. Both calls are now loops (marked `Aegis-V`). On a host with `read`/`write` capped at 12 KB, the original fails on an 80 KB file and the fixed one patches it correctly.
- `checksum-transcriber.c` includes `<bootstrappable.h>`: `-I /src/seed/M2libc`.
- `untar.c` calls `symlink` undeclared (implicit `int`, the result is an `int`; its `makefile` passes `-Wno-implicit-function-declaration`); `unxz.c` warns about pointer types (`-Wno-incompatible-pointer-types` there). Left as is.
- Not built: `wrap` (`unshare`/`mount` namespaces, which Tilck lacks), `sha3sum` (not used by the recipes).

Command lines (mescc-tools-extra / live-bootstrap): `untar --file A [--verbose] [--non-strict]` (writes with `fopen`, no modes), `ungz|unbz2|unxz --file IN --output OUT`, `sha256sum FILE… [-o OUT]` / `sha256sum -c LIST`, `catm OUT IN…`, `mkdir [-p] [-m MODE] DIR…`, `cp SRC DST` (no mode copy), `chmod MODE FILE…`, `rm FILE…`, `replace --file F --match-on S --replace-with R [--output O]`, `match A B` (exit 0 if equal), `checksum-transcriber FILE` (→ `FILE.SHA256SUM`, paths from `DISTFILES`), `simple-patch FILE BEFORE AFTER`, `kaem [--verbose] [--strict] [--file F]`, `mkustar OUT ROOT LIST`.

## `rung-init` (`src/rung-init.c`, `artifacts/rung-init.elf`)

`/initrd/bin/init` of a rung: opens `/dev/console` as fds 0–2 (Tilck gives init none); unpacks `/initrd/rootfs.tar` into `/` with its own ustar reader (types `5` and `0` only; `mkdir -p` of parents; the header's permission bits, which `mkustar` takes from `stat()` (plan 4 phase 1; before, `mkustar` wrote 0644 for every file and `rung-init` added `0111` to an ELF `ET_EXEC`, so a `#!` script such as `yyfix` lost its x bits from one rung to the next); writes in a loop, because Tilck's `write()` takes at most 12 KB per call. The first boot failed with `rung-init fail: write /usr/include/elf.h`, the first file over 12 KB), prints `rung-init: N files, M dirs from /initrd/rootfs.tar`; if `/usr/bin/bash` is a regular file, `mkdir /bin` and the symlink `/bin/sh` → `/usr/bin/bash`, UART `rung-init: /bin/sh -> /usr/bin/bash` (from rung tools2's rootfs on; only `/usr` is packed, so every rung makes it anew); `chdir("/")`; forks and runs `/usr/bin/kaem --verbose --strict --file /initrd/steps/run.kaem` with exactly `PREFIX=/usr BINDIR=/usr/bin LIBDIR=/usr/lib INCDIR=/usr/include DISTFILES=/initrd/distfiles ARCH=riscv64 PATH=/usr/bin`; waits and prints `rung exit N` (exit code; 128 + signal if killed; 127 if `execve` failed). Since plan 4 phase 2 it reaps every child as an init must (`waitpid(-1)` until kaem's pid comes back, and after `rung exit N` in its endless loop): Tilck reparents orphans to pid 1, and an unreaped zombie keeps its cwd referenced, so after findutils' `configure` (its `` `(hostname || uname -n) | sed 1q` `` can leave an orphan) `rm -rf` of the build tree failed with `Resource busy` (ramfs `rmdir` refuses a directory with references). A setup failure prints `rung-init fail: <what> <path> (errno E)` and `rung exit -1`. Then it sleeps forever (pid 1 must not exit). Checked on a host (macOS build of the same file with the paths redirected to `/tmp`): it unpacks a seed tar byte for byte (250 files, executables 0755, the rest 0644).

## `seed-rootfs.tar` (`artifacts/seed-rootfs.tar`)

POSIX ustar from `mkustar -e` (mtime 0, uid/gid 0, parents first, directories 0755, ELF executables (`usr/bin/*`) 0755, every other file 0644):

- `usr/bin/`: `tcc kaem catm cp chmod match mkdir rm replace sha256sum untar ungz unbz2 unxz checksum-transcriber simple-patch mkustar`;
- `usr/lib/`: `libc.a crt1.o crti.o crtn.o` (musl), `usr/lib/tcc/libtcc1.a`, `usr/lib/tcc/include/` (tcc's 10 headers, stage 11's `tccdefs.h`);
- `usr/include/`: musl's 218 installed headers.

## Rungs on Tilck (`tilck.py --rung NAME`)

`src/scripts/tilck.py` `RUNGS` is the table of rungs. Each entry gives `rootfs` (the tar unpacked into `/`: the seed for the first rung, otherwise the previous rung's `rootfs-NAME.tar`), `steps` (a directory; every file goes to `/initrd/steps/` with its relative path, `run.kaem` is the script), `extra` (a function returning more `(initrd path, bytes)`), `distfiles` (`(file in build/sources, sha256)`, checked by the host and placed at `/initrd/distfiles/FILE`), `marks` (in order; the host appends `rung exit 0`), `timeout` and `out` (the directory for `rootfs-NAME.tar`). The initrd (FAT32, `lib/fat32.py`) holds `bin/init` = `artifacts/rung-init.elf`, `rootfs.tar`, `steps/…`, `distfiles/…`. The host waits for the marks, fails on `PANIC`, `panic:`, `init fail` (which also matches `rung-init fail`) or any other `rung exit `, copies the outbox dump to `OUT/rootfs-NAME.tar` and checks that its sha256 equals the guest's `rootfs sha256 HEX` (`[NAME] rootfs sha256 match …`). Each `got` line shows the seconds since QEMU started.

kaem conventions that work under rung-init (`--verbose --strict`, cwd `/`, environment above): one command per line, `#` comments, `${BINDIR}`/`${LIBDIR}` expansion, the builtins `cd`, `echo`; `--strict` aborts on the first non-zero exit (`Subprocess error N` / `ABORTING HARD`, then `rung exit 1`). Programs are found through `PATH=/usr/bin`; names starting with `/` or `.` are run as they are. There is no redirection: tools that write files take an output argument (`sha256sum -o`, `catm OUT …`, `filelist OUT …`). `/initrd` is read-only; work in `/build` or `/tmp` (ramfs). Files written by `cp` (stdio, 0644) need `chmod 755` to be executed; the mode a file has in `/usr` is the mode it has in the next rung (`mkustar` and `rung-init` keep it).

### Rung tcc (`steps/tcc/`)

`tilck.py --rung tcc`: rootfs `artifacts/seed-rootfs.tar`; `steps/tcc/` (`run.kaem`, `hello.c`, `filelist.c`, `sha256-prefix`); `extra` = `steps/tcc-mob/`, the bytes the seed's tcc-m was built from (`kronos.py` `TCCM_SRC` of the pinned mob archive with stage 11's overrides, `src/config.h`, and `lib/` with the `src/LIBTCC1` sources). `run.kaem`:

1. `tcc -static -o /build/hello hello.c` (tcc-m of the seed), `/build/hello` → `tilck hello ok`.
2. `tcc -static -DONE_SOURCE=1 -o /build/tcc-m2/tcc tcc-mob/tcc.c`, then the same with `/build/tcc-m2/tcc` → `/build/tcc-m3/tcc`. There is no `cmp` in the seed: `sha256sum -o /build/tcc-m2.sum tcc` in `/build/tcc-m2` and `sha256sum -c /build/tcc-m2.sum` in `/build/tcc-m3` (the same relative name) → `tcc: OK`, `tccm fixed ok`.
3. `cp` tcc-m3 to `${BINDIR}/tcc` (`chmod 755`); the nine `LIBTCC1` objects with `tcc -c`, `tcc -ar rcs ${LIBDIR}/tcc/libtcc1.a …`.
4. `tcc -static -o ${BINDIR}/filelist filelist.c` (with the new tcc and libtcc1.a; kept for the next rungs). `filelist OUT DIR` lists the regular files under `DIR` (recursive `opendir`/`readdir`/`stat`, symlinks followed, sorted bytewise), one per line: `mkustar`'s LIST, since there is no `find`. From `/`: `filelist /build/usr.list usr`, `mkustar /build/rootfs.tar . /build/usr.list`, `cp /build/rootfs.tar /dev/outbox` (one close, one dump), `sha256sum -o /build/rootfs.sum /build/rootfs.tar`, `catm /dev/console sha256-prefix /build/rootfs.sum` prints `rootfs sha256 HEX  /build/rootfs.tar` (`sha256-prefix` is `rootfs sha256 ` without a newline: kaem's `echo` cannot join it with the sum), `echo rung done tcc`.

`rootfs-tcc.tar` (`artifacts/`): the seed's 250 files with `usr/bin/tcc` = tcc-m3 and the rebuilt `usr/lib/tcc/libtcc1.a`, plus `usr/bin/filelist`.

Problems the first rung found, fixed: `rung-init` and stage 13's `mkustar` treated Tilck's 12 KB short `write()` as an error, and `simple-patch` assumed one `read`/`write` per file (above); `config.h` had no `{B}` in `CONFIG_TCC_LIBPATHS` (above); Tilck's `copy_from_user()` rejected a NULL pointer for 0 bytes, so every musl `fclose()` failed (12-tilck README, `kernel/user.c`).

## Our files

- `src/config.h`, `src/PROGS`, `src/LIBTCC1`, `src/libtcc1.rsp`, `src/rung-init.c`, `src/override/` — see above.
- `steps/tcc/` — rung tcc, see above.
- `src/seed-tree.c`: `seed-tree OUTLIST ROOT [SRCDIR PREFIX LIST]...`, a Kronos program (`t7_cc()`); `-` as SRCDIR checks that the file is already there.

## Success

Kronos UART `libtcc1 objs ok`, `libtcc1 built`, `seed progs ok`, `seed-tree ok`, `mkustar ok`, `seed built`, `seed done`; `kronos.py` prints `[snap] rinit …`, `[snap] seed …`, a `[seed] usr/bin/<prog> …` line per program and `[seed] seed-rootfs.tar … files`.

First gate run: `seed-rootfs.tar` 5 356 544 bytes, 250 files (17 in `usr/bin`, 218 in `usr/include`), `rung-init.elf` 65 459 bytes; the whole `--tcc-self-k` run took 8.5 min, the seed part well under a minute of it (KFS 17.6 MB). As a cross-check, the same `PROGS` lines run by a host cross tcc of the same mob sources give byte-identical `usr/bin/*` except `tcc` (its `__FILE__` strings carry the host paths). After the fixes of rung tcc: `seed-rootfs.tar` 5 357 568 bytes (still 250 files), `rung-init.elf` 65 619 bytes, `--tcc-self-k` 9 min.

Rung tcc: `tilck.py --rung tcc` UART `tilck hello ok`, `tcc: OK`, `tccm fixed ok`, `filelist ok 251 files`, `mkustar ok 251 files`, `outbox 0x240000000 5443584`, `rootfs sha256 27f9fced…`, `rung done tcc`, `rung exit 0`; host `[tcc] rootfs sha256 match 27f9fcedbb80374cd2e5c16325c9e22acdce10ad1bdf9bcf598f70453d1e1252 (5443584 bytes)`. Times from QEMU start: hello 0.7 s, tcc-m2 and tcc-m3 built and compared 6.9 s, done 14.8 s; the whole run 16 s. Two runs gave the same sha256. Since plan 4 phase 1 (modes in the tars, the rebuilt `mkustar`; `seed-rootfs.tar` 5 360 640 bytes, `rung-init.elf` 68 683 bytes): `[tcc] rootfs sha256 match 1782b3b7f73224b7d36b2eee6105f9e9efc2d131e09eae9a7a7b403ae79393d9 (5446656 bytes)`, the 18 programs 0755, the rest 0644; 14 s. Since plan 4 phase 2 (tcc-m's `lr`/`sc` encodings fixed, so musl's atomics and `malloc` work; 11-tcc7 README): `[tcc] rootfs sha256 match 489a8cc792ef11272dfc0ad11b9207955ed70ba31b4b4b29c17ed391483a8f67 (5446656 bytes)`, 13 s. Since plan 4 phase 3 (Tilck: shared file offsets, `abort()` in a handler; 12-tilck README; `seed-rootfs.tar` 5 360 640 bytes, `rung-init.elf` 68 747 bytes): the same sha256, 13 s. Then with tcc's epilogue fix (11-tcc7 README `epilog.c`; everything rebuilt, `seed-rootfs.tar` 5 384 704 bytes, `rung-init.elf` 69 467 bytes): `[tcc] rootfs sha256 match 1b48342f4d5d3de92dedcde76cab0264f33de4dfefd847f13f54419e2ed908d5 (5471744 bytes)`, 13 s.
