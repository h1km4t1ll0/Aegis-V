# Linux 6.6 build, entirely in Tilck using stage 20 GCC 9.5.
set -eu
set -o pipefail
export SHELL=/bin/sh CONFIG_SHELL=/bin/sh
unset CC CXX CFLAGS CXXFLAGS CPPFLAGS LDFLAGS LIBS
mkdir -p /steps /build/logs /usr/share/aegis/linux66
cp -R /initrd/steps/. /steps/
cd /steps
checksum-transcriber sources
sha256sum -c sources.SHA256SUM
step() {
    label=$1
    shift
    echo "linux start ${label}"
    if "$@" 2>&1 | tee /build/logs/${label}.log; then
        tail -4 /build/logs/${label}.log
        echo "linux ${label} ok"
    else
        tail -100 /build/logs/${label}.log
        echo "linux build FAIL ${label}"
        exit 1
    fi
}
pack_rootfs() {
    mkdir -p /usr/share/aegis/linux66/logs /usr/share/aegis/linux66/recipe
    cp -R /build/logs/. /usr/share/aegis/linux66/logs/
    cp -R /steps/. /usr/share/aegis/linux66/recipe/
    gcc -v > /usr/share/aegis/linux66/logs/gcc-version.log 2>&1
    if [ -x /build/linux/scripts/kconfig/conf ]; then
        cp /build/linux/scripts/kconfig/conf /usr/share/aegis/linux66/kconfig-conf
    fi
    if [ -f /build/linux/.config ]; then
        cp /build/linux/.config /usr/share/aegis/linux66/config.last
    fi
    cd /
    rm -rf /build/linux
    filelist /build/usr.list usr
    /build/mkustar /build/rootfs.tar . /build/usr.list
    catm /dev/outbox /build/rootfs.tar
    sha256sum -o /build/rootfs.sum /build/rootfs.tar
    read sum rest < /build/rootfs.sum
    echo "rootfs sha256 ${sum}"
    echo "rung done $1"
}

gcc -v
# Pinned stage 20 paxx and stage 19 mkustar sources are supplied unchanged.
gcc -static -O2 /steps/src/paxx.c -o /build/paxx
cp /steps/src/mkustar.c /build/mkustar.c
cd /build
patch -p1 -i /steps/src/mkustar-long.patch
gcc -static -O2 /build/mkustar.c -o /build/mkustar
finish() {
    rc=$?
    trap - EXIT
    if [ "$rc" != 0 ]; then
        echo "linux build failed status $rc; preserving guest checkpoint/logs"
        set +e
        pack_rootfs linux-diagnostic
    fi
    exit "$rc"
}
trap 'finish $?' EXIT
mkdir -p /build/linux
if [ "${1-}" = base-only ] &&
   [ "$(cat /usr/share/aegis/linux66/headers.source 2>/dev/null || :)" = 3c88ff4648122e81832b7067be6456ef28e119b64649a29e84f7f04691e489f9 ]; then
    echo 'linux source ok (guest headers checkpoint; archive reverified)'
else
    # The full upstream tree is extracted, not a hand-picked source subset.
    echo "linux start source-decompress"
    gzip -dc "${DISTFILES}/linux-6.6.tar.gz" > /build/linux.tar
    echo "linux source decompressed"
    echo "linux start source-extract"
    /build/paxx -C /build/linux -s 1 < /build/linux.tar
    rm /build/linux.tar
    echo 'linux source ok'
fi
cd /build/linux
export ARCH=riscv
# Kbuild assigns HOSTCC itself; command-line assignments must override it.
kbuild() {
    make -j1 ARCH=riscv HOSTCC='gcc -static' HOSTCXX='g++ -static' HOSTLDFLAGS=-static "$@"
}
export KBUILD_BUILD_USER=aegis KBUILD_BUILD_HOST=guest
export KBUILD_BUILD_TIMESTAMP='Thu Jan  1 00:00:00 UTC 1970' KBUILD_BUILD_VERSION=1
# musl's checkpoint has libc headers but no Linux UAPI headers. Generate
# them with Kbuild in the guest, then copy the resulting header directories.
# The headers target avoids requiring rsync merely to install those bytes.
if [ "$(cat /usr/share/aegis/linux66/headers.source 2>/dev/null || :)" = 3c88ff4648122e81832b7067be6456ef28e119b64649a29e84f7f04691e489f9 ]; then
    echo 'linux headers ok (guest checkpoint)'
else
    step headers kbuild headers
    cp -R /build/linux/usr/include/. /usr/include/
    echo 3c88ff4648122e81832b7067be6456ef28e119b64649a29e84f7f04691e489f9 > /usr/share/aegis/linux66/headers.source
fi
if [ -x /usr/share/aegis/linux66/busybox ] &&
   grep -q '^CONFIG_MKTEMP=y$' /usr/share/aegis/linux66/busybox.config; then
    echo 'linux busybox built (guest checkpoint)'
else
cd /build
# BusyBox's release uses GNU LongLink records, supported by our guest tar.
bzip2 -dc "${DISTFILES}/busybox-1.36.1.tar.bz2" | tar xf -
mv busybox-1.36.1 busybox
cd /build/busybox
patch -p1 -i /steps/patches/busybox-fixdep-read.patch
patch -p1 -i /steps/patches/busybox-split-find.patch
bbmake() {
    make -j1 HOSTCC='gcc -static' EXTRA_LDFLAGS=-static "$@"
}
bbconfigure() {
    bbmake allnoconfig
    # BusyBox's conf -n resets predefined bools. Merge after that reset and
    # use oldconfig, preserving supplied values and defaulting new settings.
    while IFS= read -r line; do
        key=${line%%=*}
        sed -i "/^${key}=/d; /^# ${key} is not set$/d" .config
        printf '%s\n' "$line" >> .config
    done < /steps/busybox.config
    awk 'BEGIN { for (i=0; i<4096; i++) print "" }' > /build/busybox.defaults
    bbmake oldconfig < /build/busybox.defaults
}
step busybox-config bbconfigure
step busybox-helpers /bin/sh /steps/tests/busybox-helpers.sh
for option in STATIC ASH SH_IS_ASH TAR MOUNT LS UNAME MKTEMP; do
    if ! grep -q "^CONFIG_${option}=y$" .config; then
        echo "linux build FAIL missing BusyBox CONFIG_${option}"
        exit 1
    fi
done
step busybox bbmake
if readelf -l busybox | grep -q INTERP; then
    echo 'linux build FAIL dynamic BusyBox'
    exit 1
fi
cp busybox /usr/share/aegis/linux66/busybox
cp .config /usr/share/aegis/linux66/busybox.config
echo 'linux busybox built'
fi
/usr/share/aegis/linux66/busybox sh -c 'test "$((6 * 7))" = 42'
/usr/share/aegis/linux66/busybox uname -m
tmp=$(/usr/share/aegis/linux66/busybox mktemp /tmp/aegis-build.XXXXXX)
test -f "$tmp"
/usr/share/aegis/linux66/busybox rm "$tmp"
ln -sf /usr/share/aegis/linux66/busybox /usr/bin/mktemp
echo 'linux native mktemp ok'
readelf -h /usr/share/aegis/linux66/busybox | grep -q 'RISC-V'
if readelf -l /usr/share/aegis/linux66/busybox | grep -q INTERP; then
    echo 'linux build FAIL dynamic checkpoint BusyBox'
    exit 1
fi
if [ "${1-}" = base-only ]; then
    echo 'linux base ok'
    pack_rootfs linux-base
    exit 0
fi
cd /build/linux
patch -p1 -i /steps/patches/linux-kconfig-fork-shell.patch
patch -p1 -i /steps/patches/linux-kconfig-shared-compare.patch
patch -p1 -i /steps/patches/linux-modpost-read.patch
patch -p1 -i /steps/patches/linux-sorttable-read-write.patch
patch -p1 -i /steps/patches/linux-preserve-linked-elf.patch
step kconfig-shell kbuild KBUILD_KCONFIG=/steps/tests/Kconfig.shell alldefconfig
cp .config /usr/share/aegis/linux66/Kconfig-shell.config
cat .config
grep -q '^CONFIG_AEGIS_SHELL_OK=y$' .config
grep -q '^# CONFIG_AEGIS_SHELL_FAIL is not set$' .config
grep -q '^CONFIG_AEGIS_SHELL_LINES="one two"$' .config
echo 'linux Kconfig real shell probes ok'
step config kbuild KCONFIG_ALLCONFIG=/steps/config allnoconfig
grep -q '^CONFIG_CC_IS_GCC=y$' .config
grep -q '^CONFIG_GCC_VERSION=90500$' .config
for option in 64BIT MMU SOC_VIRT RISCV_SBI FPU BINFMT_ELF BLK_DEV_INITRD TTY SERIAL_8250_CONSOLE SERIAL_OF_PLATFORM DEVTMPFS PROC_FS SYSFS SYSCTL PROC_SYSCTL BLOCK BLK_DEV VIRTIO_MENU VIRTIO_MMIO VIRTIO_BLK EXT4_FS; do
    if ! grep -q "^CONFIG_${option}=y$" .config; then
        echo "linux build FAIL missing CONFIG_${option}"
        exit 1
    fi
done
cp .config /usr/share/aegis/linux66/config
# Host programs here are RISC-V programs executed by Tilck.
step prepare kbuild prepare
step modpost-build kbuild scripts/mod/
step modpost-probe /bin/sh /steps/tests/modpost.sh
step sorttable-probe /bin/sh /steps/tests/sorttable.sh
# No cpio executable is needed: use the kernel's own generator.
step cpio-generator gcc -static -O2 usr/gen_init_cpio.c -o usr/gen_init_cpio
gcc -static -O2 /steps/init.c -o /build/init
cp /steps/init-list /build/init-list
usr/gen_init_cpio -t 0 /build/init-list > /usr/share/aegis/linux66/initramfs.cpio
if readelf -l /build/init | grep -q INTERP; then
    echo 'linux build FAIL dynamic init'
    exit 1
fi
echo 'linux init built'
# libfdt from this same kernel tarball (scripts/dtc/libfdt).
gcc -nostdlib -static -ffreestanding -fno-builtin -fno-stack-protector \
    -fno-pic -fno-pie -mno-relax -msmall-data-limit=0 -mcmodel=medany -march=rv64gc -mabi=lp64d \
    -O2 -I scripts/dtc/libfdt /steps/firmware/entry.S /steps/firmware/sbi.c \
    scripts/dtc/libfdt/fdt.c scripts/dtc/libfdt/fdt_ro.c \
    scripts/dtc/libfdt/fdt_rw.c scripts/dtc/libfdt/fdt_wip.c \
    -Wl,-T,/steps/firmware/link.ld -Wl,--no-relax -Wl,--build-id=none -lgcc -o /build/linux-sbi.elf
objcopy -O binary /build/linux-sbi.elf /usr/share/aegis/linux66/sbi.bin
echo 'linux firmware built'
step kernel kbuild Image
cp arch/riscv/boot/Image /usr/share/aegis/linux66/Image
cp vmlinux /usr/share/aegis/linux66/vmlinux
rm -f /usr/share/aegis/linux66/vmlinux.unsorted
echo 'linux kernel built'

pack_rootfs linux-build
