#!/bin/sh
# Linux guest only. First formatting is deliberately separate from verification.
set -eu
export PATH=/usr/bin:/usr/sbin:/bin:/sbin LC_ALL=C
die() { echo "disk-persistence: $*" >&2; exit 1; }
test "$#" = 2 || die 'usage: persistence.sh --first-run-blank|--verify aegis-<14 lowercase hex digits>'
mode=$1
token=$2
case $mode in --first-run-blank|--verify) ;; *) die 'unknown mode (no default formatting)';; esac
case $token in aegis-*) ;; *) die 'invalid ownership token';; esac
hex=${token#aegis-}
test "${#hex}" = 14 || die 'token must be exactly 20 characters including aegis-'
case $hex in *[!0-9a-f]*) die 'invalid token hex';; esac
test "$(uname -s)" = Linux || die 'format/mount test requires Linux'
test "$(uname -m)" = riscv64 || die 'requires riscv64 guest'
test -x /bin/busybox || die 'requires guest-built BusyBox for mount/umount/dd'
for cmd in sha256sum awk cmp mkdir rmdir rm cat; do
    command -v "$cmd" >/dev/null || die "missing guest program: $cmd"
done

DEV=/dev/vda
SYS=/sys/class/block/vda
MNT=/mnt/aegis-persist
TOOLS=${DISK_TOOLS:-/build/disk-tools}
test -b "$DEV" || die 'expected whole virtio block device /dev/vda'
for file in "$SYS/serial" "$SYS/size" "$SYS/ro" "$SYS/dev" \
    /proc/self/mountinfo /proc/sys/kernel/random/boot_id; do
    test -r "$file" || die "missing kernel interface: $file"
done
test "$(cat "$SYS/serial")" = "$token" || die 'virtio serial does not match ownership token'
test "$(cat "$SYS/size")" = 8388608 || die 'requires exactly 4 GiB (8388608 sectors)'
test "$(cat "$SYS/ro")" = 0 || die 'disk is read-only'
for child in "$SYS"/vda* "$SYS"/holders/*; do
    test ! -e "$child" || die "refusing partitioned/in-use disk: $child"
done
if awk -v dev="$(cat "$SYS/dev")" '$3 == dev { found=1 } END { exit !found }' \
    /proc/self/mountinfo; then
    die 'device already mounted (including mount aliases)'
fi
if test -e /proc/swaps; then
    test -r /proc/swaps || die 'cannot inspect active swap'
    if awk 'NR > 1 { found=1 } END { exit !found }' /proc/swaps; then
        die 'active swap present; use an isolated test guest without swap'
    fi
fi
boot_id=$(cat /proc/sys/kernel/random/boot_id)
test "${#boot_id}" = 36 || die 'missing Linux boot ID'
if test "$mode" = --first-run-blank; then
    test -x "$TOOLS/sbin/mke2fs" || die 'missing guest-built mke2fs'
    test -r "$TOOLS/etc/mke2fs.conf" || die 'missing pinned mke2fs.conf'
fi

# Exclusive directories: do not mount over somebody else's directory or files.
WORK=/tmp/aegis-persist.$$
mkdir "$WORK"
mounted=no
own_mountpoint=no
finish() {
    rc=$?
    trap - 0
    if test "$mounted" = yes; then
        /bin/busybox umount "$MNT" || rc=1
    fi
    if test "$own_mountpoint" = yes; then rmdir "$MNT" || rc=1; fi
    rm -f "$WORK/owner" "$WORK/payload"
    rmdir "$WORK" || rc=1
    exit "$rc"
}
trap finish 0
trap 'exit 1' 1 2 15
mkdir -p /mnt
mkdir "$MNT" || die 'mountpoint already exists; refusing to reuse it'
own_mountpoint=yes
printf 'aegis-disk-owner-v1 %s\n' "$token" > "$WORK/owner"
printf 'Aegis-V persistent ext4 payload v1\nowner=%s\n' "$token" > "$WORK/payload"
/bin/busybox dd if=/dev/zero bs=4096 count=256 >> "$WORK/payload"

if test "$mode" = --first-run-blank; then
    echo 'disk-persistence: reading all 4 GiB to verify an entirely zero device'
    # Bounded memory: checkpoint sha256sum buffers whole files, not suitable here.
    # cmp 2.7 streams but has no -n/--bytes. Both inputs must end after 4 GiB.
    # A short/failed dd cannot pass: cmp then sees early EOF against the device.
    /bin/busybox dd if=/dev/zero bs=1048576 count=4096 | cmp - "$DEV" || \
        die 'device is not entirely zero or could not be read; refusing mkfs'
    # No -F, no partitioner, no host mkfs; unexpected prompts fail on EOF.
    MKE2FS_CONFIG=$TOOLS/etc/mke2fs.conf
    export MKE2FS_CONFIG
    "$TOOLS/sbin/mke2fs" -t ext4 -b 4096 -I 256 -m 0 -L aegis-persist \
        -E nodiscard,lazy_itable_init=0,lazy_journal_init=0 "$DEV" < /dev/null
    /bin/busybox mount -t ext4 -o rw "$DEV" "$MNT"
    mounted=yes
    cat "$WORK/owner" > "$MNT/.aegis-disk-owner"
    cat "$WORK/payload" > "$MNT/payload"
    printf '%s\n' "$boot_id" > "$MNT/first-boot-id"
    (cd "$MNT"; sha256sum -o checksums.sha256 .aegis-disk-owner payload first-boot-id)
else
    # No journal replay writes on the second boot; boot one must unmount cleanly.
    /bin/busybox mount -t ext4 -o ro,noload "$DEV" "$MNT"
    mounted=yes
    test "$(cat "$MNT/first-boot-id")" != "$boot_id" || die 'same boot; reboot before verification'
fi
cmp "$WORK/owner" "$MNT/.aegis-disk-owner"
cmp "$WORK/payload" "$MNT/payload"
(cd "$MNT"; sha256sum -c checksums.sha256)
# Successful Linux umount flushes the filesystem; no missing BusyBox sync applet.
/bin/busybox umount "$MNT"
mounted=no
if test "$mode" = --first-run-blank; then
    echo "disk-persistence boot1 PASS owner=$token boot=$boot_id; shut down and start a new QEMU"
else
    echo "disk-persistence boot2 PASS owner=$token boot=$boot_id"
fi
