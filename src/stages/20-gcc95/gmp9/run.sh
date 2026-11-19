# Rung gmp9 on Tilck (20-gcc95 README): gcc 4.6.4 (rung gcc4's /usr/bin/gcc)
# builds paxx, our pax extractor, tests it on an archive written by
# tests/paxw.c and on gcc-9.5.0.tar.gz; then builds gmp 6.1.0, mpfr 3.1.4
# and mpc 1.0.3 (GCC 9.5's contrib/download_prerequisites), each with its
# own configure, make, make check, make install into /usr in place of rung
# gmp's tcc-built gmp 4.3.2, mpfr 2.4.2, mpc 0.8.1. run.kaem runs this with
# /bin/sh (bash 2.05b) from / ; /initrd is read-only.
# rung-init's environment: PREFIX BINDIR LIBDIR INCDIR DISTFILES ARCH PATH.
# steps/src/paxx.c is 20-gcc95/src/paxx.c, steps/gcc9a/excludes rung
# gcc9a's; steps/src/mkustar.c is stage 13's, steps/src/mkustar-long.patch
# rung gcc4's (tilck.py).
set -e

export SHELL=/bin/sh CONFIG_SHELL=/bin/sh
unset CC CFLAGS LDFLAGS CPPFLAGS LIBS
TARGET=riscv64-unknown-linux-musl
LOGS=/build/logs
# every program static (19-gcc464 README, "Static linking"): in CC, so that
# libtool's links get it too (libtool takes -static in LDFLAGS as its own)
GCC="gcc -static"
OPT="-O2 -g0"
M="make -j1 MAKEINFO=true"

# 1. steps to /steps (checksum-transcriber writes sources.SHA256SUM there)
mkdir -p /steps "${LOGS}" /tmp
cp -R /initrd/steps/. /steps/

# watched LOG CMD...: CMD with its output in LOG, run in the background
# while this shell prints a heartbeat every 300 s (guest clock): seconds so
# far, PASS/FAIL lines of the log so far, its last line (cut to 100 bytes).
# A rung's host timeout cannot tell a slow or hung make check from a host
# that slept (run 1); this shows on the UART where the guest is. CMD's exit
# status in w_rc. The status goes through a file: no kill -0 or wait for a
# status on Tilck needed.
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
            echo "gmp9 heartbeat $(basename "${w_log}" .log): ${w_dt} s, $(grep -c '^PASS' "${w_log}") PASS, $(grep -c -E '^(FAIL|ERROR)' "${w_log}") FAIL, last: $(tail -1 "${w_log}" | cut -c1-100)"
        fi
    done
    wait
    read w_rc < /tmp/watched.rc
}

# step LOG MARK CMD...: CMD's output in /build/logs/LOG.log; then its last
# two lines, `gmp9 MARK: N s` (guest clock) and MARK; or `gmp9 LOG FAIL`,
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
        echo "gmp9 ${s_mark}: $(($(date +%s) - s_t0)) s"
        echo "${s_mark}"
    else
        echo "gmp9 $(basename "${s_log}" .log) FAIL"
        tail -60 "${s_log}"
        f_cl=$(ls -t config.log */config.log 2>/dev/null | head -1)
        if [ -n "${f_cl}" ]; then
            echo "--- ${f_cl}"
            sed -n '/^## Cache variables/q;p' "${f_cl}" | tail -40
        fi
        exit 1
    fi
}

# check LIB: `make check` (automake's parallel harness: `# TOTAL: N`,
# `# PASS: N`, ... per directory) to /build/logs/LIB-check.log; the sums,
# every SKIP/XFAIL/FAIL/XPASS/ERROR line, the seconds and make's status;
# `LIB check ok` only if make returned 0 and nothing failed. Otherwise
# `LIB check FAIL` and the head of each test-suite.log with a failure; the
# script goes on (the next library and the packing still run; the host
# misses the mark and fails the rung).
check() {
    c_log=${LOGS}/$1-check.log
    c_t0=$(date +%s)
    watched "${c_log}" ${M} check
    c_rc=${w_rc}
    awk -v lib="$1" '/^# (TOTAL|PASS|SKIP|XFAIL|FAIL|XPASS|ERROR):/ { s[$2] += $3; d += ($2 == "TOTAL:") }
        END { printf "%s check: %d directories, total %d, pass %d, skip %d, xfail %d, fail %d, xpass %d, error %d\n",
              lib, d, s["TOTAL:"], s["PASS:"], s["SKIP:"], s["XFAIL:"], s["FAIL:"], s["XPASS:"], s["ERROR:"] }' "${c_log}"
    grep -E '^(SKIP|XFAIL|FAIL|XPASS|ERROR):' "${c_log}" || true
    echo "$1 check: $(($(date +%s) - c_t0)) s, make check exit ${c_rc}"
    if [ "${c_rc}" = 0 ] && ! grep -q -E '^(FAIL|XPASS|ERROR):' "${c_log}"; then
        echo "$1 check ok"
    else
        echo "$1 check FAIL"
        chk_bad=$((chk_bad + 1))
        tail -30 "${c_log}"
        for c_tl in $(find . -name test-suite.log); do
            if grep -q -E '^(FAIL|XPASS|ERROR):' "${c_tl}"; then
                echo "--- ${c_tl}"
                head -150 "${c_tl}"
            fi
        done
    fi
}

chk_bad=0

# 2. the pinned sources: gmp, mpfr, mpc, gcc 9.5.0
cd /steps
checksum-transcriber sources
sha256sum -c sources.SHA256SUM
echo "gmp9 gcc -v: $(gcc -v 2>&1 | tail -1)"

# 3. paxx (src/paxx.c), the test writer (tests/paxw.c), and stage 13's
# mkustar with long names for step 9 (rung gcc4's mkustar-long.patch:
# /usr/include/c++/4.6.4/ext/pb_ds/ has names over 100 bytes)
${GCC} ${OPT} -Wall -o /build/paxx /steps/src/paxx.c
${GCC} ${OPT} -Wall -o /build/paxw /steps/tests/paxw.c
cp /steps/src/mkustar.c /build/mkustar.c
patch /build/mkustar.c < /steps/src/mkustar-long.patch
${GCC} -O2 -o /build/mkustar /build/mkustar.c

# 4. paxx test (plan task 1): paxw's archive through gzip and a pipe,
# extracted with -s 1 and four exclude lines; paxx's counts, paxw's check
# of the tree; an unsupported type and a bad checksum must stop paxx with
# the member's name
pt_ok=1
pt_fail() {
    echo "paxx test FAIL $*"
    pt_ok=0
}
mkdir -p /build/paxt
cd /build/paxt
printf 'skip/*\n!skip/keep.txt\n*.gmo\ngone\n' > excludes
/build/paxw make > t.tar
gzip -c t.tar > t.tar.gz
echo "paxx test archive: $(wc -c < t.tar) bytes"
gzip -dc t.tar.gz | /build/paxx -C out -s 1 -X excludes > paxx.out 2>&1 || pt_fail "paxx exit $?"
cat paxx.out
test "$(sed -n 1p paxx.out)" = "paxx: 6 files, 2 dirs, 3 links, 4 pax headers" || pt_fail counts
test "$(sed -n 2p paxx.out)" = "paxx: 4 excluded" || pt_fail excluded
/build/paxw check out || pt_fail check
if /build/paxw bad-type | /build/paxx -C neg1 > neg1.out 2>&1; then
    pt_fail "bad-type: exit 0"
fi
cat neg1.out
grep -q "^paxx: t/dev/null0: unsupported type '3'" neg1.out || pt_fail bad-type
if /build/paxw bad-sum | /build/paxx -C neg2 > neg2.out 2>&1; then
    pt_fail "bad-sum: exit 0"
fi
cat neg2.out
grep -q '^paxx: t/badsum.txt: bad header checksum' neg2.out || pt_fail bad-sum
if [ ${pt_ok} = 1 ]; then
    echo "paxx ok"
fi

# 5. trial extraction of gcc-9.5.0.tar.gz with rung gcc9a's excludes (plan
# task 3) into /build/gcc-src: paxx's counts, find's counts, a digest of
# the tree (md5sum of every file, sorted by name, then md5sum of that
# list; the host's tarfile gives the same), the 7 pb_ds headers whose
# names are over 100 bytes below gcc-9.5.0/. Removed again: rung gcc9a
# extracts it itself.
pg_ok=1
pg_fail() {
    echo "paxx gcc FAIL $*"
    pg_ok=0
}
t0=$(date +%s)
gzip -dc "${DISTFILES}/gcc-9.5.0.tar.gz" | /build/paxx -C /build/gcc-src -s 1 \
    -X /steps/gcc9a/excludes > "${LOGS}/paxx-gcc.log" 2>&1 || pg_fail "paxx exit $?"
cat "${LOGS}/paxx-gcc.log"
echo "paxx gcc: $(($(date +%s) - t0)) s"
test "$(sed -n 1p "${LOGS}/paxx-gcc.log")" = "paxx: 7704 files, 520 dirs, 0 links, 94878 pax headers" \
    || pg_fail counts
test "$(sed -n 2p "${LOGS}/paxx-gcc.log")" = "paxx: 86653 excluded" || pg_fail excluded
cd /build/gcc-src
pg_f=$(find . -type f | wc -l)
pg_d=$(find . -type d | wc -l)
pg_k=$(du -sk . | cut -f1)
echo "paxx gcc: find ${pg_f} files, ${pg_d} dirs, ${pg_k} KB, BASE-VER $(cat gcc/BASE-VER)"
test ${pg_f} = 7704 || pg_fail "find files"
test ${pg_d} = 521 || pg_fail "find dirs"
test "$(cat gcc/BASE-VER)" = 9.5.0 || pg_fail BASE-VER
t0=$(date +%s)
pg_sum=$(find . -type f | sort | xargs -n 200 md5sum | md5sum | cut -d' ' -f1)
echo "paxx gcc: tree md5 ${pg_sum}, $(($(date +%s) - t0)) s"
test "${pg_sum}" = 74830983c2bc5f6eb5c9a8e15b48002d || pg_fail "tree md5"
for h in cc_hash_table_map_/constructor_destructor_no_store_hash_fn_imps.hpp \
    cc_hash_table_map_/constructor_destructor_store_hash_fn_imps.hpp \
    gp_hash_table_map_/constructor_destructor_no_store_hash_fn_imps.hpp \
    gp_hash_table_map_/constructor_destructor_store_hash_fn_imps.hpp \
    left_child_next_sibling_heap_/constructors_destructor_fn_imps.hpp \
    left_child_next_sibling_heap_/left_child_next_sibling_heap_.hpp \
    resize_policy/cc_hash_max_collision_check_resize_trigger_imp.hpp; do
    if test -s "libstdc++-v3/include/ext/pb_ds/detail/${h}"; then
        echo "pb_ds $(wc -c < "libstdc++-v3/include/ext/pb_ds/detail/${h}") ${h}"
    else
        pg_fail "missing ${h}"
    fi
done
ls libstdc++-v3/testsuite
test -f libstdc++-v3/testsuite/Makefile.in || pg_fail "testsuite/Makefile.in"
find . -name '*PaxHeaders*' | head -3
cd /
rm -rf /build/gcc-src
if [ ${pg_ok} = 1 ]; then
    echo "paxx gcc ok"
fi

# 6. rung gmp's libraries out of /usr, so the new ones do not mix with them
rm -f /usr/include/gmp.h /usr/include/mpfr.h /usr/include/mpf2mpfr.h /usr/include/mpc.h \
    /usr/lib/libgmp.a /usr/lib/libgmp.la /usr/lib/libmpfr.a /usr/lib/libmpfr.la \
    /usr/lib/libmpc.a /usr/lib/libmpc.la /usr/share/info/gmp.info /usr/share/info/gmp.info-1 \
    /usr/share/info/gmp.info-2 /usr/share/info/mpfr.info /usr/share/info/mpc.info
rm -rf /usr/share/doc/mpfr

# 7. gmp 6.1.0, mpfr 3.1.4, mpc 1.0.3 (plan task 2), in this order (mpfr
# needs gmp in /usr, mpc both): configure (native: --build = --host),
# make, make check, make install. The tarballs' own config.sub (2014-2015)
# know riscv64-unknown-linux-musl; --build is given, so config.guess is
# not run (it cannot guess Tilck).
cd /build
bzip2 -dc "${DISTFILES}/gmp-6.1.0.tar.bz2" | tar xf -
cd gmp-6.1.0
step gmp-configure "gmp configured" ${CONFIG_SHELL} ./configure --prefix=/usr \
    --build="${TARGET}" --host="${TARGET}" --disable-shared --enable-static \
    --disable-assembly CC="${GCC}" CC_FOR_BUILD="${GCC}" CFLAGS="${OPT}" MAKEINFO=true
grep -E '^#define (GMP_LIMB_BITS|__GMP_CC|__GMP_CFLAGS) ' gmp.h || true
echo "gmp mpn: $(ls mpn/*.c | wc -l) C files, $(ls mpn/*.asm mpn/*.s mpn/*.S 2>/dev/null | wc -l) asm files"
step gmp-make "gmp built" ${M}
check gmp
step gmp-install "gmp installed" ${M} install

cd /build
bzip2 -dc "${DISTFILES}/mpfr-3.1.4.tar.bz2" | tar xf -
cd mpfr-3.1.4
step mpfr-configure "mpfr configured" ${CONFIG_SHELL} ./configure --prefix=/usr \
    --build="${TARGET}" --host="${TARGET}" --disable-shared --enable-static \
    --with-gmp=/usr CC="${GCC}" CFLAGS="${OPT}" MAKEINFO=true
grep -E 'long double|HAVE_LDOUBLE|MPFR_WANT' config.log | grep -v '^|' | head -10 || true
step mpfr-make "mpfr built" ${M}
check mpfr
step mpfr-install "mpfr installed" ${M} install

cd /build
gzip -dc "${DISTFILES}/mpc-1.0.3.tar.gz" | tar xf -
cd mpc-1.0.3
step mpc-configure "mpc configured" ${CONFIG_SHELL} ./configure --prefix=/usr \
    --build="${TARGET}" --host="${TARGET}" --disable-shared --enable-static \
    --with-gmp=/usr --with-mpfr=/usr CC="${GCC}" CFLAGS="${OPT}" MAKEINFO=true
step mpc-make "mpc built" ${M}
check mpc
step mpc-install "mpc installed" ${M} install

# 8. the installed headers and libraries, without rung gcc4's alloca.o
# (gcc 4.6.4 inlines alloca; tcc's gmp called libtcc1's). `gmp9 ok` if
# libs-test passes and so did steps 4, 5 and the three make checks.
ls -l /usr/include/gmp.h /usr/include/mpfr.h /usr/include/mpf2mpfr.h /usr/include/mpc.h \
    /usr/lib/libgmp.a /usr/lib/libmpfr.a /usr/lib/libmpc.a
echo "gmp9 undefined alloca: $(nm /usr/lib/libgmp.a /usr/lib/libmpfr.a /usr/lib/libmpc.a | grep -c ' U alloca$' || true)"
mkdir -p /build/t
cd /build/t
if ${GCC} ${OPT} -o libs-test /steps/tests/libs-test.c -lmpc -lmpfr -lgmp && ./libs-test; then
    if [ ${pt_ok} = 1 ] && [ ${pg_ok} = 1 ] && [ ${chk_bad} = 0 ]; then
        echo "gmp9 ok"
    fi
else
    echo "gmp9 libs-test FAIL"
fi

# 9. /usr packed with step 3's mkustar into /dev/outbox (as rung gcc4)
cd /
filelist /build/usr.list usr
/build/mkustar /build/rootfs.tar . /build/usr.list
catm /dev/outbox /build/rootfs.tar
sha256sum -o /build/rootfs.sum /build/rootfs.tar
read sum rest < /build/rootfs.sum
echo "rootfs sha256 ${sum}"
echo rung done gmp9
