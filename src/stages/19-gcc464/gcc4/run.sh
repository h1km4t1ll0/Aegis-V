# Rung gcc4 on Tilck (19-gcc464 README): gcc-3 (rung gcc3's /usr/bin/gcc)
# builds GCC 4.6.4 for C and C++ (--disable-bootstrap) into /usr: cc1plus,
# g++, libstdc++. Then g++ tests and the fixed-point check: gcc-3's cc1 and
# the new cc1 compile the same GCC sources to the same assembly. run.kaem
# runs this with /bin/sh (bash 2.05b) from / ; /initrd is read-only.
# rung-init's environment: PREFIX BINDIR LIBDIR INCDIR DISTFILES ARCH PATH.
# steps/sources, excludes, patches/ and tests/sanity.c are rung gcc1's;
# patches/riscv-init-array.patch is this rung's own; steps/src/mkustar.c is
# stage 13's (tilck.py).
set -e

export SHELL=/bin/sh CONFIG_SHELL=/bin/sh
unset CC CFLAGS LDFLAGS CPPFLAGS
TARGET=riscv64-unknown-linux-musl
GCC_TAR=gcc-4.6.4-c0d20242.tar
SRC=/build/gcc-src
OBJ=/build/gcc4
LOGS=/build/logs
FP=/build/fp
LIBEXEC=/usr/libexec/gcc/${TARGET}/4.6.4

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
        echo "gcc4 ${s_mark}: $(($(date +%s) - s_t0)) s"
        echo "${s_mark}"
    else
        echo "gcc4 $(basename "${s_log}" .log) FAIL"
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

# 2. gcc-3's cc1 for the fixed-point check (step 8): make install
# overwrites it
mkdir -p "${FP}/c3" "${FP}/c4"
cp "${LIBEXEC}/cc1" "${FP}/c3/cc1"
sha256sum -o /tmp/cc1.sum "${FP}/c3/cc1"
read sum rest < /tmp/cc1.sum
echo "gcc4 gcc-3 cc1 sha256 ${sum}"

# 3. the pinned source as in rung gcc1, with C++: gcc1's excludes without
# gcc/cp and libstdc++-v3, but with libstdc++-v3/testsuite. tar 1.12 does
# not read the ustar prefix field: the 125 members with a path over 100
# bytes come out at the top under their last name and are removed (as in
# rung gcc1); 7 of them are libstdc++-v3/include/ext/pb_ds/detail/
# headers, extracted again by ustar-x (src/ustar-x.c, built by gcc-3),
# with libstdc++-v3/testsuite/Makefile.in (libstdc++'s configure writes
# testsuite/Makefile).
cd /steps
checksum-transcriber sources
sha256sum -c sources.SHA256SUM
gcc -O2 -static -o /build/ustar-x /steps/src/ustar-x.c
# stage 13's mkustar with long names (ustar prefix) and more directories,
# for step 9 (src/mkustar-long.patch)
cp /steps/src/mkustar.c /build/mkustar.c
patch /build/mkustar.c < /steps/src/mkustar-long.patch
gcc -O2 -static -o /build/mkustar /build/mkustar.c
grep -v -e '^gcc/cp/\*$' -e '^libstdc++-v3/\*$' /steps/excludes > /build/excludes
echo 'libstdc++-v3/testsuite/*' >> /build/excludes
t0=$(date +%s)
mkdir -p "${SRC}"
cd "${SRC}"
tar xf "${DISTFILES}/${GCC_TAR}" -X /build/excludes
rm -rf explicit_instantiation *.cc *.class *.css *.h *.hpp *.html *.java
/build/ustar-x "${DISTFILES}/${GCC_TAR}" libstdc++-v3/include/ext/pb_ds/detail/ \
    libstdc++-v3/testsuite/Makefile.in
for h in cc_hash_table_map_/constructor_destructor_no_store_hash_fn_imps.hpp \
    cc_hash_table_map_/constructor_destructor_store_hash_fn_imps.hpp \
    gp_hash_table_map_/constructor_destructor_no_store_hash_fn_imps.hpp \
    gp_hash_table_map_/constructor_destructor_store_hash_fn_imps.hpp \
    left_child_next_sibling_heap_/constructors_destructor_fn_imps.hpp \
    left_child_next_sibling_heap_/left_child_next_sibling_heap_.hpp \
    resize_policy/cc_hash_max_collision_check_resize_trigger_imp.hpp; do
    if ! test -s "libstdc++-v3/include/ext/pb_ds/detail/${h}"; then
        echo "gcc4 source: missing ${h}"
        exit 1
    fi
done
for p in /steps/patches/*.patch; do
    echo "Applying patch: ${p}"
    patch -p1 < "${p}"
done
echo "gcc4 source: $(find . -type f | wc -l) files, $(du -sk . | cut -f1) KB, $(($(date +%s) - t0)) s"
echo "gcc4 source ok"

# tcc-built gmp, mpfr and mpc call tcc's runtime `alloca` (rung gcc3)
mkdir -p /build/tccrt
cd /build/tccrt
ar x /usr/lib/tcc/libtcc1.a alloca.o
export LIBS=/build/tccrt/alloca.o

# 4. configure (plan task 6): rung gcc1's line with --prefix=/usr, C and
# C++, gcc-3 as CC. Every link static: LDFLAGS (host and build programs),
# LDFLAGS_FOR_TARGET (the target libraries' configure tests).
mkdir -p "${OBJ}"
cd "${OBJ}"
step configure "gcc4 configured" "${CONFIG_SHELL}" "${SRC}/configure" --prefix=/usr \
    --build="${TARGET}" --host="${TARGET}" --target="${TARGET}" \
    --enable-languages=c,c++ --disable-bootstrap --disable-multilib \
    --disable-nls --disable-shared --disable-libmudflap --disable-libssp \
    --disable-libgomp --disable-libquadmath --disable-lto \
    --disable-decimal-float --disable-threads --disable-libstdcxx-pch \
    --with-gmp=/usr --with-mpfr=/usr --with-mpc=/usr \
    --with-arch=rv64gc --with-abi=lp64d \
    CC=gcc CFLAGS="-O2 -g0" LDFLAGS=-static \
    CFLAGS_FOR_TARGET="-O2 -g0" CXXFLAGS_FOR_TARGET="-O2 -g0" \
    LDFLAGS_FOR_TARGET=-static AR=ar RANLIB=ranlib MAKEINFO=true
grep -E '^(TARGET_CONFIGDIRS|CC|CFLAGS|LDFLAGS|CFLAGS_FOR_TARGET|CXXFLAGS_FOR_TARGET|LDFLAGS_FOR_TARGET) =' Makefile

# 5. make, one subsystem per step (as rung gcc1). gcc/configure drops -O
# from CFLAGS unless CC is a bootstrap's prev-gcc/xgcc ("for historical
# reasons, unless bootstrapping we prefer optimizations to be activated
# explicitly by the toplevel"): gcc/Makefile has `CFLAGS = -g0`, and runs 1
# and 2 built cc1, cc1plus and the generators at -O0. The host steps give
# CFLAGS on make's command line (it reaches gcc/'s make through MAKEFLAGS).
M="make -j1 MAKEINFO=true"
step support "gcc4 support built" ${M} CFLAGS="-O2 -g0" all-libiberty all-libcpp \
    all-libdecnumber maybe-all-build-libiberty maybe-all-build-fixincludes \
    maybe-all-zlib maybe-all-intl maybe-all-fixincludes configure-gcc
step cc1 "cc1 built" ${M} CFLAGS="-O2 -g0" -C gcc cc1
step cc1plus "cc1plus built" ${M} CFLAGS="-O2 -g0" -C gcc cc1plus
# the rest of gcc/: xgcc, g++, cpp, collect2, specs, fixincludes, gcov
step xgcc "xgcc built" ${M} CFLAGS="-O2 -g0" all-gcc
./gcc/xgcc -v 2>&1 | tail -1
# The target libraries with LIBS empty: alloca.o is for the host links
# (gcc/Makefile has it from configure); libtool would put an object from
# LIBS into libstdc++.a.
step libgcc "libgcc built" env LIBS= ${M} all-target-libgcc
# libstdc++-v3 (libsupc++, src, include), then whatever `all` has left
gcc4_libstdcxx() {
    env LIBS= ${M} all-target-libstdc++-v3 && env LIBS= ${M}
}
step libstdcxx "libstdc++ built" gcc4_libstdcxx
ls -l gcc/cc1 gcc/cc1plus gcc/xgcc gcc/g++ "${TARGET}/libgcc/libgcc.a" \
    "${TARGET}/libstdc++-v3/src/.libs/libstdc++.a" || true

# 6. make install into /usr (gcc's install-headers-tar through a pipe, as
# rung gcc3)
step install "gcc4 installed" ${M} install
ls -l /usr/bin/gcc /usr/bin/g++ /usr/bin/c++ "${LIBEXEC}/cc1" "${LIBEXEC}/cc1plus" \
    "/usr/lib/gcc/${TARGET}/4.6.4/libgcc.a" || true
find /usr/lib /usr/lib64 -name 'lib*c++*' 2> /dev/null | xargs ls -l || true
ls -d /usr/include/c++/4.6.4/iostream \
    /usr/include/c++/4.6.4/ext/pb_ds/detail/cc_hash_table_map_/constructor_destructor_store_hash_fn_imps.hpp || true

# 7. tests (plan task 6), every program static (README, "Static linking").
# A failed test does not stop the script: the fixed-point check and the
# packing still run (the host misses the test's mark and fails the rung).
mkdir -p /build/t
cd /build/t
echo "g++ -v: $(g++ -v 2>&1 | tail -1)"
if g++ -O2 -static -o hello /steps/tests/hello.cpp; then
    ./hello || {
        echo "gcc4 hello.cpp exit $?"
        readelf -S hello | grep -e '\.init' -e '\.fini' -e eh_frame || true
        nm hello | grep -e frame_dummy -e __register_frame_info || true
    }
else
    echo "gcc4 hello.cpp: g++ failed"
fi
sed 's/gcc1 sanity/gcc4 sanity/' /steps/tests/sanity.c > sanity.c
if gcc -O2 -static -o sanity sanity.c; then
    ./sanity || echo "gcc4 sanity exit $?"
else
    echo "gcc4 sanity.c: gcc failed"
fi

# 7b. layout test (rung gcc3's riscv-rtx-costs.patch): riscv_rtx_costs
# indexed mode_class[] and mode_size[] with an rtx code, so its costs came
# from whichever table followed in insn-modes.o's .rodata. gcc-4's cc1 is
# linked again with insn-modes.o built -fno-toplevel-reorder (the tables in
# source order instead of the reverse); both must give the same assembly.
mkdir -p /build/flip
cp "${LIBEXEC}/cc1" /build/flip/cc1-installed
cd "${OBJ}/gcc"
printf 'fp-flags:\n\t@echo $(ALL_CPPFLAGS) $(INTERNAL_CFLAGS) -DHAVE_CONFIG_H\n' > /build/flip/flags.mk
LTFLAGS=$(make -s -f Makefile -f /build/flip/flags.mk fp-flags)
grep -e ' -c .*-o insn-modes.o' "${LOGS}/cc1.log" | tail -1 | cut -c1-160 || true
rm -f insn-modes.o libbackend.a cc1
if ${M} CFLAGS="-O2 -g0 -fno-toplevel-reorder" insn-modes.o > "${LOGS}/flip.log" 2>&1 \
    && ${M} CFLAGS="-O2 -g0" cc1 >> "${LOGS}/flip.log" 2>&1; then
    mkdir -p /build/flip/a /build/flip/b
    cp /build/flip/cc1-installed /build/flip/a/cc1
    cp cc1 /build/flip/b/cc1
    for c in a b; do
        echo "layout $c: $(nm -n /build/flip/$c/cc1 | grep -n -e ' mode_class$' -e ' mode_fbit$' | tr '\n' ' ')"
    done
    lt_d=0
    for f in combine expr reload cse; do
        for c in a b; do
            gcc -B/build/flip/$c/ -S -O1 -g0 ${LTFLAGS} -o /build/flip/$c-$f.s "${SRC}/gcc/$f.c"
        done
        if cmp -s /build/flip/a-$f.s /build/flip/b-$f.s; then
            echo "layout $f same"
        else
            lt_d=$((lt_d + 1))
            echo "layout $f DIFFERS"
            diff /build/flip/a-$f.s /build/flip/b-$f.s | head -10 || true
        fi
    done
    if [ ${lt_d} = 0 ]; then
        echo "cc1 layout ok"
    fi
else
    echo "gcc4 layout test: relink failed"
    tail -20 "${LOGS}/flip.log"
fi

# 8. fixed point: gcc-3's cc1 (step 2) and gcc-4's cc1 (installed) compile
# the same GCC sources with the same driver (gcc-4's), the same flags
# (gcc/Makefile's ALL_CPPFLAGS, -DIN_GCC -DHAVE_CONFIG_H, in the configured
# gcc/ of this build) to assembly; -B puts each cc1 first. Same sha256 of
# the two .s files is "same". Each .s is also assembled and the two
# objects compared with `cmp --ignore-initial=16` as gcc's bootstrap
# `compare` does (rung gcc3's open question): its exit codes are counted.
cp "${LIBEXEC}/cc1" "${FP}/c4/cc1"
cd "${OBJ}/gcc"
printf 'fp-flags:\n\t@echo $(ALL_CPPFLAGS) $(INTERNAL_CFLAGS) -DHAVE_CONFIG_H\n' > "${FP}/flags.mk"
FPFLAGS=$(make -s -f Makefile -f "${FP}/flags.mk" fp-flags) || echo "fixedpoint: no flags from make"
echo "fixedpoint flags: ${FPFLAGS}"
gcc -B"${FP}/c3/" -print-prog-name=cc1
gcc -B"${FP}/c4/" -print-prog-name=cc1
FP_O1="fold-const combine tree-ssa-structalias expr dwarf2out builtins
 c-typeck tree c-decl reload1 c-parser gimplify reload sel-sched tree-vrp
 cse var-tracking optabs varasm tree-cfg tree-vect-stmts function
 tree-ssa-loop-ivopts tree-vect-loop simplify-rtx tree-inline gcse emit-rtl
 haifa-sched tree-data-ref calls rtlanal tree-ssa-pre df-problems df-scan
 cfgexpand sched-deps ira-color tree-ssa-sccvn recog ira stmt tree-ssa-dom
 ira-build alias cfgrtl tree-ssa-ccp cfgcleanup loop-iv ipa-inline
 postreload ggc-page tree-ssa-reassoc bitmap regrename
 config/riscv/riscv insn-recog insn-emit insn-attrtab"
FP_O2="fold-const combine tree-ssa-structalias expr reload1 cse tree-vrp
 ira-color simplify-rtx c-parser gimplify config/riscv/riscv"
fp_n=0
fp_d=0
fp_shown=0
fp_c0=0
fp_c1=0
fp_c2=0
# fp_one LEVEL NAME: NAME.c from gcc/ of the source tree, or generated here
fp_one() {
    f_src=${SRC}/gcc/$2.c
    test -f "${f_src}" || f_src=$2.c
    f_b=$1-$(basename "$2")
    f_t0=$(date +%s)
    for c in c3 c4; do
        if ! gcc -B"${FP}/${c}/" -S "-$1" -g0 ${FPFLAGS} -o "${FP}/${c}/${f_b}.s" \
            "${f_src}" > "${FP}/${c}/${f_b}.err" 2>&1; then
            echo "fixedpoint ${c} ${f_b}: compile failed"
            tail -5 "${FP}/${c}/${f_b}.err"
            : > "${FP}/${c}/${f_b}.s"
        fi
        gcc -c -o "${FP}/${c}/${f_b}.o" "${FP}/${c}/${f_b}.s" > /dev/null 2>&1 || true
        sha256sum -o /tmp/fp.sum "${FP}/${c}/${f_b}.s"
        read sum rest < /tmp/fp.sum
        eval "f_${c}=${sum}"
    done
    fp_n=$((fp_n + 1))
    if cmp --ignore-initial=16 "${FP}/c3/${f_b}.o" "${FP}/c4/${f_b}.o" > /dev/null 2>&1; then
        f_st=0
    else
        f_st=$?
    fi
    case ${f_st} in
    0) fp_c0=$((fp_c0 + 1)) ;;
    1) fp_c1=$((fp_c1 + 1)) ;;
    *) fp_c2=$((fp_c2 + 1)) ;;
    esac
    if [ "${f_c3}" = "${f_c4}" ] && [ -s "${FP}/c3/${f_b}.s" ]; then
        echo "fp ${f_b} same $(wc -l < "${FP}/c3/${f_b}.s") lines cmp ${f_st} $(($(date +%s) - f_t0)) s"
    else
        fp_d=$((fp_d + 1))
        echo "fp ${f_b} DIFFERS cmp ${f_st}"
        if [ ${fp_shown} -lt 3 ]; then
            fp_shown=$((fp_shown + 1))
            diff "${FP}/c3/${f_b}.s" "${FP}/c4/${f_b}.s" | head -20 || true
        fi
    fi
}
fp_t0=$(date +%s)
for f in ${FP_O1}; do
    fp_one O1 "${f}"
done
for f in ${FP_O2}; do
    fp_one O2 "${f}"
done
echo "gcc4 fixedpoint: $(($(date +%s) - fp_t0)) s"
echo "fixedpoint cmp --ignore-initial=16 of the objects: ${fp_c0} exit 0, ${fp_c1} exit 1, ${fp_c2} exit 2"
echo "fixedpoint ${fp_n} files, ${fp_d} differ"
if [ ${fp_d} = 0 ]; then
    echo "fixedpoint ok"
fi
# cmp itself: a copy of an object, another object, a copy with one byte
# changed past the first 16
cp fold-const.o "${FP}/a.o"
cp fold-const.o "${FP}/b.o"
printf 'Z' | dd of="${FP}/b.o" bs=1 seek=1000 conv=notrunc 2> /dev/null || true
for pair in "fold-const.o ${FP}/a.o" "fold-const.o combine.o" "fold-const.o ${FP}/b.o"; do
    if cmp --ignore-initial=16 ${pair} > /dev/null 2>&1; then
        st=0
    else
        st=$?
    fi
    echo "cmp probe ${pair}: exit ${st}"
done

# 9. /usr packed with step 3's mkustar into /dev/outbox (as rung gcc1)
cd /
filelist /build/usr.list usr
/build/mkustar /build/rootfs.tar . /build/usr.list
catm /dev/outbox /build/rootfs.tar
sha256sum -o /build/rootfs.sum /build/rootfs.tar
read sum rest < /build/rootfs.sum
echo "rootfs sha256 ${sum}"
echo rung done gcc4
