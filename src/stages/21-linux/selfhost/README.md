# Build the next Linux under Linux

Approved scope: native rebuild on the saved Linux 6.6.0 / GCC 9.5 system;
source and objects on ext4, then boot and test the resulting checkpoint.
The known-good rootfs-linux-ready.tar and linux-data-03.raw stay intact.
The cloned linux-selfhost.raw is the persistent build volume. Root remains
initramfs in this milestone; a persistent disk root follows separately.

prepare.py verifies the ready checkpoint and official Linux source SHA256,
clones the guest-formatted disk using APFS copy-on-write, and packs source,
recipes, resolved config and our previously guest-built SBI as opaque bytes.
No host compiler, host mkfs or host cpio generator is used. The input archive
is attached read-only as /dev/vdb; a fresh export raw device is /dev/vdc.
The data volume remains /dev/vda. Only one runner can use the shared UART and
boot paths at a time.

## First build

From the repository root, run prepare once:

```sh
python3 src/stages/21-linux/selfhost/prepare.py
python3 -u src/scripts/linux.py --shell \
  --disk src/stages/21-linux/artifacts/linux-selfhost.raw \
  --input-tar src/stages/21-linux/artifacts/linux-selfhost-input.tar \
  --export-disk src/stages/21-linux/artifacts/linux-selfhost-export.raw
```

Inside Linux:

```sh
set -e
mkdir -p /mnt/data
mount -t ext4 /dev/vda /mnt/data
test "$(cat /mnt/data/.aegis-disk-owner)" = "aegis-disk-owner-v1 $(cat /sys/class/block/vda/serial)"
(cd /mnt/data; sha256sum -c guest-build.sha256)
mkdir -p /mnt/data/linux-selfhost/inputs /mnt/data/linux-selfhost/logs
tar xf /dev/vdb -C /mnt/data/linux-selfhost/inputs
if /usr/bin/bash /mnt/data/linux-selfhost/inputs/build.sh; then
  echo 'selfhost build PASS'
else
  rc=$?
  umount /mnt/data
  exit "$rc"
fi
umount /mnt/data
exit
```

build.sh verifies the full official gzip source hash inside the guest,
extracts the complete source with the native paxx helper and normalizes source
mtimes after verification (minimal Linux starts its clock in 1970). It uses
unmodified upstream source and the resolved config, with native GCC 9.5 and
static Kbuild HOSTCC/HOSTCXX. All upstream generators execute under Linux.
The Tilck-specific source patches are unnecessary here. Separate source,
object and log directories persist under /linux-selfhost on ext4.

LOCALVERSION=-aegis-selfhost and build version 2 distinguish the resulting
kernel at boot. The guest builds init and the kernel's real gen_init_cpio,
reuses our BusyBox/SBI and generates initramfs with the saved toolchain.
The boot smoke repeats C/C++, Bash associative arrays and actual pthread
create/mutex/barrier/join/shared-memory tests. An attached existing owned
disk can also use the normal read-only --verify-disk gate.

The guest packs /usr into checkpoint.tar using its native USTAR generator,
prints its byte count and SHA256,
then copies those exact bytes to the new raw export device and flushes it.
After clean shutdown, recover.py matches the UART hash and every boot artifact
against the guest manifest before truncating transport padding and promoting
the checkpoint. It never repacks or rewrites its archive contents on the host.

```sh
python3 src/stages/21-linux/selfhost/recover.py \
  src/stages/21-linux/artifacts/linux-selfhost-export.raw \
  src/stages/21-linux/artifacts/linux-selfhost-build.log \
  src/stages/21-linux/artifacts/rootfs-linux-selfhost.tar
python3 -u src/scripts/linux.py --test \
  --rootfs src/stages/21-linux/artifacts/rootfs-linux-selfhost.tar \
  --verify-disk src/stages/21-linux/artifacts/linux-selfhost.raw
```

## Resume

Keep the same ext4 clone and input archive. prepare.py intentionally refuses
to overwrite its input tar, and --export-disk refuses an existing output.
Choose a new export filename after a failed/interrupted attempt. Mount the
disk, retain inputs/source/objects, and invoke the same guest build.sh again.
Make reuses completed objects; no bootstrap or full source restart is required.
Inspect saved logs first. A successful source extraction has its own marker.

The selfhost initramfs contains the existing-disk verification script and
runtime tests. Use the original ready checkpoint for --new-disk formatting;
the native checkpoint keeps its boot payload focused on rebuild verification.

## Execution status

Completed 2026-11-20: full upstream Kbuild passed under Linux/GCC 9.5, and
6.6.0-aegis-selfhost then passed a separate QEMU boot, C/C++, Bash, actual
pthread synchronization and persisted ext4 verification; normal poweroff.

Checkpoint rootfs-linux-selfhost.tar: 568181248 bytes, guest/host SHA256
2ce05a5775e2b29588ca9bf60a124a0794ece36fc24692ba7095877f89566df9.
Image: 2288408 bytes, SHA256
f2a030429a0999f85f8779a300bdde5ff103ec87e81e844bcf7b2310be4df4f2.

The complete compile log is artifacts/linux-selfhost-build.log (Kbuild success,
followed by an initial export error: no sync executable). A compiled flush.c
now calls real fsync/sync. The first native boot also exposed GNU LongLink
records in the toolchain archive, unsupported by minimal BusyBox tar.
repack.sh verified the cached native kernel hashes and rebuilt only initramfs
and the archive using the already proven mkustar USTAR prefix implementation.
The kernel was not rebuilt. Clean packaging/export: linux-selfhost-repack.log.
Final successful boot: linux-selfhost-boot.log and corresponding -uart.log.

The runtime wrapper needs PATH inherited from PID 1 after its child restores
/usr. The runner now supplies PATH=/usr/bin:/usr/sbin:/bin:/sbin in bootargs;
smoke.sh also exports it for future rebuilt initramfs payloads. The saved
checkpoint boots through the current runner with all checks passing.

The final recover command used linux-selfhost-export-ustar.raw and
linux-selfhost-repack.log; recovery renamed the verified raw export to the
checkpoint without repacking bytes on the host. Failed diagnostics have their
own logs. The redundant failed raw export and before-USTAR full checkpoint
were removed after success, preserving the original build recipe separately
as linux-selfhost-first-build.sh.

The input transport was refreshed after completion with corrected recipes:
linux-selfhost-input.tar SHA256
527724c97e9e3c481df3a109a8abfaac4c7f0bd02e6e1381032d9bf674f94f92.
Original transport linux-selfhost-input-before-fixes.tar is retained as
provenance (SHA256 11e21fb72920334fc24e554e320b018f4b9f123d72d57966e6d2df773ad67729).
Both carry the same pinned upstream source. Sources, objects, native build
logs and the current guest checkpoint also persist on linux-selfhost.raw.
