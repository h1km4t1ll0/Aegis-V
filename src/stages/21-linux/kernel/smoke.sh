#!/bin/sh
set -e
export PATH=/usr/bin:/usr/sbin:/bin:/sbin
/bin/busybox mount -t devtmpfs devtmpfs /dev
/bin/busybox mount -t proc proc /proc
/bin/busybox mount -t sysfs sysfs /sys
/bin/busybox uname -a
/bin/busybox ls /proc/self
/bin/busybox sh -c 'test "$((6 * 7))" = 42'
echo 'linux busybox ok'
# Restore the guest-built toolchain checkpoint into writable initramfs.
/bin/busybox tar -xf /rootfs.tar -C /
/bin/busybox rm /rootfs.tar
/usr/bin/gcc -static -O2 /hello.c -o /tmp/hello
/tmp/hello
/usr/bin/g++ -static -std=c++14 -O2 /hello.cpp -o /tmp/hello-cpp
/tmp/hello-cpp
echo 'linux toolchain ok'
disk_mode=
disk_token=
for arg in $(cat /proc/cmdline); do
    case "$arg" in
        aegis.disk-mode=*) disk_mode=${arg#aegis.disk-mode=} ;;
        aegis.disk-token=*) disk_token=${arg#aegis.disk-token=} ;;
    esac
done
case "$disk_mode" in
    first)
        mkdir -p /build/e2fs-src
        sha256sum -c /disk/archive.sha256
        gcc -static -O2 /paxx.c -o /tmp/paxx
        unxz --file /e2fsprogs-1.45.7.tar.xz --output /tmp/e2fs.tar
        /tmp/paxx -C /build/e2fs-src -s 1 < /tmp/e2fs.tar
        rm /tmp/e2fs.tar
        DISTFILES=/ /bin/sh /disk/build.sh
        /bin/sh /disk/persistence.sh --first-run-blank "$disk_token"
        ;;
    verify)
        /bin/sh /disk/persistence.sh --verify "$disk_token"
        ;;
    '') ;;
    *) echo 'linux disk mode invalid'; exit 1 ;;
esac
if /bin/busybox grep -q 'aegis.shell=1' /proc/cmdline; then
    echo 'Linux shell ready; exit returns to PID 1 for shutdown.'
    /bin/busybox sh -i
fi
