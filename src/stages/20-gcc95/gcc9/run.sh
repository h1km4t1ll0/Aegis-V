# Rung gcc9 on Tilck (20-gcc95 README): gcc9a (rung gcc9a's /usr/gcc9a)
# runs GCC 9.5.0's `make bootstrap` for C and C++ (stage1 built by gcc9a,
# stage2 by stage1, stage3 by stage2, stage2 and stage3 objects compared),
# installs it into /usr, checks the fixed point (gcc9a's and gcc9's cc1 and
# cc1plus compile the same sources to the same assembly), removes gcc9a and
# gcc 4.6.4, and tests the installed gcc and g++. run.kaem runs this with
# /bin/sh (bash 2.05b) from / ; /initrd is read-only.
# rung-init's environment: PREFIX BINDIR LIBDIR INCDIR DISTFILES ARCH PATH.
# steps/tests/ are rung gcc9a's; steps/src/paxx.c is
# 20-gcc95/src/paxx.c; steps/src/mkustar.c is stage 13's,
# steps/src/mkustar-long.patch rung gcc4's (tilck.py gcc9_step_files()).
set -e

export SHELL=/bin/sh CONFIG_SHELL=/bin/sh
unset CC CXX CFLAGS CXXFLAGS LDFLAGS CPPFLAGS LIBS
TARGET=riscv64-unknown-linux-musl
SRC=/build/gcc-src
OBJ=/build/gcc9
LOGS=/build/logs
FP=/build/fp
A=/usr/gcc9a
LIBEXEC_A=${A}/libexec/gcc/${TARGET}/9.5.0
LIBEXEC=/usr/libexec/gcc/${TARGET}/9.5.0
GCCLIB=/usr/lib/gcc/${TARGET}/9.5.0
OPT="-O2 -g0"
# the plan's stage flags: STAGE1_CFLAGS for stage 1 (built by gcc9a),
# BOOT_CFLAGS for stages 2 and 3; with C++ bootstrapped STAGEn_CXXFLAGS are
# STAGEn_CFLAGS (Makefile.tpl). Every make of the tree gets them, so
# nothing is rebuilt with the default `-g -O2`.
S1FLAGS="-O1 -g0"
BFLAGS="-O2 -g0"
mk() {
    make -j1 MAKEINFO=true STAGE1_CFLAGS="${S1FLAGS}" BOOT_CFLAGS="${BFLAGS}" "$@"
}

# 1. steps to /steps (checksum-transcriber writes sources.SHA256SUM there)
mkdir -p /steps "${LOGS}" /tmp
cp -R /initrd/steps/. /steps/

# watched LOG CMD...: as rung gcc9a's (heartbeat every 300 s of guest
# time; CMD's exit status in w_rc)
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
            echo "gcc9 heartbeat $(basename "${w_log}" .log): ${w_dt} s, $(wc -l < "${w_log}") lines, last: $(tail -1 "${w_log}" | cut -c1-100)"
        fi
    done
    wait
    read w_rc < /tmp/watched.rc
}

# fail_tail LOG: the log's last 60 lines, the newest config.log's last 40
# before its variable dump; exit 1
fail_tail() {
    tail -60 "$1"
    f_cl=$(ls -t config.log */config.log */*/config.log 2>/dev/null | head -1)
    if [ -n "${f_cl}" ]; then
        echo "--- ${f_cl}"
        sed -n '/^## Cache variables/q;p' "${f_cl}" | tail -40
    fi
    exit 1
}

# step LOG MARK CMD...: as rung gcc9a's (`gcc9 time LOG: N s`, MARK; or
# `gcc9 LOG FAIL` and the tails, exit 1)
step() {
    s_log=${LOGS}/$1.log
    s_mark=$2
    shift 2
    s_t0=$(date +%s)
    watched "${s_log}" "$@"
    if [ "${w_rc}" = 0 ]; then
        tail -2 "${s_log}"
        echo "gcc9 time $(basename "${s_log}" .log): $(($(date +%s) - s_t0)) s"
        echo "${s_mark}"
    else
        echo "gcc9 $(basename "${s_log}" .log) FAIL"
        fail_tail "${s_log}"
    fi
}

# 2. the pinned source; gcc9a's version; paxx and stage 13's mkustar with
# rung gcc4's mkustar-long.patch (for step 12), both by gcc9a
cd /steps
checksum-transcriber sources
sha256sum -c sources.SHA256SUM
echo "gcc9 gcc9a -v: $(${A}/bin/gcc -v 2>&1 | tail -1)"
echo "gcc9 gcc -v (4.6.4): $(gcc -v 2>&1 | tail -1)"
# gcc 4.6.4's libstdc++ and libsupc++ out of /usr/lib before the build
# (README, run 1): cc1's link line carries -L/usr/lib (--with-gmp=/usr
# and friends) ahead of the driver's own directories, so gcc9a's g++ took
# 4.6.4's /usr/lib/libstdc++.a for its -lstdc++ (no std::__cxx11). gcc9a's
# are in ${A}/lib/gcc/TARGET/9.5.0; the rest of 4.6.4 goes in step 9.
rm -f /usr/lib/libstdc++.a /usr/lib/libstdc++.la /usr/lib/libstdc++.a-gdb.py \
    /usr/lib/libsupc++.a /usr/lib/libsupc++.la
${A}/bin/g++ -static -O2 -L/usr/lib -o /build/hello-L /steps/tests/hello.cpp
/build/hello-L > /build/hello-L.out
if grep -q -x 'hello from g++ 9\.5\.0' /build/hello-L.out; then
    echo "gcc9 gcc9a g++ -L/usr/lib ok"
else
    cat /build/hello-L.out
    echo "gcc9 FAIL gcc9a g++ -L/usr/lib"
    exit 1
fi
${A}/bin/gcc -static ${OPT} -Wall -o /build/paxx /steps/src/paxx.c
cp /steps/src/mkustar.c /build/mkustar.c
patch /build/mkustar.c < /steps/src/mkustar-long.patch
${A}/bin/gcc -static -O2 -o /build/mkustar /build/mkustar.c

# gcc9a's cc1 and cc1plus for the fixed point (step 8), before anything
# else touches /usr
mkdir -p "${FP}/a" "${FP}/b"
cp "${LIBEXEC_A}/cc1" "${LIBEXEC_A}/cc1plus" "${FP}/a/"
for f in cc1 cc1plus; do
    sha256sum -o /tmp/fp.sum "${FP}/a/${f}"
    read sum rest < /tmp/fp.sum
    echo "gcc9 gcc9a ${f}: $(wc -c < "${FP}/a/${f}") bytes, sha256 ${sum}"
done

# 3. gcc-9.5.0.tar.gz through paxx into /build/gcc-src with excludes:
# rung gcc9a's without gcc/fortran (README, rung gcc9 "Source tree");
# patches/*.patch, if any
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
echo "gcc9 source: $(find . -type f | wc -l) files, $(du -sk . | cut -f1) KB, $(($(date +%s) - t0)) s"
echo "gcc9 source ok"

# 4. configure (plan task 6): rung gcc9a's line with --prefix=/usr and
# without --disable-bootstrap, gcc9a as CC and CXX. Every host and build
# program static: --with-stage1-ldflags (stage 1; its default is
# -static-libstdc++ -static-libgcc) and LDFLAGS (LDFLAGS_FOR_BUILD, the
# modules built once), --with-boot-ldflags (stages 2 and 3, whose LDFLAGS
# are POSTSTAGE1_LDFLAGS alone; its default -static-libstdc++ -static-libgcc
# links dynamically). --enable-version-specific-runtime-libs as rung gcc9a:
# 9.5's libstdc++, libsupc++, libatomic and C++ headers go to
# /usr/lib/gcc/TARGET/9.5.0, apart from gcc 4.6.4's /usr/lib/libstdc++.a.
# No --with-build-config: the default is bootstrap-debug (README).
mkdir -p "${OBJ}"
cd "${OBJ}"
step configure "gcc9 configured" "${CONFIG_SHELL}" "${SRC}/configure" --prefix=/usr \
    --build="${TARGET}" --host="${TARGET}" --target="${TARGET}" \
    --enable-languages=c,c++ --disable-multilib --disable-nls \
    --disable-shared --enable-static --disable-lto --without-isl \
    --disable-libsanitizer --disable-libssp --disable-libgomp --disable-libitm \
    --disable-libquadmath --disable-libvtv --disable-symvers --disable-libstdcxx-pch \
    --enable-threads=posix --disable-sjlj-exceptions \
    --with-gmp=/usr --with-mpfr=/usr --with-mpc=/usr \
    --with-arch=rv64gc --with-abi=lp64d \
    --with-native-system-header-dir=/usr/include --enable-version-specific-runtime-libs \
    --with-stage1-ldflags=-static --with-boot-ldflags=-static \
    CC="${A}/bin/gcc" CXX="${A}/bin/g++" CFLAGS="${OPT}" CXXFLAGS="${OPT}" LDFLAGS=-static \
    CFLAGS_FOR_TARGET="${OPT}" CXXFLAGS_FOR_TARGET="${OPT}" LDFLAGS_FOR_TARGET=-static \
    AR=ar RANLIB=ranlib MAKEINFO=true
grep -e 'default BUILD_CONFIG' -e 'accepts -static-libstdc++' "${LOGS}/configure.log" || true
grep -E '^(configdirs|TARGET_CONFIGDIRS|CC|CXX|do-compare|compare_exclusions) =' Makefile || true
echo "gcc9 bootstrapped: $(grep '^all-stage1: ' Makefile | sed 's/^all-stage1: all-stage1-//' | tr '\n' ' ')"
mk -s -f Makefile -f /steps/src/print.mk gcc9-top || echo "gcc9 print.mk gcc9-top failed"

# 5. make bootstrap in the foreground (as rung gcc3), its log read by a
# background watcher: events from the log, in order, `gcc9 EVENT: N s`
# (guest seconds since the start of make); `m` events are marks, also
# printed alone. An `i` event is the K-th line holding the text; an `m`
# event needs a line after that one (the command finished: make -j1 prints
# the next command after it), and not a linker error. The link lines' last lines hold
# `cc1-checksum.o libbackend.a` / `cc1plus-checksum.o libbackend.a`;
# libtool's last command for libstdc++.la is its `ln -s` in .libs. One per
# stage. Every 300 s a heartbeat (stage_current, the log's size and last
# line), every 3600 s the size of the tree.
BLOG=${LOGS}/bootstrap.log
BDONE=${LOGS}/bootstrap.done
WDONE=${LOGS}/watch.done
EVF=/build/events
: > "${EVF}"
for n in 1 2 3; do
    cat >> "${EVF}" << EOF
i|1|Configuring stage ${n} in |stage${n} start
i|1|Configuring stage ${n} in ./gcc|stage${n} gcc configure
m|${n}|cc1-checksum.o libbackend.a|stage${n} cc1 built
m|${n}|cc1plus-checksum.o libbackend.a|stage${n} cc1plus built
i|1|Configuring stage ${n} in ${TARGET}/libgcc|stage${n} libgcc configure
i|1|Configuring stage ${n} in ${TARGET}/libstdc++-v3|stage${n} libstdc++ configure
m|${n}|"../libstdc++.la" "libstdc++.la"|stage${n} libstdc++ built
EOF
done
echo 'i|1|Comparing stages 2 and 3|compare start' >> "${EVF}"
: > "${BLOG}"
b_t0=$(date +%s)
# ev_seen KIND K TEXT: the K-th line of the log holding TEXT is there (and,
# for KIND m, a line after it)
ev_seen() {
    e_l=$(grep -n -F -e "$3" "${BLOG}" | sed -n "$2p" | cut -d: -f1)
    test -n "${e_l}" || return 1
    test "$1" = i && return 0
    test $(($(wc -l < "${BLOG}") + 0)) -gt "${e_l}" || return 1
    # and that next line is not the linker's error (run 1's stage-1 cc1)
    ! sed -n "$((e_l + 1))p" "${BLOG}" | grep -q -i -e error -e 'undefined reference' -e 'In function'
}
(
    hb_next=300
    du_next=3600
    while IFS='|' read -r e_kind e_n e_pat e_name; do
        while :; do
            e_done=0
            test -f "${BDONE}" && e_done=1
            if ev_seen "${e_kind}" "${e_n}" "${e_pat}"; then
                echo "gcc9 ${e_name}: $(($(date +%s) - b_t0)) s"
                test "${e_kind}" = m && echo "${e_name}"
                break
            fi
            if [ ${e_done} = 1 ]; then
                echo "gcc9 watcher: no ${e_name}"
                break
            fi
            h_dt=$(($(date +%s) - b_t0))
            if [ ${h_dt} -ge ${hb_next} ]; then
                hb_next=$((h_dt + 300))
                echo "gcc9 heartbeat: ${h_dt} s, $(cat "${OBJ}/stage_current" 2> /dev/null || echo none), $(($(wc -l < "${BLOG}") + 0)) lines, last: $(tail -1 "${BLOG}" | cut -c1-100)"
                if [ ${h_dt} -ge ${du_next} ]; then
                    du_next=$((h_dt + 3600))
                    echo "gcc9 du: ${OBJ} $(du -sk "${OBJ}" | cut -f1) KB"
                fi
            fi
            sleep 60
        done
    done < "${EVF}"
    : > "${WDONE}"
) < /dev/null &
if mk bootstrap > "${BLOG}" 2>&1; then
    b_st=0
else
    b_st=$?
fi
: > "${BDONE}"
i=0
while [ ! -f "${WDONE}" ] && [ ${i} -lt 120 ]; do
    sleep 5
    i=$((i + 1))
done
test -f "${WDONE}" || echo "gcc9 watcher did not finish"
echo "gcc9 make bootstrap exit ${b_st}: $(($(date +%s) - b_t0)) s"
grep -e '^Comparison successful' -e '^Bootstrap comparison failure' -e '^warning: .* differs' "${BLOG}" || true
if [ "${b_st}" = 0 ] && grep -q '^Comparison successful' "${BLOG}"; then
    tail -2 "${BLOG}"
    echo "bootstrap compare ok"
else
    echo "gcc9 bootstrap FAIL"
    if [ -f .bad_compare ]; then
        echo "--- .bad_compare ($(wc -l < .bad_compare) lines)"
        head -40 .bad_compare
    fi
    fail_tail "${BLOG}"
fi
echo "gcc9 stage_current: $(cat stage_current 2> /dev/null || echo none)"
ls -l gcc/xgcc gcc/xg++ gcc/cc1 gcc/cc1plus prev-gcc/cc1 prev-gcc/cc1plus stage1-gcc/cc1 \
    stage1-gcc/cc1plus "${TARGET}/libgcc/libgcc.a" "${TARGET}/libstdc++-v3/src/.libs/libstdc++.a" || true
du -sk stage1-* prev-* * 2> /dev/null | sort -n | tail -24
echo "gcc9 du: ${OBJ} $(du -sk "${OBJ}" | cut -f1) KB, ${SRC} $(du -sk "${SRC}" | cut -f1) KB"
# stage 1 is not needed any more (Tilck's / is in RAM)
rm -rf stage1-*

# 6. What compare compared and what a recount sees (plan task 6, rung
# gcc3's "465 differ"). After `make bootstrap` stage 3 is current (gcc/,
# TARGET/, ...) and stage 2 is prev-*; gcc's compare ran on the same files
# as stage2-* and stage3-* (it ends the stage first). Each pair: cmp's exit
# (configure's do-compare, rung gcc3's recount) and contrib/compare-debug's
# (bootstrap-debug's do-compare), counted per directory.
DOCMP=$(sed -n 's/^do-compare = //p' Makefile)
echo "gcc9 configure's do-compare: ${DOCMP}"
cmp_st() {
    if cmp --ignore-initial=16 "$1" "$2" > /dev/null 2>&1; then
        c_st=0
    else
        c_st=$?
    fi
    if sh "${SRC}/contrib/compare-debug" "$1" "$2" > /dev/null 2>&1; then
        d_st=0
    else
        d_st=$?
    fi
}
find prev-* -name '*.o' | sort > /build/recount.list
: > /build/recount.tab
r_t0=$(date +%s)
while read f; do
    g=${f#prev-}
    cmp_st "${f}" "${g}"
    case ${g} in
    ${TARGET}/*) k=$(echo "${g}" | cut -d/ -f1-2) ;;
    *) k=$(echo "${g}" | cut -d/ -f1) ;;
    esac
    echo "${k} ${c_st} ${d_st} ${g}" >> /build/recount.tab
done < /build/recount.list
echo "gcc9 recount: $(($(date +%s) - r_t0)) s"
awk '{ n[$1]++; c[$1 " " $2]++; d[$1 " " $3]++; N++; C[$2]++; D[$3]++ }
END {
    for (k in n)
        printf "gcc9 recount %s: %d objects; cmp 0/1/2: %d/%d/%d; compare-debug 0/1: %d/%d\n",
            k, n[k], c[k " 0"], c[k " 1"], c[k " 2"], d[k " 0"], d[k " 1"]
    printf "gcc9 recount: %d objects; cmp --ignore-initial=16: %d same, %d differ, %d cmp errors; compare-debug: %d same, %d differ\n",
        N, C[0], C[1], C[2], D[0], D[1]
}' /build/recount.tab | sort
awk '$3 != 0 { print "gcc9 recount compare-debug differs: " $4 }' /build/recount.tab
# three pairs in detail: a gcc/ object, a host library's, a target
# library's (libgcc, built with CFLAGS_FOR_TARGET only)
for g in gcc/tree.o libiberty/hashtab.o "${TARGET}/libgcc/_muldi3.o" \
    "${TARGET}/libstdc++-v3/src/c++11/string-inst.o"; do
    if [ ! -f "${g}" ] || [ ! -f "prev-${g}" ]; then
        echo "gcc9 pair ${g}: missing"
        continue
    fi
    echo "gcc9 pair ${g}: compare's f1 f2 were \$r/stage2-${g} \$r/stage3-${g}; the recount's prev-${g} ${g} (the same files, renamed by stage3-start)"
    for x in "prev-${g}" "${g}"; do
        echo "gcc9 pair   ${x}: $(wc -c < "${x}") bytes, $(readelf -S "${x}" | grep -c ' \.debug_') .debug_* sections, $(readelf -S "${x}" | grep -c '\] \.') sections"
    done
    cmp_st "prev-${g}" "${g}"
    echo "gcc9 pair   cmp --ignore-initial=16 exit ${c_st}, compare-debug exit ${d_st}; cmp -l | head -4:"
    cmp -l "prev-${g}" "${g}" 2>&1 | head -4 || true
    # the object's compile command per stage (log order: 1, 2, 3): -O, -g, checking
    b=$(basename "${g}")
    grep -F -e "-o ${b}" "${BLOG}" | while read -r l; do
        echo "gcc9 pair   compiled with: $(echo "${l}" | tr ' ' '\n' | grep -e '^-O' -e '^-g' -e 'checking' | tr '\n' ' ')"
    done
done
# gcc's own compare loop again, with configure's do-compare (rung gcc3's
# cmp) in place of bootstrap-debug's: it must flag what the recount's cmp
# flags; then with its own do-compare: `Comparison successful` again
# (`compare` is a stamp file: removed, or make finds it up to date)
rm -f compare .bad_compare
if mk compare "do-compare=${DOCMP}" > "${LOGS}/compare-cmp.log" 2>&1; then
    echo "gcc9 compare with cmp: exit 0"
else
    echo "gcc9 compare with cmp: exit $?"
fi
grep -e '^Comparing' -e '^Comparison' -e '^Bootstrap' -e '^warning' "${LOGS}/compare-cmp.log" || true
if [ -f .bad_compare ]; then
    echo "gcc9 compare with cmp: $(wc -l < .bad_compare) differ"
    sed 's/ differs$//' .bad_compare | awk -F/ '{ k = $1; if (k ~ /^'"${TARGET}"'$/) k = k "/" $2; n[k]++ } END { for (k in n) print "gcc9 compare with cmp " k ": " n[k] }' | sort
    rm -f .bad_compare
fi
rm -f compare
if mk compare > "${LOGS}/compare-again.log" 2>&1; then
    echo "gcc9 compare again: exit 0"
else
    echo "gcc9 compare again: exit $?"
fi
grep -e '^Comparing' -e '^Comparison' -e '^Bootstrap' -e '^warning' "${LOGS}/compare-again.log" || true
mk stage3-start > "${LOGS}/stage3-start.log" 2>&1 || true
echo "gcc9 stage_current: $(cat stage_current 2> /dev/null || echo none)"

# 7. make install into /usr (plan task 7)
step install "gcc9 installed" mk install
echo "gcc9 install log: $(grep -c -e ' -c ' "${LOGS}/install.log" || true) compile lines"
ls -l /usr/bin/gcc /usr/bin/g++ /usr/bin/c++ /usr/bin/cpp "${LIBEXEC}/cc1" "${LIBEXEC}/cc1plus" \
    "${LIBEXEC}/collect2" "${GCCLIB}/libgcc.a" "${GCCLIB}/libstdc++.a" || true
find "${GCCLIB}" -name '*.a' | xargs ls -l || true
for f in "${LIBEXEC}/cc1" "${LIBEXEC}/cc1plus" "${LIBEXEC}/collect2" "${LIBEXEC}/lto-wrapper" \
    /usr/bin/gcc /usr/bin/g++ /usr/bin/cpp /usr/bin/gcov; do
    echo "gcc9 $(basename "${f}"): $(readelf -l "${f}" | grep -c INTERP || true) INTERP"
done
for f in cc1 cc1plus; do
    if cmp -s "${LIBEXEC}/${f}" "gcc/${f}"; then
        echo "gcc9 installed ${f} = stage 3's gcc/${f}"
    else
        echo "gcc9 installed ${f} differs from gcc/${f}"
    fi
done

# 8. fixed point (plan task 7): gcc9a's cc1/cc1plus (step 2) and gcc9's
# (installed) compile the same sources with the same driver (the installed
# gcc/g++, -B puts each pair first) and the same flags to assembly; the
# same .s is "same". gcc/'s own sources are C++ (g++ -> cc1plus) with
# gcc/Makefile's flags; libgcc2.c (gcc -> cc1) with libgcc's; a few
# libstdc++ sources (cc1plus) with libstdc++'s.
cp "${LIBEXEC}/cc1" "${LIBEXEC}/cc1plus" "${FP}/b/"
for c in a b; do
    echo "gcc9 fixedpoint ${c}: $(g++ -B"${FP}/${c}/" -print-prog-name=cc1plus) $(gcc -B"${FP}/${c}/" -print-prog-name=cc1)"
done
fp_n=0
fp_d=0
fp_f=0
fp_shown=0
# fp_one LEVEL DRIVER SOURCE TAG FLAGS...: in the current directory
fp_one() {
    f_lvl=$1
    f_drv=$2
    f_src=$3
    f_tag=$4
    shift 4
    f_t0=$(date +%s)
    f_ok=1
    for c in a b; do
        if ! ${f_drv} -B"${FP}/${c}/" -S "$@" "-${f_lvl}" -g0 -o "${FP}/${c}/${f_tag}.s" \
            "${f_src}" > "${FP}/${c}/${f_tag}.err" 2>&1; then
            echo "fixedpoint ${c} ${f_tag}: compile failed"
            tail -5 "${FP}/${c}/${f_tag}.err"
            f_ok=0
        fi
    done
    fp_n=$((fp_n + 1))
    if [ ${f_ok} = 0 ]; then
        fp_f=$((fp_f + 1))
    elif cmp -s "${FP}/a/${f_tag}.s" "${FP}/b/${f_tag}.s"; then
        echo "fp ${f_tag} same $(($(wc -l < "${FP}/a/${f_tag}.s") + 0)) lines $(($(date +%s) - f_t0)) s"
    else
        fp_d=$((fp_d + 1))
        echo "fp ${f_tag} DIFFERS"
        if [ ${fp_shown} -lt 3 ]; then
            fp_shown=$((fp_shown + 1))
            diff "${FP}/a/${f_tag}.s" "${FP}/b/${f_tag}.s" | head -20 || true
        fi
    fi
    rm -f "${FP}/a/${f_tag}.s" "${FP}/b/${f_tag}.s"
}
# fp_gcc LEVEL FILE...: gcc/ sources (source tree, or generated in gcc/)
fp_gcc() {
    g_lvl=$1
    shift
    for f in "$@"; do
        g_src=${SRC}/gcc/${f}
        test -f "${g_src}" || g_src=${f}
        g_tag=${f##*/}
        case ${f} in
        c/* | cp/* | c-family/*) g_fe=-DIN_GCC_FRONTEND ;;
        *) g_fe= ;;
        esac
        fp_one "${g_lvl}" g++ "${g_src}" "${g_lvl}-${g_tag%.*}" ${FPFLAGS} ${g_fe}
    done
}
cd "${OBJ}/gcc"
FPFLAGS=$(make -s -f Makefile -f /steps/src/print.mk gcc9-fp-gcc CXXFLAGS=) || echo "gcc9 fixedpoint: no gcc/ flags"
echo "gcc9 fixedpoint gcc/ flags: ${FPFLAGS}"
fp_t0=$(date +%s)
fp_gcc O1 fold-const.c combine.c tree-ssa-structalias.c expr.c dwarf2out.c builtins.c \
    tree.c reload1.c gimplify.c reload.c sel-sched.c tree-vrp.c cse.c var-tracking.c \
    optabs.c varasm.c tree-cfg.c tree-vect-stmts.c function.c tree-ssa-loop-ivopts.c \
    tree-vect-loop.c simplify-rtx.c tree-inline.c gcse.c emit-rtl.c haifa-sched.c \
    tree-data-ref.c calls.c rtlanal.c tree-ssa-pre.c df-problems.c df-scan.c cfgexpand.c \
    sched-deps.c ira-color.c tree-ssa-sccvn.c recog.c ira.c tree-ssa-dom.c ira-build.c \
    alias.c cfgrtl.c tree-ssa-ccp.c cfgcleanup.c loop-iv.c ipa-inline.c postreload.c \
    ggc-page.c tree-ssa-reassoc.c bitmap.c regrename.c lra-constraints.c lra-assigns.c \
    ipa-cp.c gimple-ssa-strength-reduction.c wide-int.cc config/riscv/riscv.c \
    insn-recog.c insn-emit.c insn-attrtab.c gimple-match.c c/c-typeck.c c/c-decl.c \
    c/c-parser.c c-family/c-common.c cp/parser.c cp/pt.c
fp_gcc O2 fold-const.c combine.c tree-ssa-structalias.c expr.c reload1.c cse.c tree-vrp.c \
    ira-color.c simplify-rtx.c gimplify.c lra-constraints.c config/riscv/riscv.c \
    c/c-parser.c cp/pt.c
echo "gcc9 fixedpoint gcc/: $(($(date +%s) - fp_t0)) s"
cd "${OBJ}/${TARGET}/libgcc"
LGFLAGS=$(make -s -f Makefile -f /steps/src/print.mk gcc9-fp-libgcc) || echo "gcc9 fixedpoint: no libgcc flags"
echo "gcc9 fixedpoint libgcc flags: ${LGFLAGS}"
for l in _muldi3 _divdi3 _moddi3 _udivdi3 _umoddi3 _udivmoddi4 _clzdi2 _popcountdi2 \
    _powidf2 _muldc3 _divdc3 _fixunsdfdi _floatundidf; do
    fp_one O2 gcc "${SRC}/libgcc/libgcc2.c" "O2-libgcc2${l}" ${LGFLAGS} "-DL${l}"
done
cd "${OBJ}/${TARGET}/libstdc++-v3/src/c++11"
LSFLAGS=$(make -s -f Makefile -f /steps/src/print.mk gcc9-fp-libstdcxx) || echo "gcc9 fixedpoint: no libstdc++ flags"
echo "gcc9 fixedpoint libstdc++ flags: ${LSFLAGS}"
for f in string-inst.cc cow-string-inst.cc locale-inst.cc random.cc hashtable_c++0x.cc \
    functexcept.cc; do
    fp_one O2 g++ "${SRC}/libstdc++-v3/src/c++11/${f}" "O2-${f%.cc}" -nostdinc++ ${LSFLAGS}
done
echo "gcc9 fixedpoint: $(($(date +%s) - fp_t0)) s"
echo "fixedpoint ${fp_n} files, ${fp_d} differ, ${fp_f} failed"
bad=0
if [ ${fp_d} = 0 ] && [ ${fp_f} = 0 ]; then
    echo "fixedpoint ok"
else
    bad=$((bad + 1))
fi

# 9. gcc 4.6.4 and gcc9a out of /usr (plan task 7). 9.5's install wrote
# /usr/bin/gcc, g++, c++, cpp, gcov and the riscv64-unknown-linux-musl-*
# names over 4.6.4's; what 4.6.4 left: its lib/gcc and libexec/gcc
# directories, its C++ headers, python printers and the versioned driver
# name (its libstdc++/libsupc++ in /usr/lib went in step 2; 9.5's are in
# GCCLIB).
cd /
if [ ! -f "${GCCLIB}/libstdc++.a" ]; then
    echo "gcc9 FAIL no ${GCCLIB}/libstdc++.a"
    exit 1
fi
ls -l /usr/lib/libstdc++* /usr/lib/libsupc++* 2> /dev/null || true
rm -rf "/usr/lib/gcc/${TARGET}/4.6.4" "/usr/libexec/gcc/${TARGET}/4.6.4" /usr/include/c++/4.6.4 \
    /usr/share/gcc-4.6.4
rm -f /usr/lib/libstdc++.a /usr/lib/libstdc++.la /usr/lib/libstdc++.a-gdb.py \
    /usr/lib/libsupc++.a /usr/lib/libsupc++.la "/usr/bin/${TARGET}-gcc-4.6.4"
rm -rf "${A}"
echo "gcc9 left of 4.6.4 and gcc9a: $(find /usr -name '*4.6.4*' | wc -l) names, $(find /usr -name '*gcc9a*' | wc -l) names"
find /usr -name '*4.6.4*' | head -5
ls /usr/lib/gcc/${TARGET} /usr/libexec/gcc/${TARGET} /usr/include/c++ 2> /dev/null || true
ls -l /usr/bin/cc 2> /dev/null || echo "gcc9 no /usr/bin/cc"

# 10. tests (plan task 7) by /usr/bin/gcc and g++, every program static
# (19-gcc464 README, "Static linking"); rung gcc9a's tests. Each test runs;
# any failure (or the fixed point's) stops the script before the packing.
mkdir -p /build/t
cd /build/t
t_fail() {
    echo "gcc9 test FAIL $*"
    bad=$((bad + 1))
}
for p in gcc g++ c++ cpp gcov "${TARGET}-gcc" "${TARGET}-g++" "${TARGET}-c++" "${TARGET}-gcc-9.5.0"; do
    echo "gcc9 ${p} --version: $(/usr/bin/${p} --version 2>&1 | head -1)"
    /usr/bin/${p} --version 2>&1 | head -1 | grep -q ' 9\.5\.0$' || t_fail "${p} --version"
done
echo "gcc9 gcc -v: $(gcc -v 2>&1 | tail -1)"
gcc -v 2>&1 | grep -q '^gcc version 9\.5\.0 ' || t_fail "gcc -v"
for f in cc1 cc1plus collect2; do
    t_f=$(g++ -print-prog-name=${f})
    echo "gcc9 g++ -print-prog-name=${f}: ${t_f}"
    test "${t_f}" = "${LIBEXEC}/${f}" || t_fail "${f} is ${t_f}"
done
for f in libstdc++.a libsupc++.a libatomic.a libgcc.a libc.a crt1.o crtbeginT.o; do
    t_f=$(g++ -print-file-name=${f})
    echo "gcc9 g++ -print-file-name=${f}: ${t_f}"
    case ${f}:${t_f} in
    libc.a:/usr/lib/libc.a | crt1.o:/usr/lib/crt1.o) ;;
    libc.a:* | crt1.o:*) t_fail "${f} is ${t_f}" ;;
    *:${GCCLIB}/${f}) ;;
    *) t_fail "${f} is ${t_f}" ;;
    esac
done
# t_run NAME EXPECT: ./NAME must exit 0 and print the line EXPECT (grep -x)
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
for f in sanity.c sanity.cpp; do
    sed 's/gcc9a sanity/gcc9 sanity/' "/steps/tests/${f}" > "${f}"
done
if gcc -static -o hello /steps/tests/hello.c; then
    t_run hello 'hello from gcc 9\.5\.0'
else
    t_fail "hello.c: gcc failed"
fi
if g++ -static -O2 -o hello-cpp /steps/tests/hello.cpp; then
    t_run hello-cpp 'hello from g++ 9\.5\.0'
else
    t_fail "hello.cpp: g++ failed"
fi
san_bad=${bad}
if gcc -static -O2 -Wall -o sanity sanity.c; then
    t_run sanity 'gcc9 sanity c ok'
else
    t_fail "sanity.c: gcc failed"
fi
if g++ -static -O2 -Wall -o sanity-cpp sanity.cpp; then
    t_run sanity-cpp 'gcc9 sanity c++ ok'
else
    t_fail "sanity.cpp: g++ failed"
fi
for f in hello hello-cpp sanity sanity-cpp; do
    if [ -f ${f} ]; then
        echo "gcc9 test ${f}: $(wc -c < ${f}) bytes, $(readelf -l ${f} | grep -c INTERP || true) INTERP"
        if readelf -l ${f} | grep -q INTERP; then
            t_fail "${f}: dynamic"
        fi
    fi
done
if [ ${bad} = ${san_bad} ]; then
    echo "gcc9 sanity ok"
fi
if [ ${bad} != 0 ]; then
    echo "gcc9 FAIL ${bad}"
    exit 1
fi
echo "gcc9 /usr: $(du -sk /usr | cut -f1) KB"

# 11. the build tree out of RAM before packing (Tilck's / is in memory)
cd /
rm -rf "${OBJ}" "${SRC}" "${FP}"

# 12. /usr packed with step 2's mkustar into /dev/outbox (as rung gcc9a)
filelist /build/usr.list usr
/build/mkustar /build/rootfs.tar . /build/usr.list
catm /dev/outbox /build/rootfs.tar
sha256sum -o /build/rootfs.sum /build/rootfs.tar
read sum rest < /build/rootfs.sum
echo "rootfs sha256 ${sum}"
echo rung done gcc9
