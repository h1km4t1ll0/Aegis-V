# Independent checksum provenance

Fetched over verified HTTPS on 2026-11-19. Each archive was already downloaded
from its official GNU URL in `../sources`. Its locally calculated SHA256 was
compared with the corresponding published distro recipe below; all matched.
The saved files are verbatim copies, including recipes' own provenance comments.
The guest verifies the same pins again with checksum-transcriber/sha256sum.

| Archive | Published independent SHA256 recipe | Saved copy |
| --- | --- | --- |
| bash-4.4.tar.gz | [Buildroot 2017.02](https://raw.githubusercontent.com/buildroot/buildroot/2017.02/package/bash/bash.hash) | bash-buildroot-2017.02.hash |
| sed-4.8.tar.xz | [Buildroot 2020.05](https://raw.githubusercontent.com/buildroot/buildroot/2020.05/package/sed/sed.hash) | sed-buildroot-2020.05.hash |
| bison-3.0.4.tar.gz | [Nixpkgs 17.03](https://raw.githubusercontent.com/NixOS/nixpkgs/17.03/pkgs/development/tools/parsing/bison/3.x.nix) | bison-nixpkgs-17.03.nix |
| bc-1.07.1.tar.gz | [Buildroot 2020.05](https://raw.githubusercontent.com/buildroot/buildroot/2020.05/package/bc/bc.hash) | bc-buildroot-2020.05.hash |

This is independent published hash comparison, not a claim that this run
verified GNU signatures itself. Buildroot's bash and sed recipes state that
their authors checked the corresponding PGP signatures.

2026-11-19 Tilck compatibility: sed 4.8's native `gl_FUNC_NANOSLEEP`
configure test arms `alarm(1)` and sleeps for `TIME_MAX` seconds, expecting
`EINTR` and a large remainder. The pinned Tilck source declares
`CREATE_STUB_SYSCALL_IMPL(sys_setitimer)` in
`include/tilck/kernel/syscalls.h:180`; musl's RISC-V `alarm()` uses
`setitimer`. The real streamed configure run remained in this probe while
its shell heartbeat continued (`artifacts/linux-tools-nanosleep-stall-uart.log`).
The guest GCC 9.5-built `tests/nanosleep-probe.c` confirmed
`setitimer: -1, errno ENOSYS (38)` and a one-nanosecond sleep returning 0
(`artifacts/nanosleep-probe.log`, kernel SHA256
`2de08c783cd13b18755aae0fb71ac14294c8826b5ee06c95f3ebec366c75f6fc`).
The recipe requires this API probe to pass before passing the accurately
negative `gl_cv_func_nanosleep='no (mishandles large arguments)'` result to
sed configure. This selects its GNU replacement path and
`HAVE_BUG_BIG_NANOSLEEP=1`, checked in the generated config header; it does
not claim native success, change thread settings, or replace any archive
hash check. No setitimer implementation is added for this optional probe.

The same pinned sed configure script also tests `sleep` by arming
`alarm(1)` and sleeping for 50 days, expecting a large remaining count.
The next streamed run reached and stayed in that exact probe
(`artifacts/linux-tools-sleep-stall-uart.log`). The same confirmed
`setitimer=ENOSYS` prerequisite makes that test unable to terminate too.
`gl_cv_func_sleep_works=no` therefore selects GNU's `REPLACE_SLEEP` path,
without claiming working alarm delivery. Source inspection found only
these sleep/nanosleep runtime uses of `alarm()` in this configure script.
