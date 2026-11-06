# Rung gcc3 on Tilck (19-gcc464 README): gcc-1 (/usr/gcc1) and musl-2 run
# GCC 4.6.4's `make bootstrap` for C (stage1 by gcc-1, stage2 by stage1,
# stage3 by stage2, stage2 and stage3 objects compared); the result goes
# into /usr and /usr/gcc1 goes away. run.kaem runs this with /bin/sh
# (bash 2.05b) from / ; /initrd is read-only.
# rung-init's environment: PREFIX BINDIR LIBDIR INCDIR DISTFILES ARCH PATH.
# steps/sources, excludes, patches/ and tests/sanity.c are rung gcc1's;
# patches/riscv-rtx-costs.patch is this rung's own (also rung gcc4's).
set -e

export SHELL=/bin/sh CONFIG_SHELL=/bin/sh
unset CC CFLAGS LDFLAGS CPPFLAGS
TARGET=riscv64-unknown-linux-musl
GCC_TAR=gcc-4.6.4-c0d20242.tar
SRC=/build/gcc-src
OBJ=/build/gcc3
LOGS=/build/logs

# 1. steps to /steps (checksum-transcriber writes sources.SHA256SUM there)
mkdir -p /steps "${LOGS}" /tmp
cp -R /initrd/steps/. /steps/

# step LOG MARK CMD...: as in rung gcc1
step() {
    s_log=${LOGS}/$1.log
    s_mark=$2
    shift 2
    s_t0=$(date +%s)
    if "$@" > "${s_log}" 2>&1; then
        tail -2 "${s_log}"
        echo "gcc3 ${s_mark}: $(($(date +%s) - s_t0)) s"
        echo "${s_mark}"
    else
        echo "gcc3 $(basename "${s_log}" .log) FAIL"
        fail_tail "${s_log}"
    fi
}

# fail_tail LOG: the log's last 60 lines, the newest config.log's last 40
# before its variable dump (the failed test)
fail_tail() {
    tail -60 "$1"
    f_cl=$(ls -t config.log */config.log */*/config.log 2>/dev/null | head -1)
    if [ -n "${f_cl}" ]; then
        echo "--- ${f_cl}"
        sed -n '/^## Cache variables/q;p' "${f_cl}" | tail -40
    fi
    exit 1
}

# 2. the pinned source as in rung gcc1: sha256, unpack with gcc1's
# excludes, remove tar 1.12's ustar-prefix strays, gcc1's patches and
# riscv-rtx-costs.patch
cd /steps
checksum-transcriber sources
sha256sum -c sources.SHA256SUM
t0=$(date +%s)
mkdir -p "${SRC}"
cd "${SRC}"
tar xf "${DISTFILES}/${GCC_TAR}" -X /steps/excludes
rm -rf explicit_instantiation *.cc *.class *.css *.h *.hpp *.html *.java
for p in /steps/patches/*.patch; do
    echo "Applying patch: ${p}"
    patch -p1 < "${p}"
done
echo "gcc3 source: $(find . -type f | wc -l) files, $(du -sk . | cut -f1) KB, $(($(date +%s) - t0)) s"
echo "gcc3 source ok"

# tcc-built gmp, mpfr and mpc (rung gmp) call `alloca` as a function of
# tcc's runtime (/usr/lib/tcc/libtcc1.a, alloca.o), which tcc linked into
# gcc-1 and gcc does not link ("undefined reference to `alloca'" from
# libmpfr.a and libgmp.a; configure: "Building GCC requires GMP 4.2+ ...").
# That object goes into every link of the build: LIBS, exported, reaches
# every stage's configure and gcc/Makefile.
mkdir -p /build/tccrt
cd /build/tccrt
ar x /usr/lib/tcc/libtcc1.a alloca.o
export LIBS=/build/tccrt/alloca.o

# 3. configure (plan task 5): rung gcc1's line with --prefix=/usr, without
# --disable-bootstrap, gcc-1 as CC. Every link static: LDFLAGS (stage1,
# and LDFLAGS_FOR_BUILD by default) and --with-boot-ldflags (stages 2-3;
# its default -static-libstdc++ -static-libgcc links dynamically).
mkdir -p "${OBJ}"
cd "${OBJ}"
step configure "gcc3 configured" "${CONFIG_SHELL}" "${SRC}/configure" --prefix=/usr \
    --build="${TARGET}" --host="${TARGET}" --target="${TARGET}" \
    --enable-languages=c --disable-multilib \
    --disable-nls --disable-shared --disable-libmudflap --disable-libssp \
    --disable-libgomp --disable-libquadmath --disable-lto \
    --disable-decimal-float --disable-threads --disable-libstdcxx-pch \
    --with-gmp=/usr --with-mpfr=/usr --with-mpc=/usr \
    --with-arch=rv64gc --with-abi=lp64d --with-boot-ldflags=-static \
    CC=/usr/gcc1/bin/gcc CFLAGS="-O1 -g0" LDFLAGS=-static \
    CFLAGS_FOR_TARGET="-O2 -g0" AR=ar RANLIB=ranlib MAKEINFO=true
grep -E '^(do-compare|STAGE1_CHECKING|POSTSTAGE1_LDFLAGS|LDFLAGS|LDFLAGS_FOR_BUILD|CFLAGS_FOR_TARGET) =' Makefile

# 4. make bootstrap in the foreground (as rung gcc1's make). The stage
# marks come from its log, read by a background watcher (the top-level
# Makefile's `Configuring stage N in DIR`: a stage's libgcc is configured
# once its gcc/ is built, stage N+1 starts once stage N's libgcc is built;
# `Comparing stages 2 and 3` once stage 3 is built). Nothing waits for the
# watcher with `wait`: the first run, with make in a background subshell,
# hung after make's error exit (README).
mk() {
    make -j1 MAKEINFO=true STAGE1_CFLAGS="-O1 -g0" BOOT_CFLAGS="-O1 -g0" "$@"
}
BLOG=${LOGS}/bootstrap.log
BDONE=${LOGS}/bootstrap.done
WDONE=${LOGS}/watch.done
: > "${BLOG}"
b_t0=$(date +%s)
(
    while IFS='|' read pat mark; do
        while ! grep -q "${pat}" "${BLOG}"; do
            if [ -f "${BDONE}" ]; then
                grep -q "${pat}" "${BLOG}" && break
                : > "${WDONE}"
                exit 0
            fi
            sleep 30
        done
        echo "gcc3 ${mark}: $(($(date +%s) - b_t0)) s"
        echo "${mark}"
    done << EOF
Configuring stage 1 in ${TARGET}/libgcc|stage1 cc1 built
Configuring stage 2 in |stage1 libgcc built
Configuring stage 2 in ${TARGET}/libgcc|stage2 cc1 built
Configuring stage 3 in |stage2 libgcc built
Configuring stage 3 in ${TARGET}/libgcc|stage3 cc1 built
Comparing stages 2 and 3|stage3 libgcc built
EOF
    : > "${WDONE}"
) < /dev/null &
if mk bootstrap > "${BLOG}" 2>&1; then
    b_st=0
else
    b_st=$?
fi
: > "${BDONE}"
# the watcher's last marks before ours (it polls every 30 s)
i=0
while [ ! -f "${WDONE}" ] && [ ${i} -lt 20 ]; do
    sleep 5
    i=$((i + 1))
done
test -f "${WDONE}" || echo "gcc3 watcher did not finish"
echo "gcc3 make bootstrap exit ${b_st}: $(($(date +%s) - b_t0)) s"
grep -B1 -A1 -E '^(Comparison successful|Bootstrap comparison failure)' "${BLOG}" || true
grep '^warning: .* differs' "${BLOG}" || true
# our own count of what `compare` compared (it counts only cmp's exit 1 as
# a difference, an error (2) passes): after `make bootstrap` stage 3 is
# current (gcc/, ...) and stage 2 is prev-*. cmp's exit codes are counted
# apart (run 5 counted every non-zero exit as "differ": README)
n=0
s=0
d=0
e=0
for f in $(find prev-* -name '*.o' 2>/dev/null); do
    n=$((n + 1))
    if cmp --ignore-initial=16 "${f}" "${f#prev-}" > /dev/null 2>&1; then
        st=0
    else
        st=$?
    fi
    case ${st} in
    0) s=$((s + 1)) ;;
    1) d=$((d + 1))
       echo "gcc3 recompare: ${f#prev-} differs" ;;
    *) e=$((e + 1))
       echo "gcc3 recompare: ${f#prev-} cmp exit ${st}"
       if [ ${e} -le 3 ]; then
           cmp --ignore-initial=16 "${f}" "${f#prev-}" 2>&1 | head -3 || true
       fi ;;
    esac
done
echo "gcc3 recompare: ${n} objects, ${s} same, ${d} differ, ${e} cmp errors"
if [ "${b_st}" = 0 ] && grep -q '^Comparison successful' "${BLOG}"; then
    tail -2 "${BLOG}"
    echo "bootstrap compare ok"
else
    echo "gcc3 bootstrap FAIL"
    if [ -f .bad_compare ]; then
        echo "--- .bad_compare"
        cat .bad_compare
    fi
    fail_tail "${BLOG}"
fi
ls -l gcc/xgcc gcc/cc1 "${TARGET}/libgcc/libgcc.a"

# 5. make install into /usr (gcc's default install-headers-tar through a
# pipe, fixed in Tilck since rung musl2); gcc-1 goes away
step install "gcc3 installed" mk install
rm -rf /usr/gcc1
ls -l /usr/bin/gcc /usr/bin/cpp "/usr/libexec/gcc/${TARGET}/4.6.4/cc1" \
    "/usr/lib/gcc/${TARGET}/4.6.4/libgcc.a"

# 6. tests (plan task 5), every program static (README, "Static linking")
mkdir -p /build/t
cd /build/t
gcc -v 2>&1 | tail -1
gcc -static -o hello /steps/tests/hello.c
./hello
sed 's/gcc1 sanity/gcc3 sanity/' /steps/tests/sanity.c > sanity.c
gcc -O2 -static -o sanity sanity.c
./sanity

# 7. /usr packed with mkustar into /dev/outbox (as rung gcc1)
cd /
filelist /build/usr.list usr
mkustar /build/rootfs.tar . /build/usr.list
catm /dev/outbox /build/rootfs.tar
sha256sum -o /build/rootfs.sum /build/rootfs.tar
read sum rest < /build/rootfs.sum
echo "rootfs sha256 ${sum}"
echo rung done gcc3
