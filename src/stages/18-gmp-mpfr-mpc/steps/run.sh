# Rung gmp on Tilck (18-gmp-mpfr-mpc README). run.kaem runs this with
# /bin/sh (bash 2.05b of rung tools2) from / ; /initrd is read-only.
# rung-init's environment: PREFIX BINDIR LIBDIR INCDIR DISTFILES ARCH PATH.
set -e

export SHELL=/bin/sh CONFIG_SHELL=/bin/sh
# tcc-m links a dynamic executable without -static (14-seed README);
# libtool takes -static as its own flag and passes -Wc,-static on to tcc
# (17-binutils README)
export LDFLAGS="-static -Wc,-static"
# skips libtool's probe of the longest command line (as rung binutils)
export lt_cv_sys_max_cmd_len=32768
export TARGET=riscv64-unknown-linux-musl
MAKEJOBS=-j1
DESTDIR=

# 1. recipes to /steps (they write build/ and sources.SHA256SUM)
mkdir -p /steps /build /tmp
cp -R /initrd/steps/. /steps/

# 2. live-bootstrap's build() (steps/helpers.sh at the pin), cut down as in
# rungs build1 and binutils
fn_exists() {
    test "$(type -t "$1")" = function
}

call() {
    if fn_exists "$1"; then
        $1
    else
        default_"${1}"
    fi
}

default() {
    "default_${build_stage}"
}

default_src_unpack() {
    set -- $(head -n 1 ../sources)
    f=$(basename "$2")
    case "$f" in
    *.tar.gz) gzip -dc "${DISTFILES}/${f}" | tar xf - ;;
    *.tar.bz2) bzip2 -dc "${DISTFILES}/${f}" | tar xf - ;;
    *) echo "unknown archive $f"; exit 1 ;;
    esac
    if ! [ -e "${dirname}" ]; then
        for i in *; do
            if [ -d "${i}" ]; then
                dirname="${i}"
                break
            fi
        done
    fi
}

default_src_prepare() {
    if test -d "${patch_dir}"; then
        for p in "${patch_dir}"/*.patch; do
            echo "Applying patch: ${p}"
            patch -d.. -Np0 < "${p}"
        done
    fi
    makefile="${mk_dir}/main.mk"
    if test -e "${makefile}"; then
        cp "${makefile}" Makefile
    fi
}

default_src_configure() {
    :
}

default_src_compile() {
    make "${MAKEJOBS}" -f Makefile PREFIX="${PREFIX}"
}

default_src_install() {
    make -f Makefile install PREFIX="${PREFIX}" DESTDIR="${DESTDIR}"
}

build() {
    pkg=$1
    dirname=${pkg}
    base_dir=/steps/${pkg}
    cd "${base_dir}"
    echo "${pkg}: checking sources."
    checksum-transcriber sources
    sha256sum -c sources.SHA256SUM
    patch_dir="${base_dir}/patches"
    mk_dir="${base_dir}/mk"
    rm -rf build
    mkdir build
    cd build
    unset -f src_unpack src_prepare src_configure src_compile src_install
    if test -e "${base_dir}/pass1.sh"; then
        . "${base_dir}/pass1.sh"
    fi
    for build_stage in src_unpack src_prepare src_configure src_compile src_install; do
        echo "${pkg}: ${build_stage}."
        call ${build_stage}
        if [ ${build_stage} = src_unpack ]; then
            cd "${dirname}"
        fi
    done
    cd /steps
    rm -rf "${base_dir}/build"
    echo "${pkg}: build successful"
}

# 3. helpers for the recipes (steps/*/pass1.sh)
mkdir -p /build/logs

# step LIB NAME CMD...: CMD's output in /build/logs/LIB-NAME.log, then its
# last two lines, the guest's seconds and `LIB NAME ok`; or `LIB NAME FAIL`,
# the tail of the log and of the newest config.log, and exit 1
step() {
    s_lib=$1
    s_name=$2
    shift 2
    s_log=/build/logs/${s_lib}-${s_name}.log
    s_t0=$(date +%s)
    if "$@" > "${s_log}" 2>&1; then
        tail -2 "${s_log}"
        echo "${s_lib} ${s_name}: $(($(date +%s) - s_t0)) s"
        echo "${s_lib} ${s_name} ok"
    else
        echo "${s_lib} ${s_name} FAIL"
        tail -60 "${s_log}"
        s_cl=$(ls -t config.log */config.log 2>/dev/null | head -1)
        if [ -n "${s_cl}" ]; then
            echo "--- ${s_cl}"
            tail -40 "${s_cl}"
        fi
        exit 1
    fi
}

# check LIB: `make check` (automake's serial harness: "All N tests passed"
# per directory, "N of M tests failed"); its summary lines and total, then
# `LIB check ok` only if make returned 0; otherwise `LIB check FAIL`, the
# FAIL: lines, the log's tail and exit 1
check() {
    c_log=/build/logs/$1-check.log
    c_t0=$(date +%s)
    if make "${MAKEJOBS}" check MAKEINFO=true > "${c_log}" 2>&1; then
        c_rc=0
    else
        c_rc=$?
    fi
    grep -E '^All [0-9]+ tests|tests? (was|were) not run|^[0-9]+ of [0-9]+ tests? failed' "${c_log}" || true
    awk -v lib="$1" '/^All [0-9]+ tests/ { n += $2; d++ }
        END { printf "%s check: %d tests passed in %d directories\n", lib, n, d }' "${c_log}"
    echo "$1 check: $(($(date +%s) - c_t0)) s, make check exit ${c_rc}"
    if [ "${c_rc}" = 0 ]; then
        echo "$1 check ok"
    else
        echo "$1 check FAIL"
        grep -E '^FAIL' "${c_log}" || true
        tail -60 "${c_log}"
        exit 1
    fi
}

# 4. riscv64-aware config.sub and config.guess (2018-01-01) from the pinned
# binutils 2.30 tarball: those of gmp 4.3.2, mpfr 2.4.2 and mpc 0.8.1
# (2008-2009) reject riscv64-unknown-linux-musl
cd /steps/riscv-config
checksum-transcriber sources
sha256sum -c sources.SHA256SUM
bzip2 -dc "${DISTFILES}/binutils-2.30.tar.bz2" | tar xf - binutils-2.30/config.sub binutils-2.30/config.guess
CONFIG_DIR=/steps/riscv-config/binutils-2.30
sha256sum "${CONFIG_DIR}/config.sub" "${CONFIG_DIR}/config.guess"

# 5. gmp, mpfr, mpc: configure, make, make check, make install
build gmp-4.3.2
build mpfr-2.4.2
build mpc-0.8.1

# 6. the installed headers and libraries
mkdir -p /build/t
cd /build/t
tcc -static -o libs-test /steps/tests/libs-test.c -lmpc -lmpfr -lgmp
./libs-test

# 7. /usr packed with mkustar into /dev/outbox (as rung binutils)
cd /
filelist /build/usr.list usr
mkustar /build/rootfs.tar . /build/usr.list
catm /dev/outbox /build/rootfs.tar
sha256sum -o /build/rootfs.sum /build/rootfs.tar
read sum rest < /build/rootfs.sum
echo "rootfs sha256 ${sum}"
echo rung done gmp
