#!/usr/bin/bash
# Rebuild packaging only; keep the successfully compiled native kernel.
set -eu
set -o pipefail
export PATH=/usr/bin:/usr/sbin:/bin:/sbin LC_ALL=C TZ=UTC0
WORK=/mnt/data/linux-selfhost
INPUT=$WORK/inputs
META=/usr/share/aegis/linux66
test "$(uname -s)" = Linux
test "$(uname -m)" = riscv64
test "$(cat "$WORK/objects/include/config/kernel.release")" = 6.6.0-aegis-selfhost
test "$(cat /sys/class/block/vdc/serial)" = aegis-export
test "$(cat /sys/class/block/vdc/size)" = 1572864
cd "$WORK"
grep 'Kernel: arch/riscv/boot/Image is ready' logs/kernel.log
tar -xOf checkpoint.tar usr/share/aegis/linux66/selfhost/artifacts.sha256 > cached-artifacts.sha256
for name in Image vmlinux config; do
    case "$name" in
        Image) file=$WORK/objects/arch/riscv/boot/Image ;;
        vmlinux) file=$WORK/objects/vmlinux ;;
        config) file=$WORK/objects/.config ;;
    esac
    wanted=$(awk -v name="$name" '$2 == name {print $1}' cached-artifacts.sha256)
    printf '%s  %s\n' "$wanted" "$file" > cached-one.sha256
    sha256sum -c cached-one.sha256
done
echo 'selfhost cached native kernel verified'
cp "$META/recipe/src/mkustar.c" "$WORK/mkustar.c"
patch -p1 -i "$META/recipe/src/mkustar-long.patch"
gcc -static -O2 "$WORK/mkustar.c" -o "$WORK/mkustar"
gcc -static -O2 "$WORK/source/usr/gen_init_cpio.c" -o "$WORK/gen_init_cpio"
gcc -static -O2 "$INPUT/original/init.c" -o "$WORK/init"
gcc -static -O2 "$INPUT/flush.c" -o "$WORK/flush"
./pthread-check
# USTAR prefix fields work with the minimal BusyBox tar; GNU LongLink does not.
cd /
filelist "$WORK/toolchain.list" usr
"$WORK/mkustar" "$WORK/toolchain.tar" . "$WORK/toolchain.list"
"$WORK/gen_init_cpio" -t 0 "$INPUT/init-list" > "$WORK/initramfs.cpio"
echo 'selfhost USTAR initramfs built'
cp "$WORK/objects/arch/riscv/boot/Image" "$META/Image"
cp "$WORK/objects/vmlinux" "$META/vmlinux"
cp "$WORK/objects/.config" "$META/config"
cp "$INPUT/sbi.bin" "$META/sbi.bin"
cp "$WORK/initramfs.cpio" "$META/initramfs.cpio"
mkdir -p "$META/selfhost/logs" "$META/selfhost/recipe"
cp -R "$WORK/logs/." "$META/selfhost/logs/"
cp "$WORK/source.sha256" "$META/selfhost/"
for name in build.sh smoke.sh pthread.c flush.c init-list repack.sh; do
    cp "$INPUT/$name" "$META/selfhost/recipe/$name"
done
(cd "$META"; sha256sum -o selfhost/artifacts.sha256 Image vmlinux config sbi.bin initramfs.cpio)
cat "$META/selfhost/artifacts.sha256"
filelist "$WORK/usr.list" usr
"$WORK/mkustar" "$WORK/checkpoint.tar" . "$WORK/usr.list"
bytes=$(wc -c < "$WORK/checkpoint.tar")
test "$bytes" -lt 805306368
sha256sum -o "$WORK/checkpoint.sha256" "$WORK/checkpoint.tar"
cat "$WORK/checkpoint.sha256"
echo "selfhost checkpoint bytes $bytes"
/bin/busybox dd if="$WORK/checkpoint.tar" of=/dev/vdc bs=1048576
"$WORK/flush" /dev/vdc
echo 'selfhost export ok'
