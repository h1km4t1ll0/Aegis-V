# Refresh only userspace/initramfs; reuse our previous guest-built Image/SBI.
set -eu
set -o pipefail
mkdir -p /steps /build /usr/share/aegis/linux66/ready
cp -R /initrd/steps/. /steps/
cd /steps
checksum-transcriber sources
sha256sum -c sources.SHA256SUM
sha256sum -c gen-cpio.sha256
sha256sum -c previous-kernel.sha256
echo 'linux ready inputs ok'
gzip -dc /initrd/distfiles/gawk-3.0.4.tar.gz | tar xf - -C /build
cd /build/gawk-3.0.4
patch -p1 -i /steps/alloca-size_t.patch
# The early recipe's -I vms selected VMS fcntl.h (O_CREAT=0x200).
# Linux/musl requires its own fcntl.h (O_CREAT=0x40).
sed 's/-I vms //' /steps/gawk.mk > Makefile
find . -type f -exec touch -t 197001010000.00 {} +
if ! make -j1 CC=gcc PREFIX=/usr > /build/gawk-build.log 2>&1; then
    tail -60 /build/gawk-build.log
    exit 1
fi
cp gawk /usr/bin/gawk
# Checkpoint mkustar stores symlink targets as regular files, including awk.
cp gawk /usr/bin/awk
test ! -e /build/awk-created
awk 'BEGIN {print "created" > "/build/awk-created"; close("/build/awk-created"); print "appended" >> "/build/awk-created"}'
printf 'created\nappended\n' > /build/awk-expected
cmp /build/awk-created /build/awk-expected
awk 'BEGIN {print "truncated" > "/build/awk-created"}'
printf 'truncated\n' > /build/awk-expected
cmp /build/awk-created /build/awk-expected
readelf -h /usr/bin/gawk | grep -q RISC-V
if readelf -l /usr/bin/gawk | grep -q INTERP; then exit 1; fi
echo 'linux ready awk ok'
cp /steps/src/mkustar.c /build/mkustar.c
cd /build
patch -p1 -i /steps/src/mkustar-long.patch
gcc -static -O2 /build/mkustar.c -o /build/mkustar
# Embed the updated toolchain without recursively including the new initramfs.
cd /
filelist /build/toolchain.list usr
/build/mkustar /build/toolchain.tar . /build/toolchain.list
gcc -static -O2 /steps/src/gen_init_cpio.c -o /build/gen_init_cpio
gcc -static -O2 /steps/init.c -o /build/init
for binary in /build/gen_init_cpio /build/init; do
    readelf -h "$binary" | grep -q RISC-V
    if readelf -l "$binary" | grep -q INTERP; then exit 1; fi
done
echo 'linux ready generators built'
for name in Image sbi.bin config; do
    cp /initrd/previous/$name /usr/share/aegis/linux66/$name
done
sed 's@/initrd/rootfs.tar@/build/toolchain.tar@' /steps/init-list > /build/init-list
/build/gen_init_cpio -t 0 /build/init-list > /usr/share/aegis/linux66/initramfs.cpio
echo 'linux ready initramfs built'
cp -R /steps/. /usr/share/aegis/linux66/ready/
cp /build/gawk-build.log /usr/share/aegis/linux66/ready/
gcc -static -O2 /steps/src/paxx.c -o /build/paxx
cd /
filelist /build/usr.list usr
/build/mkustar /build/rootfs.tar . /build/usr.list
catm /dev/outbox /build/rootfs.tar
sha256sum -o /build/rootfs.sum /build/rootfs.tar
read sum rest < /build/rootfs.sum
echo "rootfs sha256 ${sum}"
echo 'rung done linux-ready'
