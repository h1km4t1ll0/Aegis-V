# Stage 21: Linux built under Linux on RV64 QEMU

The first Linux, BusyBox, init, initramfs and Aegis SBI were built from source
inside Tilck/QEMU with the bootstrapped GCC 9.5.0. That Linux has now rebuilt
the full unmodified Linux 6.6 kernel itself, with sources and objects on ext4.
The result booted as 6.6.0-aegis-selfhost in a separate RV64 QEMU. PID 1,
fork/pipe/wait, file IO, BusyBox, C/C++ compilation/execution, Bash, actual
pthread synchronization and persisted ext4 data passed; clean poweroff.

The host places pinned source/checkpoint bytes and recovers guest outputs.
No host compiler, downloaded kernel, default QEMU OpenSBI or prebuilt OS
binary enters this stage. Earlier ladder checkpoints are reused.

## Run the Linux-native result

The latest checkpoint boots with `/` on ext4 and opens Bash in `/root`.

```sh
python3 -u src/scripts/linux.py --shell \
  --disk-root src/stages/21-linux/artifacts/linux-diskroot.raw
```

The runner loads `artifacts/rootfs-linux-diskroot.tar` automatically.
Installed programs and changes to `/etc` and `/root` survive poweroff.
See [disk-root setup and verification](diskroot/README.md).

## Run the Linux-native initramfs checkpoint

```sh
python3 -u src/scripts/linux.py --test \
  --rootfs src/stages/21-linux/artifacts/rootfs-linux-selfhost.tar \
  --verify-disk src/stages/21-linux/artifacts/linux-selfhost.raw
python3 -u src/scripts/linux.py --shell \
  --rootfs src/stages/21-linux/artifacts/rootfs-linux-selfhost.tar \
  --disk src/stages/21-linux/artifacts/linux-selfhost.raw
```

The kernel release is 6.6.0-aegis-selfhost, build #2, GCC 9.5.0. The native
build source and objects remain on the cloned 4 GiB ext4 volume, under
/linux-selfhost/source and /linux-selfhost/objects (mount /dev/vda at /mnt/data).
Root still starts from initramfs. The original ready checkpoint/disk remain
available. Native build/recovery details are in selfhost/README.md.

## Run the earlier Tilck-built Linux

From the repository root, with qemu-system-riscv64 installed:

```sh
python3 -u src/scripts/linux.py --test
python3 -u src/scripts/linux.py --shell
```

Both prefer artifacts/rootfs-linux-ready.tar, falling back to the original
rootfs-linux-build.tar when the refresh is absent. The shell starts after the same
runtime/compiler checks; exit shuts down through PID 1. Root is initramfs,
with the guest-built /usr checkpoint restored at boot. No network or modules;
one RV64 hart, 2 GiB RAM, serial console. The runner uses rv64,sstc=false:
our SBI implements TIME but does not enable menvcfg.STCE for direct SSTC.

## Verified checkpoints

| Checkpoint | Bytes | SHA256 |
| --- | ---: | --- |
| GCC 9.5 input, stage 20 | 265129984 | 76981c616a3987b348ebdd03a70e311f0a81d80a17c727851b854cb62b7e9a46 |
| Bash 4.4 | 269128192 | b28e28fb90a7e4cfce93ad6c9fc74166e883d434939db5959f56802d29690a83 |
| Linux build tools | 272677376 | 22fd69cd2110b07f857bbedb056674dbf0ec0b12451189e18a23b188ecbc2fa6 |
| UAPI + BusyBox + mktemp | 281352192 | 41b7a00689278943ca80a28df8026b3c45fe3ecb0a4b17ae687eecabdde13550 |
| Complete Linux build | 574349824 | 1ad9f1351d3dbaba993f5ce518b2512ca66335f5d462a5df2723c8cee4c73db2 |
| Userspace/disk refresh | 570912768 | 7b69cb22aef5d990204b96d099623760f19981b0fb1af54a8f8201efa9ea781d |
| Linux-native rebuild | 568181248 | 2ce05a5775e2b29588ca9bf60a124a0794ece36fc24692ba7095877f89566df9 |
| Persistent ext4 root boot archive | 2427904 | 06175b086db0f28c279f7b584488e925d1c568a1d31913b2b677e98d7b9d7962 |

Each successful gate matches the guest UART SHA256 against the recovered
outbox or raw export bytes. Failed diagnostic exports are never promoted
without matching the guest hash and completing runtime verification.
Superseded diagnostic snapshots were removed during the coordinated cleanup;
canonical successful checkpoints remain available.

Boot files recovered unchanged from the original complete-build archive:

| File | Bytes | SHA256 |
| --- | ---: | --- |
| Image | 2288408 | 7fd22c55229714be2d8edcd8825deb9074a80fcfa1370081e044055b0ea7f0b5 |
| initramfs.cpio | 287315456 | cbe4387584df3e086afdac9f0b034b42caac3732d8ef2ba84bc77816b9632e3b |
| sbi.bin | 16160 | f70684893d7ec1980b22330956f1d701d607e6a411da8ca45df015da28899170 |

The linux-ready refresh retains exactly that Image/SBI and creates a new
287187968-byte initramfs (SHA256
8fcaf2542998abb2f11fc3b10c2798753ebb6951cf9a1a0b71be4e4444028ffe).
It adds mount/umount/mktemp applet links, the disk-build timestamp fix and
a source-rebuilt gawk with Linux fcntl.h. Both awk/gawk copies are replaced;
create/append/truncate tests pass. The early -I vms recipe had selected VMS
open flags, which cannot create new files on Linux. The updated toolchain
is packed in the guest before generating initramfs, without recursion.
The refresh took 637 host seconds, including the guest archive hash; see
artifacts/linux-ready-gate.log and linux-ready-uart.log.

The Image header reports 2513432 bytes of kernel memory, text offset 2 MiB.
Firmware loads at 0x80000000, kernel at 0x80200000, external initramfs at
0x90000000. It passes hart ID and DTB in S mode with satp=0, and reserves
its own first 2 MiB. Linux boot logs are artifacts/linux-boot-test.log and
artifacts/linux-boot-uart.log; all required markers and clean poweroff passed.

The complete final build took 5528 host seconds. Its UART/gate are
artifacts/linux-build-uart.log and linux-build-gate-sorttable-io.log.
Provenance, config, native Kconfig frontend and build logs are also inside
/usr/share/aegis/linux66 in the checkpoint. GCC 9.5 had already bootstrapped
itself at stage 20; this stage additionally executes C and C++ under Linux.
The subsequent full kernel rebuild under Linux passed; see the native
checkpoint and runtime evidence below.

## Rebuild the new stages

```sh
python3 -u src/scripts/tilck.py --rung linux-bash
python3 -u src/scripts/tilck.py --rung linux-tools
python3 -u src/scripts/tilck.py --rung linux-base
python3 -u src/scripts/tilck.py --rung linux-build
# Refresh initramfs only after a userspace recipe change:
python3 -u src/scripts/tilck.py --rung linux-ready
```

Inputs form a checkpoint chain: stage 20 GCC9 -> Bash -> tools -> UAPI and
BusyBox -> Linux. Only run the needed rung. Tools are Bash 4.4, sed 4.8,
Bison 3.0.4, bc 1.07.1, with existing make 3.82/flex 2.6.4/binutils 2.30.
Sources and pins are tools/sources, kernel/sources and disk/sources. Linux's
official gzip SHA256 matches kernel.org's published SHA256 list; BusyBox's
SHA256 matches its published checksum. Independent GNU hash provenance is
recorded in tools/provenance. Guest SHA256 checks repeat the host checks.

All compilers/generators execute in the guest. Kbuild HOSTCC/HOSTCXX are
static command-line assignments. The kernel's own usr/gen_init_cpio.c is
compiled natively and creates the external cpio; no host cpio generator is
used. BusyBox source contains GNU LongLink records, so the guest GNU tar
extracts it; Linux and e2fsprogs pax archives use the guest paxx helper.

Only one Tilck rung may use its shared UART/QMP/outbox paths at once.
The Linux runner has separate artifacts. For an interrupted/failed rung,
first preserve its UART and guest outbox, match their SHA256, then use:

```sh
python3 src/stages/21-linux/tools/resume.py CHECKPOINT GUEST_SHA256 --rung linux-base
```

The same runner supports linux-tools, linux-build and linux-ready. It overrides only
that invocation's input, retains the normal final gate, and never names a
failed diagnostic export as a completed rung.

## Build-host compatibility

These changes preserve the real generators and their output semantics:

- Bash uses musl malloc; sed uses GNU's real replacement sleep implementation
  selected by negative large-sleep probes, because Tilck setitimer is ENOSYS.
- Bison builds its actual core/yacc/liby/skeletons without Perl-dependent
  docs/examples. Its generated parser is compiled and executed in the guest.
- bc regenerates its grammar/scanner with real Bison/flex; sed formats the
  actual fbc-generated math library where its release recipe needed ed.
- BusyBox config merges explicit settings after allnoconfig, then uses real
  oldconfig. fixdep reads immutable files via supported read-only shared
  mappings; split-include receives the real GNU find list from its make rule.
- Kconfig shell probes use pipe/fork/exec/wait on Tilck; true/false/multiline
  results and actual GCC identification (90500) are checked. Immutable file
  comparisons use supported shared mappings.
- modpost reads an actual private writable ELF buffer. A native ELF, its
  unchanged input hash and missing-file rejection are checked before Image.
- sorttable reads/sorts/writes the actual ELF. A native ET_EXEC fixture with
  a large section gap checks instruction/fixup/type ordering, the sort-needed
  flag and idempotence. The linked ELF is preserved before postprocessing if
  a future failure needs a retry; it is not promoted without sorting.

The initial run also exposed Tilck rename-same-inode and rmdir("dir/./")
bugs, repaired and tested with guest-built kernels; stage 12 records evidence.
No downloaded executable or fake success utility is used for these ports.

## Persistent disk

The initial working system boots from initramfs. disk/ contains pinned
source-only e2fsprogs 1.45.7 build and a guest-only ext4 persistence test.
Both distinct QEMU boots passed on linux-data-03.raw (4 GiB sparse raw,
133 MiB allocated after the test). GCC 9.5 built e2fsprogs and its native
code generators inside Linux. The guest read the full blank disk, formatted
ext4, mounted it, wrote owner/payload/checksums, unmounted and powered off.
A fresh QEMU verified the same data read-only with a different Linux boot ID.

A C program was also compiled directly onto ext4 during the first boot.
The second boot checked its SHA256 and ran it successfully. The native static
mke2fs bundle, full build/config logs, recipe, provenance and output hashes
are saved on the disk under /disk-tools. The root filesystem remains
initramfs; /dev/vda is a persistent data/build volume, not a disk-root system.

Use the saved disk without formatting:

```sh
python3 -u src/scripts/linux.py --shell --disk src/stages/21-linux/artifacts/linux-data-03.raw
# Inside Linux:
mkdir -p /mnt/data
mount -t ext4 /dev/vda /mnt/data
/mnt/data/hello-linux
# Exit after flushing the mounted filesystem:
umount /mnt/data
exit
```

To repeat the read-only persistence gate on that saved disk:

```sh
python3 -u src/scripts/linux.py --test --verify-disk src/stages/21-linux/artifacts/linux-data-03.raw
```

To create a separate fresh disk (native formatter build runs again):

```sh
python3 src/scripts/linux.py --test --new-disk src/stages/21-linux/artifacts/linux-data-fresh.raw --timeout 3600
python3 src/scripts/linux.py --test --verify-disk src/stages/21-linux/artifacts/linux-data-fresh.raw
```

--new-disk exclusively creates a new blank 4 GiB file and ownership metadata.
Inside Linux, GCC builds mke2fs from source, which formats that new disk as
ext4. The second boot verifies owner/payload hashes and a changed boot ID;
it never formats. --disk attaches an existing raw disk without formatting.
Full interface and source/build details are in disk/README.md.

Verified run logs: artifacts/linux-disk-boot1-final.log and
linux-disk-boot2-final.log (both linux boot ok, exit 0), with corresponding
-final-uart.log files. Owner: aegis-c334f4e3ef2dbc; filesystem UUID:
ce4e45e3-862d-411c-a1aa-2a3eb252659f. Boot IDs changed from
43f523dc-c320-4232-9ffa-62f06fe0d0c8 to 9db69218-55be-458d-8e57-f3bbbb0b263d.

Persisted SHA256:

| File on ext4 | SHA256 |
| --- | --- |
| hello-linux | 9f544a0873cd0246b6ee75f56b7b4f71bf223e197e59cc72bfc324e208ff321c |
| disk-tools/sbin/mke2fs | 085acdcb7869c89cddf7ee5fc1cfb85b7fcd7bd7776aaff5a7168e1f475e8caf |
| disk-tools/etc/mke2fs.conf | b155410ad87acfcfeceab9db3e6e0ac926e4a37b6a3323963cea884fa4e95d9f |

The blank failed disk attempts linux-data-01/02 and superseded diagnostic
rootfs snapshots were removed during the user-authorized 2026-11-20 cleanup.
Their logs and all canonical successful checkpoints were retained. The runner
still defaults to the earlier linux-ready profile; select --rootfs explicitly
for the Linux-native result. No bootstrap restart is required.


## Linux-native verification (2026-11-20)

The native kernel source is the full pinned Linux 6.6 gzip release, verified
inside Linux and extracted without exclusions. No Tilck-specific host-tool
patches were applied. All Kbuild generators and GCC execute in the guest.
Our previously built BusyBox/SBI are reused. Init and gen_init_cpio are rebuilt
under Linux. USTAR prefix fields avoid GNU LongLink records unsupported by
the minimal BusyBox tar. The export helper performs actual fsync/sync syscalls;
the host only copies/verifies guest output bytes.

The runner supplies the normal PATH to PID 1 through kernel command-line
environment assignments. This also lets bare gcc find its installed cc1 when
runtime checks return from a child shell; the current smoke recipe exports
PATH itself too. No kernel recompile was required for either packaging fix.

| Native boot artifact | Bytes | SHA256 |
| --- | ---: | --- |
| Image | 2288408 | f2a030429a0999f85f8779a300bdde5ff103ec87e81e844bcf7b2310be4df4f2 |
| vmlinux | 2969152 | 7e13505fb674f1ec8e8af25a7f03ec89a7ce67c1794385dafc62bd6a1286d9a9 |
| config | 34747 | 30ee2945bc7c35877e1524b3024b980129d12b851ec074cf767bf5c03b532fda |
| initramfs.cpio | 281583616 | c58e80911f05a00784d3efc2af735042388d7adb6f517cbd2bbd2577046fed10 |
| sbi.bin | 16160 | f70684893d7ec1980b22330956f1d701d607e6a411da8ca45df015da28899170 |

Evidence: artifacts/linux-selfhost-build.log records the full successful
Kbuild and the initial missing-sync export failure. Packaging-only fixes and
clean export are in linux-selfhost-repack.log (exit 0). Native boot is
linux-selfhost-boot.log / linux-selfhost-boot-uart.log (exit 0), ending with:

```text
linux uname Linux 6.6.0-aegis-selfhost riscv64
hello from gcc on Linux
hello from g++ on Linux
disk-persistence boot2 PASS owner=aegis-c334f4e3ef2dbc boot=51435e5b-0799-455c-9884-d4af0b8da1dd
linux bash ok 4.4.0(1)-release
linux pthread ok pid=73 tids=74,75 counter=20000
linux init ok
reboot: Power down
linux boot ok
```
