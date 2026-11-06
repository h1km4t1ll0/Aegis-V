# Rung gcc1 on Tilck (19-gcc464 README): tcc builds GCC 4.6.4 (Ekaitz's
# RISC-V fork), C only, --disable-bootstrap, into /usr/gcc1. run.kaem runs
# this with /bin/sh (bash 2.05b) from / ; /initrd is read-only.
# rung-init's environment: PREFIX BINDIR LIBDIR INCDIR DISTFILES ARCH PATH.
set -e

export SHELL=/bin/sh CONFIG_SHELL=/bin/sh
# tcc-m links a dynamic executable without -static (14-seed README); libtool
# takes -static as its own flag and passes -Wc,-static on (17-binutils README)
export LDFLAGS="-static -Wc,-static"
export lt_cv_sys_max_cmd_len=32768
TARGET=riscv64-unknown-linux-musl
GCC_TAR=gcc-4.6.4-c0d20242.tar
SRC=/build/gcc-src
OBJ=/build/gcc1
GCC1=/usr/gcc1

# 1. steps to /steps (checksum-transcriber writes sources.SHA256SUM there)
mkdir -p /steps /build/logs /tmp
cp -R /initrd/steps/. /steps/

# step LOG MARK CMD...: CMD's output in /build/logs/LOG.log, then its last
# two lines, the guest's seconds and MARK; or `gcc1 LOG FAIL`, the tail of
# the log and of the newest config.log, and exit 1
step() {
    s_log=/build/logs/$1.log
    s_mark=$2
    shift 2
    s_t0=$(date +%s)
    if "$@" > "${s_log}" 2>&1; then
        tail -2 "${s_log}"
        echo "gcc1 ${s_mark}: $(($(date +%s) - s_t0)) s"
        echo "${s_mark}"
    else
        echo "gcc1 $(basename "${s_log}" .log) FAIL"
        tail -60 "${s_log}"
        s_cl=$(ls -t config.log */config.log 2>/dev/null | head -1)
        if [ -n "${s_cl}" ]; then
            echo "--- ${s_cl}"
            tail -40 "${s_cl}"
        fi
        exit 1
    fi
}

# 2. the pinned source: sha256, unpack without what a C-only gcc-1 does
# not need (steps/excludes, tar -X patterns), the patches (README)
cd /steps
checksum-transcriber sources
sha256sum -c sources.SHA256SUM
t0=$(date +%s)
mkdir -p "${SRC}"
cd "${SRC}"
tar xf "${DISTFILES}/${GCC_TAR}" -X /steps/excludes
# tar 1.12 ignores the ustar prefix field: the 125 members whose path needs
# it (all under libjava/ and libstdc++-v3/, excluded above) come out at the
# top under their last name; the top of the tree has no such files
rm -rf explicit_instantiation *.cc *.class *.css *.h *.hpp *.html *.java
for p in /steps/patches/*.patch; do
    echo "Applying patch: ${p}"
    patch -p1 < "${p}"
done
echo "gcc1 source: $(find . -type f | wc -l) files, $(du -sk . | cut -f1) KB, $(($(date +%s) - t0)) s"
echo "gcc1 source ok"

# 3. tcc on Ekaitz's tcc reproducer (informational: the patch stays anyway)
mkdir -p /build/t
cd /build/t
tcc -static -o ssaidx /steps/tests/ssaidx.c
./ssaidx || echo "tcc ssaidx exit $?"

# 4. an empty /usr/lib/libm.a, as musl's own `make install` makes one
# (EMPTY_LIB_NAMES in musl's Makefile: libm is in libc.a); stage 13's list
# build installed none, and gcc/Makefile links build/genautomata with -lm
# ("tcc: error: library 'm' not found")
if ! test -e /usr/lib/libm.a; then
    ar rc /usr/lib/libm.a
fi

# 5. configure (plan task 2) in /build/gcc1. Run by the shell, not as a
# `#!` script: Tilck's execve() of a script takes at most 32 arguments
# (12-tilck README), this line has 31 (E2BIG). The Makefiles run the
# directories' configures with $(SHELL) themselves.
mkdir -p "${OBJ}"
cd "${OBJ}"
step configure "gcc1 configured" "${CONFIG_SHELL}" "${SRC}/configure" --prefix="${GCC1}" \
    --build="${TARGET}" --host="${TARGET}" --target="${TARGET}" \
    --enable-languages=c --disable-bootstrap --disable-multilib \
    --disable-nls --disable-shared --disable-libmudflap --disable-libssp \
    --disable-libgomp --disable-libquadmath --disable-lto \
    --disable-decimal-float --disable-threads --disable-libstdcxx-pch \
    --with-gmp=/usr --with-mpfr=/usr --with-mpc=/usr \
    --with-arch=rv64gc --with-abi=lp64d \
    CC="tcc -static" CC_FOR_BUILD="tcc -static" \
    CFLAGS=-g0 CFLAGS_FOR_BUILD=-g0 CFLAGS_FOR_TARGET="-O2 -g0" \
    AR=ar RANLIB=ranlib MAKEINFO=true

# 6. make, one subsystem per step (each make goal of the top-level Makefile
# configures its directory first)
M="make -j1 MAKEINFO=true"
step libiberty "libiberty built" ${M} all-libiberty
step libcpp "libcpp built" ${M} all-libcpp
step libdecnumber "libdecnumber built" ${M} all-libdecnumber
step support "gcc1 support built" ${M} maybe-all-build-libiberty \
    maybe-all-build-fixincludes maybe-all-zlib maybe-all-intl \
    maybe-all-fixincludes configure-gcc
# the generator programs (built by tcc) write insn-*.c, gtype-desc.c and
# their headers; tcc compiles them
step gen "gen* done" ${M} -C gcc insn-attrtab.o insn-automata.o \
    insn-emit.o insn-enums.o insn-extract.o insn-modes.o insn-opinit.o \
    insn-output.o insn-peep.o insn-preds.o insn-recog.o gtype-desc.o
# sha256 of the generated files, source path replaced by @SRC@ (README:
# the same as those of clang-built generators on the host)
for f in insn-attr.h insn-attrtab.c insn-automata.c insn-codes.h \
    insn-conditions.md insn-config.h insn-constants.h insn-emit.c \
    insn-enums.c insn-extract.c insn-flags.h insn-modes.c insn-modes.h \
    insn-opinit.c insn-output.c insn-peep.c insn-preds.c insn-recog.c \
    tm-preds.h tm-constrs.h genrtl.h gtype-desc.c gtype-desc.h \
    tree-check.h options.h; do
    sed "s|${SRC}|@SRC@|g" "gcc/${f}" > /tmp/gen.n
    sha256sum -o /tmp/gen.sum /tmp/gen.n
    read sum rest < /tmp/gen.sum
    echo "gen sha256 ${sum} ${f}"
done
step cc1 "cc1 built" ${M} -C gcc cc1
# the rest of gcc/: xgcc, cpp, collect2, specs, fixincludes, gcov
step xgcc "xgcc built" ${M} all-gcc
./gcc/xgcc -v 2>&1 | tail -1
# libgcc, built by xgcc + cc1 (the only target library of a C-only build)
step libgcc "libgcc built" ${M}
ls -l "${TARGET}/libgcc/libgcc.a"
# `make install` as its three parts (installdirs install-host install-target),
# gcc/ with INSTALL_HEADERS_DIR=install-headers-cp: its default copies the
# headers with `tar -cf - . | tar xpf -`, Tilck's pipe gives short writes
# and tar 1.12 stops ("Only wrote 4096 of 10240 bytes to -"). A variable on
# the top-level command line does not reach gcc/ (MAKEOVERRIDES=).
gcc1_install() {
    ${M} installdirs install-host-nogcc &&
    ${M} -C gcc install INSTALL_HEADERS_DIR=install-headers-cp &&
    ${M} install-target
}
step install "gcc1 installed" gcc1_install

# 7. tests (plan task 3): every program static (README, "Static linking")
cd /build/t
"${GCC1}/bin/gcc" -v 2>&1 | tail -1
"${GCC1}/bin/gcc" -static -o hello /steps/tests/hello.c
./hello
"${GCC1}/bin/gcc" -O2 -static -o sanity /steps/tests/sanity.c
./sanity

# 8. /usr packed with mkustar into /dev/outbox (as rung gmp)
cd /
filelist /build/usr.list usr
mkustar /build/rootfs.tar . /build/usr.list
catm /dev/outbox /build/rootfs.tar
sha256sum -o /build/rootfs.sum /build/rootfs.tar
read sum rest < /build/rootfs.sum
echo "rootfs sha256 ${sum}"
echo rung done gcc1
