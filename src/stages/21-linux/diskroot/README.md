# Boot from the persistent ext4 root

From the repository root:

```sh
python3 -u src/scripts/linux.py --shell \
  --disk-root src/stages/21-linux/artifacts/linux-diskroot.raw
```

The runner loads the saved kernel and the small initramfs from
`artifacts/rootfs-linux-diskroot.tar`. The initramfs mounts the owned ext4
volume at `/dev/vda`, moves the runtime mounts, and hands PID 1 to the
disk's `/sbin/init`. The serial console opens Bash in `/root`.
`exit` syncs the disk, remounts the root read-only, and powers off QEMU.
Changes to `/usr/local`, `/etc` and `/root` persist. Kernel sources,
objects and build logs from the prior stage remain under
`/mnt/data/linux-selfhost` with their original absolute build paths.

The test mode checks the original persistence fixture, including the SHA256
of `/root/.bashrc`. Use the interactive command for ordinary work, where
those files are editable.

```sh
python3 -u src/scripts/linux.py --test \
  --disk-root src/stages/21-linux/artifacts/linux-diskroot.raw
```

## Create the disk-root checkpoint

With the saved Linux-native checkpoint and disk available, `prepare.py`
verifies the kernel source and checkpoint hashes, clones the host disk image
using APFS copy-on-write, and packages sources for the guest. Run it once
with fresh output paths. The ext4 filesystem is already present in the
guest-created base disk.

```sh
out=src/stages/21-linux/artifacts
python3 src/stages/21-linux/diskroot/prepare.py
python3 -u src/scripts/linux.py --shell \
  --rootfs "$out/rootfs-linux-selfhost.tar" \
  --disk "$out/linux-diskroot.raw" \
  --input-tar "$out/linux-diskroot-input.tar" \
  --export-disk "$out/linux-diskroot-export.raw" 2>&1 | tee "$out/linux-diskroot-install.log"
```

Wait for the guest shell prompt, then run these commands in QEMU:

```sh
set -e
mkdir -p /mnt/root /tmp/diskroot-input
mount -t ext4 /dev/vda /mnt/root
test "$(cat /sys/class/block/vdb/serial)" = aegis-sources
tar xf /dev/vdb -C /tmp/diskroot-input
if /usr/bin/bash /tmp/diskroot-input/install.sh; then
  umount /mnt/root
else
  rc=$?
  umount /mnt/root
  exit "$rc"
fi
exit
```

The guest builds the boot helpers and small initramfs with GCC 9.5. The
kernel and SBI come unchanged from the source-built native checkpoint.
The guest writes the boot archive to the export device and flushes it.
After shutdown, verify the exact bytes against the UART log:

```sh
python3 src/stages/21-linux/diskroot/recover.py \
  "$out/linux-diskroot-export.raw" "$out/linux-diskroot-install.log" \
  "$out/rootfs-linux-diskroot.tar"
python3 -u src/scripts/linux.py --test --disk-root "$out/linux-diskroot.raw" --root-stage first
python3 -u src/scripts/linux.py --test --disk-root "$out/linux-diskroot.raw" --root-stage verify
```

The first test installs a native program in `/usr/local/bin`, changes
`/etc/aegis-demo.conf` and writes a Bash setting into `/root/.bashrc`.
The second test requires a different Linux boot ID, compares all three
file hashes, and runs the installed program. Both tests also run C, C++,
Bash and pthread checks on the ext4 root.

## Verified result

The saved boot archive is 2,427,904 bytes with guest and host SHA256
`06175b086db0f28c279f7b584488e925d1c568a1d31913b2b677e98d7b9d7962`.
It reuses `6.6.0-aegis-selfhost` and contains a 39,424-byte initramfs that
mounts the disk and starts `/sbin/init`.
The root filesystem lives in the separate 4 GiB `linux-diskroot.raw` image.

Independent QEMU starts recorded boot IDs
`0c37bae8-13e6-46dd-92be-3d9704ffce13` and
`461a45bd-9d3d-47d0-abc2-c9290cc9c5a0`. Both verified `/` against the
block device and ext4 filesystem type, ran the compiler and pthread checks,
remounted root read-only, and powered off normally. The second run matched
the three stored file hashes and ran the installed program. A separate
interactive run verified Bash job control, a serial terminal, `/root` as
the working directory, and the saved kernel source, objects and log paths.

The stage artifact directory contains `linux-diskroot-install.log`,
`linux-diskroot-boot1.log`, `linux-diskroot-boot2.log` and
`linux-diskroot-console.log`.
