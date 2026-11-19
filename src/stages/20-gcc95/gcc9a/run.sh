# Rung gcc9a on Tilck (20-gcc95 README): g++ 4.6.4 (rung gcc4's /usr/bin/g++)
# builds upstream GCC 9.5.0 for C and C++ with --disable-bootstrap into
# /usr/gcc9a, against rung gmp9's gmp 6.1.0, mpfr 3.1.4 and mpc 1.0.3 in
# /usr. Then the gcc9a tests (C and C++). run.kaem runs this with /bin/sh
# (bash 2.05b) from / ; /initrd is read-only.
# rung-init's environment: PREFIX BINDIR LIBDIR INCDIR DISTFILES ARCH PATH.
# steps/src/paxx.c is 20-gcc95/src/paxx.c; steps/src/mkustar.c is stage
# 13's, steps/src/mkustar-long.patch rung gcc4's (tilck.py).
set -e

export SHELL=/bin/sh CONFIG_SHELL=/bin/sh
unset CC CXX CFLAGS CXXFLAGS LDFLAGS CPPFLAGS LIBS
TARGET=riscv64-unknown-linux-musl
SRC=/build/gcc-src
OBJ=/build/gcc9a
LOGS=/build/logs
PFX=/usr/gcc9a
OPT="-O2 -g0"
M="make -j1 MAKEINFO=true"

# 1. steps to /steps (checksum-transcriber writes sources.SHA256SUM there)
mkdir -p /steps "${LOGS}" /tmp
cp -R /initrd/steps/. /steps/

# watched LOG CMD...: CMD with its output in LOG, run in the background
# while this shell prints a heartbeat every 300 s (guest clock): seconds so
# far, lines of the log so far, its last line (cut to 100 bytes). As rung
# gmp9's: a rung's host timeout cannot tell a slow make from a host that
# slept. CMD's exit status in w_rc, through a file.
watched() {
    w_log=$1
    shift
    rm -f /tmp/watched.rc /tmp/watched.tmp
    (
        if "$@" > "${w_log}" 2>&1; then
            echo 0 > /tmp/watched.tmp
        else
            echo $? > /tmp/watched.tmp
        fi
        mv /tmp/watched.tmp /tmp/watched.rc
    ) &
    w_t0=$(date +%s)
    w_next=300
    while [ ! -f /tmp/watched.rc ]; do
        sleep 5
        w_dt=$(($(date +%s) - w_t0))
        if [ ${w_dt} -ge ${w_next} ]; then
            w_next=$((w_next + 300))
            echo "gcc9a heartbeat $(basename "${w_log}" .log): ${w_dt} s, $(wc -l < "${w_log}") lines, last: $(tail -1 "${w_log}" | cut -c1-100)"
        fi
    done
    wait
    read w_rc < /tmp/watched.rc
}

# step LOG MARK CMD...: CMD's output in /build/logs/LOG.log; then its last
# two lines, `gcc9a time LOG: N s` (guest clock) and MARK; or `gcc9a LOG FAIL`,
# the log's last 60 lines, the newest config.log's last 40 before its
# variable dump, and exit 1
step() {
    s_log=${LOGS}/$1.log
    s_mark=$2
    shift 2
    s_t0=$(date +%s)
    watched "${s_log}" "$@"
    if [ "${w_rc}" = 0 ]; then
        tail -2 "${s_log}"
        echo "gcc9a time $(basename "${s_log}" .log): $(($(date +%s) - s_t0)) s"
        echo "${s_mark}"
    else
        echo "gcc9a $(basename "${s_log}" .log) FAIL"
        tail -60 "${s_log}"
        f_cl=$(ls -t config.log */config.log */*/config.log 2>/dev/null | head -1)
        if [ -n "${f_cl}" ]; then
            echo "--- ${f_cl}"
            sed -n '/^## Cache variables/q;p' "${f_cl}" | tail -40
        fi
        exit 1
    fi
}

# 2. the pinned source; paxx and stage 13's mkustar with rung gcc4's
# mkustar-long.patch (for step 9: libstdc++'s pb_ds headers have names
# over 100 bytes), both by gcc 4.6.4
cd /steps
checksum-transcriber sources
sha256sum -c sources.SHA256SUM
echo "gcc9a gcc -v: $(gcc -v 2>&1 | tail -1)"
echo "gcc9a g++ -v: $(g++ -v 2>&1 | tail -1)"
gcc -static ${OPT} -Wall -o /build/paxx /steps/src/paxx.c
cp /steps/src/mkustar.c /build/mkustar.c
patch /build/mkustar.c < /steps/src/mkustar-long.patch
gcc -static -O2 -o /build/mkustar /build/mkustar.c

# 3. gcc-9.5.0.tar.gz (pax) through paxx into /build/gcc-src with
# excludes (README, "The GCC 9.5 source tree"); patches/*.patch, if any,
# with -p1
t0=$(date +%s)
gzip -dc "${DISTFILES}/gcc-9.5.0.tar.gz" | /build/paxx -C "${SRC}" -s 1 -X /steps/excludes
cd "${SRC}"
test "$(cat gcc/BASE-VER)" = 9.5.0
for p in /steps/patches/*.patch; do
    if [ -f "${p}" ]; then
        echo "Applying patch: ${p}"
        patch -p1 < "${p}"
    fi
done
echo "gcc9a source: $(find . -type f | wc -l) files, $(du -sk . | cut -f1) KB, $(($(date +%s) - t0)) s"
echo "gcc9a source ok"

# 4. configure (plan task 4). g++ 4.6.4 builds the compiler (GCC 9.5 is C++),
# gcc 4.6.4 the C parts (libiberty, libcpp's C, zlib, ...). Every host and
# build program static: LDFLAGS=-static (the top level adds its stage1
# -static-libstdc++ -static-libgcc in front; LDFLAGS_FOR_BUILD defaults
# to LDFLAGS when build = host). The target libraries are built by the new
# xgcc: LDFLAGS_FOR_TARGET=-static for their configure tests.
# --enable-version-specific-runtime-libs (README, "Runtime libraries"):
# libstdc++, libsupc++, libatomic and the C++ headers go to
# ${PFX}/lib/gcc/${TARGET}/9.5.0, the one library directory of its own the
# riscv driver searches (riscv.h's STARTFILE_PREFIX_SPEC replaces the
# ${PFX}/lib of other ports with /lib64/lp64d, /usr/lib64/lp64d, /lib,
# /usr/lib); run 1's g++ linked gcc 4.6.4's /usr/lib/libstdc++.a.
mkdir -p "${OBJ}"
cd "${OBJ}"
step configure "gcc9a configured" "${CONFIG_SHELL}" "${SRC}/configure" --prefix="${PFX}" \
    --build="${TARGET}" --host="${TARGET}" --target="${TARGET}" \
    --enable-languages=c,c++ --disable-bootstrap --disable-multilib --disable-nls \
    --disable-shared --enable-static --disable-lto --without-isl \
    --disable-libsanitizer --disable-libssp --disable-libgomp --disable-libitm \
    --disable-libquadmath --disable-libvtv --disable-symvers --disable-libstdcxx-pch \
    --enable-threads=posix --disable-sjlj-exceptions \
    --with-gmp=/usr --with-mpfr=/usr --with-mpc=/usr \
    --with-arch=rv64gc --with-abi=lp64d \
    --with-native-system-header-dir=/usr/include --enable-version-specific-runtime-libs \
    CC=gcc CXX=g++ CFLAGS="${OPT}" CXXFLAGS="${OPT}" LDFLAGS=-static \
    CFLAGS_FOR_TARGET="${OPT}" CXXFLAGS_FOR_TARGET="${OPT}" LDFLAGS_FOR_TARGET=-static \
    AR=ar RANLIB=ranlib MAKEINFO=true
grep -E '^(configdirs|TARGET_CONFIGDIRS|CC|CXX|CFLAGS|CXXFLAGS|LDFLAGS|STAGE1_LDFLAGS|LDFLAGS_FOR_BUILD|CFLAGS_FOR_TARGET|CXXFLAGS_FOR_TARGET|LDFLAGS_FOR_TARGET) =' Makefile

# 5. make, one subsystem per step (as rung gcc4). gcc/configure drops -O
# from CFLAGS and CXXFLAGS unless CC is a bootstrap's prev-gcc/xgcc; the
# top level passes its own CFLAGS/CXXFLAGS down, a `make -C gcc` does not:
# the host steps give both on make's command line.
H="CFLAGS=${OPT}"
HX="CXXFLAGS=${OPT}"
step support "gcc9a support built" ${M} "${H}" "${HX}" all-libiberty all-libcpp \
    all-libdecnumber all-libbacktrace maybe-all-build-libiberty maybe-all-build-libcpp \
    maybe-all-build-fixincludes maybe-all-zlib maybe-all-intl maybe-all-fixincludes \
    configure-gcc
grep -E '^(CFLAGS|CXXFLAGS|LDFLAGS|BUILD_LDFLAGS|enable_plugin|PLUGINLIBS) =' gcc/Makefile || true
grep -E 'HAVE_AS_RISCV_ATTRIBUTE|HAVE_AS_GNU_ATTRIBUTE|HAVE_AS_LEB128|HAVE_GAS_CFI_DIRECTIVE |HAVE_AS_TLS |ENABLE_PLUGIN' gcc/auto-host.h || true
step cc1 "cc1 built" ${M} "${H}" "${HX}" -C gcc cc1
step cc1plus "cc1plus built" ${M} "${H}" "${HX}" -C gcc cc1plus
# the rest of gcc/: xgcc, g++, cpp, collect2, lto-wrapper, gcov, specs,
# fixincludes' headers, the selftest runs
step xgcc "xgcc built" ${M} "${H}" "${HX}" all-gcc
./gcc/xgcc -v 2>&1 | tail -1
step libgcc "libgcc built" ${M} all-target-libgcc
step libstdcxx "libstdc++ built" ${M} all-target-libstdc++-v3
# whatever `all` has left (libatomic, ...)
step all "gcc9a all built" ${M}
ls -l gcc/cc1 gcc/cc1plus gcc/xgcc gcc/xg++ "${TARGET}/libgcc/libgcc.a" \
    "${TARGET}/libstdc++-v3/src/.libs/libstdc++.a" || true

# 6. make install into /usr/gcc9a
step install "gcc9a installed" ${M} install
LIBEXEC=${PFX}/libexec/gcc/${TARGET}/9.5.0
ls -l "${PFX}/bin" "${LIBEXEC}/cc1" "${LIBEXEC}/cc1plus" \
    "${PFX}/lib/gcc/${TARGET}/9.5.0/libgcc.a" || true
find "${PFX}" -name '*.a' | xargs ls -l || true
for f in "${LIBEXEC}/cc1" "${LIBEXEC}/cc1plus" "${PFX}/bin/gcc" "${PFX}/bin/g++"; do
    echo "gcc9a $(basename "${f}"): $(readelf -l "${f}" | grep -c INTERP || true) INTERP"
done
echo "gcc9a size: $(du -sk "${PFX}" | cut -f1) KB, $(find "${PFX}" -type f | wc -l) files"

# 7. tests (plan task 5), every program static (19-gcc464 README, "Static
# linking"). Each test runs even if an earlier one failed (all failures
# show in one run); any failure stops the script before the packing, with
# `gcc9a tests FAIL N` and exit 1 (no rootfs, no `rung done`).
mkdir -p /build/t
cd /build/t
t_bad=0
t_fail() {
    echo "gcc9a test FAIL $*"
    t_bad=$((t_bad + 1))
}
echo "gcc9a gcc -v: $(${PFX}/bin/gcc -v 2>&1 | tail -1)"
echo "gcc9a g++ -v: $(${PFX}/bin/g++ -v 2>&1 | tail -1)"
${PFX}/bin/gcc -v 2>&1 | grep -q '^gcc version 9\.5\.0 ' || t_fail "gcc -v"
# the runtime libraries the driver finds: gcc9a's own, not gcc 4.6.4's
# /usr/lib/libstdc++.a (run 1)
for f in libstdc++.a libsupc++.a libatomic.a libgcc.a libc.a crt1.o crtbeginT.o; do
    t_f=$(${PFX}/bin/g++ -print-file-name=${f})
    echo "gcc9a g++ -print-file-name=${f}: ${t_f}"
    case ${f}:${t_f} in
    libc.a:/usr/lib/libc.a | crt1.o:/usr/lib/crt1.o) ;;
    libc.a:* | crt1.o:*) t_fail "${f} is ${t_f}" ;;
    *:${PFX}/lib/gcc/${TARGET}/9.5.0/${f}) ;;
    *) t_fail "${f} is ${t_f}" ;;
    esac
done
# t_run NAME OUT EXPECT: ./NAME > NAME.out must exit 0 and print the line
# EXPECT (a grep -x pattern)
t_run() {
    if ./$1 > $1.out 2>&1; then
        cat $1.out
        grep -q -x "$2" $1.out || t_fail "$1: no '$2'"
    else
        r_rc=$?
        cat $1.out
        t_fail "$1: exit ${r_rc}"
    fi
}
if ${PFX}/bin/gcc -static -o hello /steps/tests/hello.c; then
    t_run hello 'hello from gcc 9\.5\.0'
else
    t_fail "hello.c: gcc failed"
fi
if ${PFX}/bin/g++ -static -O2 -o hello-cpp /steps/tests/hello.cpp; then
    t_run hello-cpp 'hello from g++ 9\.5\.0'
else
    t_fail "hello.cpp: g++ failed"
fi
san_bad=${t_bad}
if ${PFX}/bin/gcc -static -O2 -Wall -o sanity /steps/tests/sanity.c; then
    t_run sanity 'gcc9a sanity c ok'
else
    t_fail "sanity.c: gcc failed"
fi
if ${PFX}/bin/g++ -static -O2 -Wall -o sanity-cpp /steps/tests/sanity.cpp; then
    t_run sanity-cpp 'gcc9a sanity c++ ok'
else
    t_fail "sanity.cpp: g++ failed"
fi
for f in hello hello-cpp sanity sanity-cpp; do
    if [ -f ${f} ]; then
        echo "gcc9a test ${f}: $(wc -c < ${f}) bytes, $(readelf -l ${f} | grep -c INTERP || true) INTERP"
        if readelf -l ${f} | grep -q INTERP; then
            t_fail "${f}: dynamic"
        fi
    fi
done
if [ ${t_bad} = ${san_bad} ]; then
    echo "gcc9a sanity ok"
fi
if [ ${t_bad} != 0 ]; then
    echo "gcc9a tests FAIL ${t_bad}"
    exit 1
fi

# 8. the build tree out of RAM before packing (Tilck's / is in memory)
cd /
rm -rf "${OBJ}" "${SRC}"

# 9. /usr packed with step 2's mkustar into /dev/outbox (as rung gmp9)
filelist /build/usr.list usr
/build/mkustar /build/rootfs.tar . /build/usr.list
catm /dev/outbox /build/rootfs.tar
sha256sum -o /build/rootfs.sum /build/rootfs.tar
read sum rest < /build/rootfs.sum
echo "rootfs sha256 ${sum}"
echo rung done gcc9a
