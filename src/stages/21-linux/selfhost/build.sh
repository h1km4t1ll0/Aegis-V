#!/usr/bin/bash
# Execute only in our native Linux guest, on the cloned ext4 volume.
set -eu
set -o pipefail
export PATH=/usr/bin:/usr/sbin:/bin:/sbin LC_ALL=C TZ=UTC0
export SHELL=/usr/bin/bash CONFIG_SHELL=/usr/bin/bash
unset CC CXX CFLAGS CXXFLAGS CPPFLAGS LDFLAGS LIBS MAKEFLAGS MFLAGS
test "$(uname -s)" = Linux
test "$(uname -m)" = riscv64
test "$(gcc -dumpfullversion)" = 9.5.0
WORK=/mnt/data/linux-selfhost
INPUT=$WORK/inputs
SRC=$WORK/source
OBJ=$WORK/objects
META=/usr/share/aegis/linux66
mkdir -p "$WORK/logs" "$SRC" "$OBJ"
cd "$WORK"
step() {
    local label=$1
    shift
    echo "selfhost start $label"
    "$@" 2>&1 | tee "$WORK/logs/$label.log"
    echo "selfhost $label ok"
}
kbuild() {
    make -C "$SRC" O="$OBJ" -j1 ARCH=riscv CC=gcc \
        HOSTCC='gcc -static' HOSTCXX='g++ -static' HOSTLDFLAGS=-static \
        SHELL=/usr/bin/bash CONFIG_SHELL=/usr/bin/bash \
        LOCALVERSION=-aegis-selfhost "$@"
}
uname -a
gcc -v
test "$(cat /sys/class/block/vdb/serial)" = aegis-sources
test "$(cat /sys/class/block/vdb/ro)" = 1
test "$(cat /sys/class/block/vdc/serial)" = aegis-export
test "$(cat /sys/class/block/vdc/size)" = 1572864
printf '%s  %s\n' 3c88ff4648122e81832b7067be6456ef28e119b64649a29e84f7f04691e489f9 \
    "$INPUT/linux-6.6.tar.gz" > "$WORK/source.sha256"
step source-hash sha256sum -c "$WORK/source.sha256"
if test ! -f "$WORK/source-extracted"; then
    gcc -static -O2 /paxx.c -o "$WORK/paxx"
    gzip -dc "$INPUT/linux-6.6.tar.gz" | "$WORK/paxx" -C "$SRC" -s 1
    find "$SRC" -type f -exec touch -t 197001010000.00 {} +
    touch "$WORK/source-extracted"
fi
export KBUILD_BUILD_USER=aegis KBUILD_BUILD_HOST=linux-guest
export KBUILD_BUILD_TIMESTAMP='Thu Jan  1 00:00:00 UTC 1970' KBUILD_BUILD_VERSION=2
if test ! -f "$OBJ/.config"; then cp "$INPUT/config" "$OBJ/.config"; fi
step config kbuild olddefconfig
for option in 64BIT MMU SOC_VIRT RISCV_SBI FPU BINFMT_ELF FUTEX BLK_DEV_INITRD \
    SERIAL_8250_CONSOLE DEVTMPFS PROC_FS SYSFS SYSCTL PROC_SYSCTL VIRTIO_MMIO VIRTIO_BLK EXT4_FS; do
    grep -q "^CONFIG_${option}=y$" "$OBJ/.config"
done
grep -q '^CONFIG_GCC_VERSION=90500$' "$OBJ/.config"
step kernel kbuild Image
test "$(cat "$OBJ/include/config/kernel.release")" = 6.6.0-aegis-selfhost
echo 'selfhost native kernel built'
# Reuse our own SBI/BusyBox, build init and cpio generator under Linux.
gcc -static -O2 "$SRC/usr/gen_init_cpio.c" -o "$WORK/gen_init_cpio"
gcc -static -O2 "$INPUT/original/init.c" -o "$WORK/init"
gcc -static -O2 -pthread "$INPUT/pthread.c" -o "$WORK/pthread-check"
step pthread "$WORK/pthread-check"
/usr/bin/bash -c 'declare -A a; a[answer]=42; test "${a[answer]}" = 42; echo "linux bash ok $BASH_VERSION"'
# Pack /usr before placing initramfs in it: no recursive archive.
rm -f "$META/Image" "$META/vmlinux" "$META/initramfs.cpio"
cp "$META/recipe/src/mkustar.c" "$WORK/mkustar.c"
(cd "$WORK"; patch -p1 -i "$META/recipe/src/mkustar-long.patch")
gcc -static -O2 "$WORK/mkustar.c" -o "$WORK/mkustar"
(cd /; filelist "$WORK/toolchain.list" usr; "$WORK/mkustar" "$WORK/toolchain.tar" . "$WORK/toolchain.list")
"$WORK/gen_init_cpio" -t 0 "$INPUT/init-list" > "$WORK/initramfs.cpio"
cp "$OBJ/arch/riscv/boot/Image" "$META/Image"
cp "$OBJ/vmlinux" "$META/vmlinux"
cp "$OBJ/.config" "$META/config"
cp "$INPUT/sbi.bin" "$META/sbi.bin"
cp "$WORK/initramfs.cpio" "$META/initramfs.cpio"
mkdir -p "$META/selfhost/logs" "$META/selfhost/recipe"
cp -R "$WORK/logs/." "$META/selfhost/logs/"
cp "$WORK/source.sha256" "$META/selfhost/"
for name in build.sh smoke.sh pthread.c flush.c init-list; do
    cp "$INPUT/$name" "$META/selfhost/recipe/$name"
done
(cd "$META"; sha256sum -o selfhost/artifacts.sha256 Image vmlinux config sbi.bin initramfs.cpio)
cat "$META/selfhost/artifacts.sha256"
readelf -h "$OBJ/vmlinux" | grep -q RISC-V
(cd /; filelist "$WORK/usr.list" usr)
step checkpoint "$WORK/mkustar" "$WORK/checkpoint.tar" / "$WORK/usr.list"
bytes=$(wc -c < "$WORK/checkpoint.tar")
test "$bytes" -lt 805306368
sha256sum -o "$WORK/checkpoint.sha256" "$WORK/checkpoint.tar"
cat "$WORK/checkpoint.sha256"
echo "selfhost checkpoint bytes $bytes"
# This raw export is a new owned transport device, not the ext4 data volume.
/bin/busybox dd if="$WORK/checkpoint.tar" of=/dev/vdc bs=1048576
gcc -static -O2 "$INPUT/flush.c" -o "$WORK/flush"
"$WORK/flush" /dev/vdc
echo 'selfhost export ok'
