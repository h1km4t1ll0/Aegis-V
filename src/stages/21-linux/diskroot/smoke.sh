#!/bin/bash
set -euo pipefail
export PATH=/usr/local/bin:/usr/bin:/usr/sbin:/bin:/sbin
export HOME=/root USER=root LOGNAME=root SHELL=/bin/bash
/sbin/root-proof
if [[ ${AEGIS_BOOT-} = shell ]]; then
    exec /sbin/console
fi
/bin/busybox sh -c 'test "$((6 * 7))" = 42'
echo 'linux busybox ok'
gcc -static -O2 /usr/local/src/diskroot/hello.c -o /tmp/hello
/tmp/hello
g++ -static -std=c++14 -O2 /usr/local/src/diskroot/hello.cpp -o /tmp/hello-cpp
/tmp/hello-cpp
echo 'linux toolchain ok'
/bin/bash -c 'declare -A a; a[answer]=42; test "${a[answer]}" = 42; echo "linux bash ok $BASH_VERSION"'
gcc -static -O2 -pthread /usr/local/src/diskroot/pthread.c -o /tmp/pthread-check
/tmp/pthread-check
boot=$(cat /proc/sys/kernel/random/boot_id)
case ${AEGIS_BOOT-verify} in
    first)
        test ! -e /root/diskroot-first-boot-id
        gcc -static -O2 /usr/local/src/diskroot/demo.c -o /usr/local/bin/aegis-demo
        printf 'answer=42\n' > /etc/aegis-demo.conf
        printf 'export AEGIS_SETTING=42\n' > /root/.bashrc
        printf '%s\n' "$boot" > /root/diskroot-first-boot-id
        sha256sum -o /root/diskroot.sha256 /usr/local/bin/aegis-demo /etc/aegis-demo.conf /root/.bashrc
        number=1
        ;;
    verify)
        test "$(cat /root/diskroot-first-boot-id)" != "$boot"
        number=2
        ;;
    *) echo 'unknown disk-root boot stage'; exit 1 ;;
esac
sha256sum -c /root/diskroot.sha256
/usr/local/bin/aegis-demo
/bin/bash -c 'source /root/.bashrc; test "$AEGIS_SETTING" = 42; echo "diskroot Bash setting persisted=42"'
echo "diskroot persistence boot${number} PASS boot=$boot"
if grep -q 'aegis.shell=1' /proc/cmdline; then
    /sbin/console
fi
