#!/bin/bash
set -euo pipefail
export PATH=/usr/bin:/usr/sbin:/bin:/sbin LC_ALL=C
test "$(uname -s)" = Linux
test "$(uname -m)" = riscv64
test "$(gcc -dumpfullversion)" = 9.5.0
INPUT=/tmp/diskroot-input
WORK=/tmp/diskroot-work
TARGET=/mnt/root
META=/usr/share/aegis/linux66
test "$(cat /sys/class/block/vdb/serial)" = aegis-sources
test "$(cat /sys/class/block/vdb/ro)" = 1
test "$(cat /sys/class/block/vdc/serial)" = aegis-export
test "$(cat /sys/class/block/vdc/size)" = 1572864
test "$(cat "$TARGET/.aegis-disk-owner")" = "aegis-disk-owner-v1 $(cat /sys/class/block/vda/serial)"
test ! -e "$TARGET/usr"
test ! -e "$TARGET/.aegis-diskroot"
mkdir "$WORK"
cd "$INPUT"
sha256sum -c SHA256SUMS
for name in boot init root-proof console flush space; do
    gcc -static -Os -s "$INPUT/$name.c" -o "$WORK/$name"
    readelf -h "$WORK/$name" | grep -q RISC-V
    if readelf -l "$WORK/$name" | grep -q INTERP; then exit 1; fi
done
gcc -static -O2 "$INPUT/gen_init_cpio.c" -o "$WORK/gen_init_cpio"
cp "$META/recipe/src/mkustar.c" "$WORK/mkustar.c"
(cd "$WORK"; patch -p1 -i "$META/recipe/src/mkustar-long.patch")
gcc -static -O2 "$WORK/mkustar.c" -o "$WORK/mkustar"
echo 'diskroot native programs built'
# Only discard reproducible packaging caches in the new clone. The original
# disk, canonical host checkpoints, kernel source, objects and logs stay intact.
rm -f "$TARGET/linux-selfhost/checkpoint.tar" "$TARGET/linux-selfhost/toolchain.tar" \
    "$TARGET/linux-selfhost/initramfs.cpio"
"$WORK/space" "$TARGET"
cp -R /usr "$TARGET/usr"
mkdir -p "$TARGET"/{bin,sbin,etc,root,tmp,var/log,dev,proc,sys,mnt/data,usr/local/bin,usr/local/src/diskroot}
chmod 1777 "$TARGET/tmp"
cp /bin/busybox "$TARGET/bin/busybox"
ln -s /usr/bin/bash "$TARGET/bin/bash"
ln -s bash "$TARGET/bin/sh"
for name in mount umount uname ls; do ln -s busybox "$TARGET/bin/$name"; done
for name in init root-proof console; do cp "$WORK/$name" "$TARGET/sbin/$name"; done
cp "$INPUT/smoke.sh" "$TARGET/smoke.sh"
cp "$INPUT/"*.c "$TARGET/usr/local/src/diskroot/"
cp "$INPUT/hello.cpp" "$TARGET/usr/local/src/diskroot/"
cp "$INPUT/"*.sh "$TARGET/usr/local/src/diskroot/"
chmod 755 "$TARGET/smoke.sh"
printf 'root:x:0:0:root:/root:/bin/bash\n' > "$TARGET/etc/passwd"
printf 'root:x:0:\n' > "$TARGET/etc/group"
printf 'root:!:0:0:99999:7:::\n' > "$TARGET/etc/shadow"
chmod 600 "$TARGET/etc/shadow"
printf '/bin/bash\n/bin/sh\n' > "$TARGET/etc/shells"
printf 'aegis\n' > "$TARGET/etc/hostname"
printf 'answer=0\n' > "$TARGET/etc/aegis-demo.conf"
cat > "$TARGET/etc/profile" <<'EOF'
export PATH=/usr/local/bin:/usr/bin:/usr/sbin:/bin:/sbin
export HOME=/root USER=root LOGNAME=root SHELL=/bin/bash TERM=linux
umask 022
if [ -f "$HOME/.bashrc" ]; then . "$HOME/.bashrc"; fi
PS1='aegis:\w# '
export PS1
EOF
printf 'aegis disk root v1\n' > "$TARGET/.aegis-diskroot"
# Keep the absolute paths recorded by the completed native kernel build.
mv "$TARGET/linux-selfhost" "$TARGET/mnt/data/linux-selfhost"
ln -s /mnt/data/linux-selfhost "$TARGET/linux-selfhost"
echo 'diskroot filesystem installed; source/object paths preserved'
cat > "$WORK/init-list" <<EOF
dir /dev 0755 0 0
nod /dev/console 0600 0 0 c 5 1
dir /proc 0555 0 0
dir /sys 0555 0 0
dir /newroot 0755 0 0
file /init $WORK/boot 0755 0 0
EOF
"$WORK/gen_init_cpio" -t 0 "$WORK/init-list" > "$WORK/initramfs.cpio"
mkdir -p "$WORK/package/usr/share/aegis/linux66/diskroot/recipe"
DEST=$WORK/package/usr/share/aegis/linux66
for name in Image sbi.bin config; do cp "$INPUT/$name" "$DEST/$name"; done
cp "$WORK/initramfs.cpio" "$DEST/initramfs.cpio"
cp "$INPUT/"*.c "$INPUT/"*.cpp "$INPUT/"*.sh "$INPUT/SHA256SUMS" "$DEST/diskroot/recipe/"
(cd "$DEST"; sha256sum -o diskroot/artifacts.sha256 Image sbi.bin config initramfs.cpio)
cat "$DEST/diskroot/artifacts.sha256"
cd "$WORK/package"
filelist "$WORK/files.list" usr
"$WORK/mkustar" "$WORK/checkpoint.tar" . "$WORK/files.list"
sha256sum -o "$WORK/checkpoint.sha256" "$WORK/checkpoint.tar"
cat "$WORK/checkpoint.sha256"
echo "diskroot checkpoint bytes $(wc -c < "$WORK/checkpoint.tar")"
/bin/busybox dd if="$WORK/checkpoint.tar" of=/dev/vdc bs=1048576
"$WORK/flush" /dev/vdc
echo 'diskroot install PASS'
