# Guest-built disk tools and two-boot persistence

This directory supplies the guest recipe for the kernel phase. The host driver
`src/scripts/linux.py` now wires disk modes to the existing `kernel/smoke.sh`
command-line interface. Build every executable in the riscv64
guest using the existing `/usr` GCC 9.5.0/musl toolchain. Formatting and mounting
are Linux guest operations; Tilck can build the tools. Do not run the recipe on
the host, launch another QEMU during the active build, or format an image now.

## Source and provenance

`sources` pins the official kernel.org tytso release:

```
e2fsprogs-1.45.7.tar.xz
SHA256 62d49c86d9d4becf305093edd65464484dc9ea41c6ff9ae4f536e4a341b171a2
https://mirrors.edge.kernel.org/pub/linux/kernel/people/tytso/e2fsprogs/v1.45.7/e2fsprogs-1.45.7.tar.xz
```

The local `build/sources/e2fsprogs-1.45.7.tar.xz` was independently hashed and
matches. The exact URL/hash pair also appears in
`steps/e2fsprogs-1.45.7/sources` inside
`build/sources/live-bootstrap-b1ceced7.tar` (SHA256
`203292a25e8f1ab86c982eddc5cc5990a8eddbd459252bb9191a5338e60cf30c`).
That archive is supporting provenance, not a second e2fsprogs payload. No claim
of signature verification or a fresh network retrieval is made.

`source-checksums.sha256` records 24 inspected release inputs, including
configure, its source, Makefiles, generators, the shipped Unicode data header,
and the default filesystem profile. It does not authenticate every source file.
The integrating driver must hash the complete pinned archive before supplying
and extracting it inside the guest. `DISTFILES`, if set for `build.sh`, must
contain that archive; the script then repeats its full hash check in the guest.
With only an already extracted source tree, it checks the 24 inputs and logs
that complete-archive verification remains the caller's responsibility.

## Concrete build recipe

Supply this directory as, for example, `/steps/disk`, and extract the complete
release in the guest into `/build/e2fs-src` with the top directory stripped.
Use the existing guest `unxz` and guest-built `paxx`; do not transplant binaries
or configured build trees from the host. Neither `/build/e2fs-build` nor
`/build/disk-tools` may already exist, even after a failed attempt.

```sh
# Inside Tilck or Linux, with /usr restored from the GCC 9.5/tools checkpoint:
/bin/sh /steps/disk/build.sh
```

The script creates a separate `/build/e2fs-build`, runs configure, `make -j1
subs`, six explicit archive targets, then `make -j1 -C misc mke2fs.static
mke2fs.conf`. It copies only the static formatter and its upstream configuration
into `/build/disk-tools/sbin/mke2fs` and `etc/mke2fs.conf`. The private archives
are `libcom_err.a`, `libuuid.a`, `libblkid.a`, `libe2p.a`, `libsupport.a`, and
`libext2fs.a`. No libss, fsck, badblocks, resize tools, FUSE, docs, or full
top-level build/install are requested.

All configure switches were checked in 1.45.7's shipped configure and
configure.ac, rather than inferred from another e2fsprogs version:

| Switches | Purpose |
| --- | --- |
| `--enable-option-checking=fatal` | Reject unknown switches. |
| `--build=riscv64-unknown-linux-musl --host=riscv64-unknown-linux-musl` | Native guest build, bypass Tilck config.guess. The shipped config.sub accepts RISC-V. |
| `--prefix=/build/disk-tools --sysconfdir=/build/disk-tools/etc` | Bundle paths; staging uses explicit copies. |
| `--enable-libuuid --enable-libblkid` | Use bundled libraries; disabling them requires external replacements/pkg-config. |
| `--disable-elf-shlibs --disable-bsd-shlibs --disable-profile` | Static private archives only. |
| `--disable-nls --disable-threads --disable-tls --disable-uuidd` | Avoid gettext, thread/TLS and daemon dependencies. |
| `--disable-backtrace --disable-testio-debug` | Remove optional diagnostic paths. |
| `--disable-debugfs --disable-imager --disable-resizer --disable-defrag --disable-fuse2fs` | Omit unrelated programs and their optional library code. |

`CC='gcc -static'`, `BUILD_CC='gcc -static'`, `CFLAGS='-O2 -g0'`, and
`LDFLAGS=-static` apply to native generators as well as the final link. There is
no guessed `--enable-static` or `--disable-shared` option. The real static target
uses `LDFLAGS_STATIC`, which configure derives from LDFLAGS and its static-link
probe. The actual configure cache variables `ac_cv_lib_magic_magic_file=no`,
`ac_cv_lib_dl_dlopen=no`, and `ac_cv_func_dlopen=no` intentionally remove optional
libmagic/dlopen support; they do not assert untested syscall functionality.
No udev/systemd/cron installation runs, so no install-directory flags are needed.

Native requirements:

- GCC 9.5.0, musl headers/static libc, binutils including ar/ranlib/readelf, make,
  `/bin/sh`, awk, sed, grep, expr, tr, cmp, and ordinary checkpoint file tools.
  Configure executes its native probes in Tilck or Linux, not in cross mode.
- `subs` builds and executes `util/subst`, generates the `compile_et` shell/awk
  tool, and generates error/type headers. `libext2fs` builds and executes
  `gen_crc32ctable`. `misc` uses awk to generate `default_profile.c`. All execute
  inside the guest; shipped configure and `utf8data.h` are retained. Autoconf,
  libtoolize, bison/flex and Unicode downloads are unnecessary for these targets.
- The exact native Linux build passed with GCC 9.5 and static ELF checks for
  util/subst, gen_crc32ctable and mke2fs. Source timestamps are normalized to
  epoch after hash checks: this minimal Linux starts CLOCK_REALTIME in 1970,
  and future release mtimes otherwise cause Makefile regeneration loops.
  The linux-ready checkpoint also rebuilds gawk without the early -I vms
  header path, so file redirection uses Linux open flags. Both fixes retain
  real configure probes, generators and source contents.
- The formatter needs working Linux block I/O, large-file seeks, fsync/ioctls,
  `/proc`, and device nodes at runtime. UUID generation can consult `/dev/urandom`;
  supply it in Linux. No Tilck mkfs/mount operation is attempted.

`logs/build.log`, `config.log`, `MCONFIG`, `config.h`, version output, ELF
inspection results and output hashes are kept with the copied recipe/provenance.
The runtime gate checks RISC-V ELF with neither INTERP nor NEEDED for mke2fs and
both executable generators, then runs only `mke2fs -V`. This is not an upstream
test-suite run and does not create a filesystem.

To retain the small tool bundle across checkpoint/boot boundaries, main can copy
`/build/disk-tools` to `/usr/lib/aegis/disk-tools` inside the guest before packing
`/usr`. Keep `/build/e2fs-build` outside the bundle. In Linux set
`DISK_TOOLS=/usr/lib/aegis/disk-tools`; the test explicitly sets `MKE2FS_CONFIG`,
so relocation does not require rebuilding the formatter.

## New raw disk and Linux attachment

Use a **new** raw file, 4 GiB = 4294967296 bytes (QEMU's `4G`), exclusively opened.
`linux.py --new-disk PATH` opens the image with `xb` and truncates only that
newly created file to 4 GiB. It generates a 20-byte `aegis-<14 lowercase hex>`
token with `secrets.token_hex(7)` and exclusively creates `PATH.owner.json`.
That JSON records version 1, the canonical `raw_path`, `size_bytes` and `token`.
The host does not compile or format anything. Keep the metadata and use the
same image for both boots. The two-boot test passed on linux-data-03.raw;
choose another name to repeat first formatting.
Choose a fresh basename; never reuse an existing rootfs, outbox, or disk image.

```sh
python3 -u src/scripts/linux.py --test \
    --rootfs src/stages/21-linux/artifacts/rootfs-linux-ready.tar \
    --new-disk build/aegis-persist-4g.raw --timeout 3600
```

An interrupted/failed creation can leave a new empty sidecar or incomplete new
image; stop and inspect them, then choose a new name. Do not automatically delete
or overwrite either file. No partition table or host mkfs is used.

The driver attaches the disk with `virtio-blk-device,serial=TOKEN` and adds
`aegis.disk-mode=first aegis.disk-token=TOKEN` to the boot metadata command line.
`kernel/smoke.sh` then builds e2fsprogs from the pinned source inside Linux and
runs the explicit first-run persistence script. A run must emit all normal
Linux markers plus `disk-persistence boot1 PASS owner=TOKEN`, and QEMU must
exit zero. The command line is checked against the 199-byte payload limit of
the boot metadata instead of silently truncating it.

`--verify-disk PATH` loads the sidecar and checks its version, canonical raw
path, recorded and actual 4 GiB sizes, and token syntax before booting. It
reuses the serial and passes `aegis.disk-mode=verify aegis.disk-token=TOKEN`,
requiring `disk-persistence boot2 PASS owner=TOKEN` plus all normal Linux
markers. Missing or invalid metadata fails; it never falls back to formatting.
`--disk PATH` only attaches an existing raw file, passes no disk-test bootargs,
and requests no formatting. The three disk options are mutually exclusive.
Both `--test` and `--shell`, and the existing `--rootfs` option, remain available.
`--timeout` must be positive and controls the `--test` run without an automatic
disk-specific extension; choose enough time for native compilation and the
blank scan. Interactive `--shell` keeps its existing unlimited wait until exit.

The one-hart QEMU command uses `-cpu rv64,sstc=false`: our SBI implements TIME
but does not enable `menvcfg.STCE`. Linux 6.6's RISC-V timer driver uses stimecmp
when the DTB advertises SSTC, so this boot deliberately uses the SBI TIME path.
No firmware extension is added.

Attach only this test disk, once, as `/dev/vda`, and give one QEMU exclusive
access. Avoid `-snapshot`, `snapshot=on`, backing overlays, read-only attachment,
and `cache=unsafe`: both boots must use the same writable raw bytes. The driver
passes the serial for `--new-disk` and `--verify-disk`; plain `--disk` also
preserves a matching owner sidecar's serial but requests no disk test.

Verify the **resolved** kernel config has `CONFIG_BLOCK=y`, `CONFIG_VIRTIO=y`,
`CONFIG_VIRTIO_MMIO=y`, `CONFIG_VIRTIO_BLK=y`, `CONFIG_EXT4_FS=y`,
`CONFIG_DEVTMPFS=y`, `CONFIG_PROC_FS=y`, `CONFIG_SYSFS=y`, and
`CONFIG_SYSCTL=y`/`CONFIG_PROC_SYSCTL=y` for the Linux boot ID. Ext4 dependencies
must resolve built-in too; modules are unavailable. Mount devtmpfs on `/dev`,
proc on `/proc`, sysfs on `/sys`. The script checks these interfaces instead of
assuming that requesting config symbols made them effective.

## Two distinct QEMU boots

Restore the guest-built tools and this script in each boot. The supplied minimal
BusyBox already requests mount/umount/dd; the test also needs the checkpoint's
awk, cmp, sha256sum (`-o` and `-c`), and ordinary file tools in `/usr/bin`.
It uses successful Linux umount to flush writes, so it does not require the sync
applet absent from the current BusyBox fragment. Keep PID 1's normal sync and
poweroff path. Use an isolated guest with no active swap.

Boot one uses the `--new-disk` command above. The existing smoke script restores
the toolchain, extracts the pinned e2fsprogs archive, builds the formatter and
runs `/disk/persistence.sh --first-run-blank TOKEN` inside Linux.

The script requires an exact size and matching virtio serial, no partitions,
holders or mounts (checked by major:minor), an exclusive mountpoint, and a full
streaming comparison against exactly 4 GiB of zero bytes. The comparison uses
BusyBox dd and checkpoint diffutils 2.7 cmp; no 4 GiB allocation or temporary
file is needed. The checkpoint's mescc-tools-extra sha256sum buffers whole files
and must not be used to hash this device. cmp 2.7 has no newer `-n`/`--bytes`
switch, so the zero stream has a finite length and both inputs must end together.
A short dd/read fails the comparison. This scan can be slow under emulation;
allow it to finish. Any nonzero byte, including an existing filesystem or the
first run's data, rejects mkfs. There is no force option or implicit retry.
It uses the upstream ext4 profile (including extents, journal, metadata checksums
and 64bit), 4096-byte blocks, 256-byte inodes, zero reserved percentage, and
`-E nodiscard,lazy_itable_init=0,lazy_journal_init=0`; these extended options
exist in 1.45.7's `misc/mke2fs.c`. It writes an owned sentinel, token-specific
payload of over 1 MiB, the first boot ID and checksums, verifies them and cleanly
unmounts. Require `disk-persistence boot1 PASS` and exit status zero.

The existing PID 1 syncs and powers off after the test. If using `--shell`, exit
the interactive shell to reach that path. Wait for QEMU to exit, and preserve
`src/stages/21-linux/artifacts/uart-linux.log` as the first-boot evidence before
the next run replaces it. Start a **new QEMU process** with the same raw path.
Do not recreate the image or repeat the first-run mode. Boot two:

```sh
python3 -u src/scripts/linux.py --test \
    --rootfs src/stages/21-linux/artifacts/rootfs-linux-ready.tar \
    --verify-disk build/aegis-persist-4g.raw --timeout 600
```

Verification mounts ext4 `ro,noload` (no journal replay), requires a different
Linux boot ID, compares the owned sentinel and reconstructed payload byte for
byte, checks recorded hashes, and unmounts. Require `disk-persistence boot2 PASS`
and exit zero, retaining both UART logs. Same-boot remounting is rejected as
persistence evidence. Cleanly unmount before any shutdown; this is a clean-boot
persistence test, not a power-loss recovery or fsck test.

This proves a persistent data filesystem. Main must additionally wire the
disk mount and checkpoint/build-tree placement (or a disk root transition) to
stop using permanent ramfs for bootstrap storage; this isolated recipe does
not claim that migration is already done.

## Verified execution

Source/archive hashes, configure option and Makefile target inspection, shell
syntax and source-manifest consistency were checked. Host unit tests also pass
for boot argument bounds, disk-option exclusivity, positive timeouts, exclusive
creation, metadata validation and preserving existing files/symlinks. Tests use
temporary sparse files only.

Native Linux/QEMU execution passed: GCC 9.5 built the six real static libraries,
the native generators and mke2fs 1.45.7. The guest checked the entire new blank
4 GiB device, formatted ext4, mounted, wrote and unmounted it. A separate QEMU
boot verified owner/payload SHA256 read-only without journal replay and with a
different boot ID. Both runs passed the usual Linux/C/C++ gates and powered off
cleanly. No host compiler or host mkfs was used.

The first shell additionally compiled /hello.c directly onto ext4 and retained
the formatter bundle/config/full logs under /disk-tools. On the second boot,
all four guest-build hashes matched and the persisted program and formatter
both executed successfully from the read-only mount. Evidence and exact hashes
are in ../README.md and artifacts/linux-disk-boot{1,2}-final.log. This is a
persistent data volume; Linux still starts from the saved initramfs.
