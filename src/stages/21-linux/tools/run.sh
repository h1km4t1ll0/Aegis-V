# Phase 1: all compilation and build generators run on Tilck with GCC 9.5.
# The host supplies only the previous /usr checkpoint and pinned source bytes.
set -e
export SHELL=/bin/sh CONFIG_SHELL=/bin/sh LC_ALL=C
unset CC CXX CFLAGS CXXFLAGS LDFLAGS CPPFLAGS LIBS
TARGET=riscv64-unknown-linux-musl
GCC='gcc -static'
OPT='-O2 -g0'
LOGS=/build/logs
PROVENANCE=/usr/share/aegis/linux-tools
mkdir -p /steps /tmp "${LOGS}"
cp -R /initrd/steps/. /steps/

# Same status-file/heartbeat scheme as gcc9: no kill -0 dependency on Tilck.
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
    w_next=60
    while [ ! -f /tmp/watched.rc ]; do
        sleep 5
        w_dt=$(($(date +%s) - w_t0))
        if [ ${w_dt} -ge ${w_next} ]; then
            w_next=$((w_next + 60))
            echo "linux-tools heartbeat $(basename "${w_log}" .log): ${w_dt} s, $(wc -l < "${w_log}") lines, last: $(tail -1 "${w_log}" | cut -c1-100)"
        fi
    done
    wait
    read w_rc < /tmp/watched.rc
}

step() {
    s_log=${LOGS}/$1.log
    s_mark=$2
    shift 2
    watched "${s_log}" "$@"
    if [ "${w_rc}" != 0 ]; then
        echo "linux-tools $(basename "${s_log}" .log) FAIL status ${w_rc}"
        tail -80 "${s_log}"
        if [ -f config.log ]; then
            echo '--- config.log before cache variables'
            sed -n '/^## Cache variables/q;p' config.log | tail -60
        fi
        exit 1
    fi
    tail -2 "${s_log}"
    echo "linux-tools time $(basename "${s_log}" .log): $(($(date +%s) - w_t0)) s"
    echo "${s_mark}"
}

# Use the newly installed shell: this recipe started under the old Bash.
streamed() {
    "${CONFIG_SHELL}" -o pipefail -c '"$@" 2>&1 | tee /dev/console' streamed "$@"
}

save_logs() {
    mkdir -p "${PROVENANCE}/logs" "${PROVENANCE}/recipe" || return 1
    cp -R /steps/. "${PROVENANCE}/recipe/" || return 1
    cp -R "${LOGS}/." "${PROVENANCE}/logs/" || return 1
    for pkg in bash sed bison bc; do
        if [ -f /build/${pkg}-src/config.log ]; then
            cp /build/${pkg}-src/config.log "${PROVENANCE}/logs/${pkg}-config.log" || return 1
        fi
    done
    if [ -d /build/tests ]; then
        cp -R /build/tests "${PROVENANCE}/tests" || return 1
    fi
}

pack_rootfs() {
    cd / || return 1
    filelist /build/usr.list usr || return 1
    /build/mkustar /build/rootfs.tar . /build/usr.list || return 1
    catm /dev/outbox /build/rootfs.tar || return 1
    sha256sum -o /build/rootfs.sum /build/rootfs.tar || return 1
    read sum rest < /build/rootfs.sum
    echo "rootfs sha256 ${sum}"
}

# A failed step exports its complete logs in the outbox before rung exit N.
# The driver rejects it (missing success marks/nonzero exit); outbox.bin is
# recoverable for diagnosis and is never named rootfs-linux-tools.tar.
finish() {
    f_rc=$1
    trap - EXIT
    if [ "${f_rc}" != 0 ]; then
        echo "linux-tools failed status ${f_rc}; preserving logs"
        save_logs || echo 'linux-tools log preservation failed'
        if [ -x /build/mkustar ]; then
            pack_rootfs || echo 'linux-tools diagnostic outbox failed'
        fi
    fi
    exit "${f_rc}"
}
trap 'finish $?' EXIT

cd /steps
step sources 'linux tools source ok' /bin/sh -ec 'checksum-transcriber sources; sha256sum -c sources.SHA256SUM'
gcc -v > "${LOGS}/gcc-version.log" 2>&1
cat "${LOGS}/gcc-version.log"
grep -q 'gcc version 9\.5\.0' "${LOGS}/gcc-version.log"

# Compile both helpers from prior-rung source, including mkustar's ustar
# prefix patch; no helper binary or generator is supplied by the host.
step paxx 'linux-tools paxx built' ${GCC} ${OPT} -Wall -o /build/paxx /steps/src/paxx.c
cp /steps/src/mkustar.c /build/mkustar.c
step mkustar-patch 'linux-tools mkustar patched' patch /build/mkustar.c /steps/src/mkustar-long.patch
step mkustar 'linux-tools mkustar built' ${GCC} ${OPT} -o /build/mkustar /build/mkustar.c

extract_gz() {
    gzip -dc "${DISTFILES}/$1" > /build/source.tar || return 1
    /build/paxx -C /build/$2-src -s 1 < /build/source.tar || return 1
    rm /build/source.tar
}
step bash-extract 'bash extracted' extract_gz bash-4.4.tar.gz bash
step sed-unxz 'sed decompressed' unxz --file "${DISTFILES}/sed-4.8.tar.xz" --output /build/sed.tar
step sed-extract 'sed extracted' /bin/sh -ec '/build/paxx -C /build/sed-src -s 1 < /build/sed.tar; rm /build/sed.tar'
step bison-extract 'bison extracted' extract_gz bison-3.0.4.tar.gz bison
step bc-extract 'bc extracted' extract_gz bc-1.07.1.tar.gz bc

# Bash's 2015 config.sub lacks RISC-V; sed's pinned 2020 copy knows it.
# musl 1.1.24 src/linux/sbrk.c rejects nonzero increments, which bash's
# bundled allocator requires (lib/malloc/malloc.c). Use musl's malloc.
cp /build/sed-src/build-aux/config.sub /build/bash-src/support/config.sub
if /bin/sh -ec 'test "${BASH_VERSINFO[0]}.${BASH_VERSINFO[1]}" = 4.4'; then
    echo 'bash installed (guest checkpoint)'
else
cd /build/bash-src
step bash-configure 'bash configured' "${CONFIG_SHELL}" ./configure --prefix=/usr \
    --build="${TARGET}" --host="${TARGET}" --enable-static-link --disable-nls \
    --disable-readline --without-bash-malloc CC="${GCC}" CFLAGS="${OPT}" LDFLAGS=-static
step bash-make 'bash built' make -j1 MAKEINFO=true
# Stage the complete installation while /bin/sh still runs the old bash.
step bash-install 'bash staged' make -j1 MAKEINFO=true DESTDIR=/build/bash-install install
step bash-probe 'bash probe ok' /build/bash-install/usr/bin/bash /steps/tests/bash.sh
mv /usr/bin/bash /build/bash-2.05b
cp -R /build/bash-install/usr/. /usr/
ln -sf bash /usr/bin/sh
test "$(readlink /bin/sh)" = /usr/bin/bash
/bin/sh -ec 'test "${BASH_VERSINFO[0]}.${BASH_VERSINFO[1]}" = 4.4'
echo 'bash installed'
fi

if [ "${1-}" = bash-only ]; then
    save_logs
    echo 'linux bash ok'
    pack_rootfs
    echo 'rung done linux-bash'
    exit 0
fi

cd /build
step rmdir-probe-build 'rmdir probe built' ${GCC} ${OPT} /steps/tests/rmdir-probe.c -o /build/rmdir-probe
step rmdir-probe 'rmdir probe ok' /build/rmdir-probe
step nanosleep-probe-build 'nanosleep probe built' ${GCC} ${OPT} /steps/tests/nanosleep-probe.c -o /build/nanosleep-probe
step nanosleep-probe 'nanosleep compatibility probe ok' /build/nanosleep-probe

if sed --version 2>/dev/null | grep -q -x 'sed (GNU sed) 4\.8'; then
    echo 'sed installed (guest checkpoint)'
else
cd /build/sed-src
step sed-configure 'sed configured' streamed "${CONFIG_SHELL}" ./configure --prefix=/usr \
    --build="${TARGET}" --host="${TARGET}" --disable-nls --disable-shared --enable-static \
    --disable-acl --without-selinux CC="${GCC}" CFLAGS="${OPT}" LDFLAGS=-static \
    gl_cv_func_nanosleep='no (mishandles large arguments)' gl_cv_func_sleep_works=no
grep -q '^#define HAVE_BUG_BIG_NANOSLEEP 1' config.h
step sed-make 'sed built' make -j1 MAKEINFO=true
step sed-install 'sed installed' make -j1 MAKEINFO=true install
fi

if bison --version 2>/dev/null | grep -q -x 'bison (GNU Bison) 3\.0\.4'; then
    echo 'bison installed (guest checkpoint)'
else
cd /build/bison-src
# Perl is used only by example extraction, docs and the full upstream tests.
# Build the real parser generator and its runtime skeletons, without those
# optional targets; no substitute Perl executable is supplied.
step bison-core-patch 'bison core patch applied' patch -p1 -i /steps/patches/bison-core-without-perl.patch
step bison-configure 'bison configured' "${CONFIG_SHELL}" ./configure --prefix=/usr \
    --build="${TARGET}" --host="${TARGET}" --disable-nls --disable-shared --enable-static \
    CC="${GCC}" CFLAGS="${OPT}" LDFLAGS=-static M4=/usr/bin/m4
step bison-make 'bison built' make -j1 MAKEINFO=true mfcalc_sources= rpcalc_sources= INFO_DEPS= MANS= DATA=
step bison-install 'bison installed' make -j1 install-binPROGRAMS install-binSCRIPTS \
    install-libLIBRARIES install-dist_pkgdataDATA install-dist_m4sugarDATA \
    install-dist_xsltDATA install-aclocalDATA
fi

cd /build/bc-src
step bc-patch 'bc ed dependency removed' patch -p1 -i /steps/patches/bc-no-ed.patch
step bc-configure 'bc configured' "${CONFIG_SHELL}" ./configure --prefix=/usr \
    --build="${TARGET}" --host="${TARGET}" --without-readline --without-libedit \
    CC="${GCC}" CFLAGS="${OPT}" LDFLAGS=-static YACC='bison -y' LEX=flex
# Force real grammar/scanner generation by the installed guest tools.
rm -f bc/bc.c bc/bc.h bc/scan.c
step bc-make 'bc built' make -j1 MAKEINFO=true
grep -q 'bison -y' "${LOGS}/bc-make.log"
step bc-install 'bc installed' make -j1 MAKEINFO=true install

step smoke 'linux-tools smoke tests passed' /bin/sh /steps/tests/smoke.sh
# Replay test markers in order for the existing driver, keeping full output.
cat "${LOGS}/smoke.log"
save_logs
echo 'linux tools ok'
pack_rootfs
echo 'rung done linux-tools'
