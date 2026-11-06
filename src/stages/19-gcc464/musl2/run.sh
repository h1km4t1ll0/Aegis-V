# Rung musl2 on Tilck (19-gcc464 README): gcc-1 (/usr/gcc1, rung gcc1)
# builds musl 1.1.24 with musl's own configure and Makefile and installs it
# into /usr over the tcc-built musl of stage 13. run.kaem runs this with
# /bin/sh (bash 2.05b) from / ; /initrd is read-only.
# rung-init's environment: PREFIX BINDIR LIBDIR INCDIR DISTFILES ARCH PATH.
set -e

export SHELL=/bin/sh CONFIG_SHELL=/bin/sh
MUSL=musl-1.1.24
GCC1=/usr/gcc1/bin/gcc
T=/build/t

# 1. steps to /steps (checksum-transcriber writes sources.SHA256SUM there)
mkdir -p /steps /build/logs /tmp
cp -R /initrd/steps/. /steps/

# step LOG MARK CMD...: CMD's output in /build/logs/LOG.log, then its last
# two lines, the guest's seconds and MARK; or `musl2 LOG FAIL`, the tail of
# the log and of config.log, and exit 1
step() {
    s_log=/build/logs/$1.log
    s_mark=$2
    shift 2
    s_t0=$(date +%s)
    if "$@" > "${s_log}" 2>&1; then
        tail -2 "${s_log}"
        echo "musl2 ${s_mark}: $(($(date +%s) - s_t0)) s"
        echo "${s_mark}"
    else
        echo "musl2 $(basename "${s_log}" .log) FAIL"
        tail -60 "${s_log}"
        if [ -f config.log ]; then
            echo "--- config.log"
            tail -40 config.log
        fi
        exit 1
    fi
}

# 2. the pinned source
cd /steps
checksum-transcriber sources
sha256sum -c sources.SHA256SUM
cd /build
gzip -dc "${DISTFILES}/${MUSL}.tar.gz" | tar xf -
cd "/build/${MUSL}"
echo "musl2 source: $(find . -type f | wc -l) files"
echo "musl2 source ok"

# 3. musl's configure, run by the shell (Tilck's `#!` takes at most 32
# arguments, 12-tilck README), with gcc-1 as the only compiler named: CC
# from the command line, no CFLAGS/LDFLAGS from the environment. AR and
# RANLIB are binutils' ar/ranlib from PATH (config.mak). No libc.so: Tilck
# runs static programs only.
unset CC CFLAGS LDFLAGS CPPFLAGS
step configure "musl2 configured" /bin/sh ./configure --prefix=/usr \
    --disable-shared CC="${GCC1}"
cat config.mak

# 4. make, make install (lib/libc.a and the crt*.o over the tcc-built ones,
# the empty libm.a & co., /usr/include)
old=$(wc -c < /usr/lib/libc.a)
step make "musl2 built" make -j1
step install "musl2 installed" make install
echo "musl2 libc.a: tcc-built ${old} bytes, gcc-1-built $(wc -c < /usr/lib/libc.a) bytes"
ls -l /usr/lib/libc.a /usr/lib/crt1.o /usr/lib/crti.o /usr/lib/crtn.o /usr/lib/libm.a

# 5. tests: stage 13's hello-musl and musl-proc, built by gcc-1 against
# musl-2. musl-proc execs itself as /initrd/bin/musl-proc and looks for both
# programs in /initrd/bin: here they are in /build/t/bin.
mkdir -p "${T}/bin"
cd "${T}"
sed 's|/initrd/bin|/build/t/bin|g' /steps/tests/musl-proc.c > musl-proc.c
"${GCC1}" -static -O2 -o bin/hello-musl /steps/tests/hello-musl.c
"${GCC1}" -static -O2 -o bin/musl-proc musl-proc.c
"${GCC1}" -static -O2 -o complex /steps/tests/complex.c
./bin/hello-musl > hello.out
cat hello.out
./bin/musl-proc > proc.out
cat proc.out
./complex
if [ "$(cat hello.out)" = "hello from musl" ] && [ "$(cat proc.out)" = "musl proc ok" ]; then
    echo "musl2 ok"
else
    echo "musl2 FAIL"
    exit 1
fi
# tar 1.12 through a pipe, as gcc's install-headers-tar (rung gcc3) copies
# its headers: it stopped on short pipe writes before Tilck's pipe_write()
# fix (12-tilck README)
mkdir -p /build/tarpipe
if (cd /usr/include && tar -cf - .) | (cd /build/tarpipe && tar xpf -) \
    && diff -r /usr/include /build/tarpipe > /dev/null; then
    echo "musl2 tar pipe ok"
else
    echo "musl2 tar pipe FAIL"
    exit 1
fi
# informational: tcc-m against musl-2's libc.a
if tcc -static -o tcc-hello /steps/tests/hello-musl.c > tcc.log 2>&1; then
    echo "musl2 tcc: $(./tcc-hello)"
else
    echo "musl2 tcc: link failed"
    tail -20 tcc.log
fi

# 6. /usr packed with mkustar into /dev/outbox (as rung gcc1)
cd /
filelist /build/usr.list usr
mkustar /build/rootfs.tar . /build/usr.list
catm /dev/outbox /build/rootfs.tar
sha256sum -o /build/rootfs.sum /build/rootfs.tar
read sum rest < /build/rootfs.sum
echo "rootfs sha256 ${sum}"
echo rung done musl2
