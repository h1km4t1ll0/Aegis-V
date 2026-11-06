# Rung build1 on Tilck (16-buildtools README). run.kaem runs this with
# /bin/sh (bash 2.05b of rung tools2) from / ; /initrd is read-only.
# rung-init's environment: PREFIX BINDIR LIBDIR INCDIR DISTFILES ARCH PATH.
set -e

export SHELL=/bin/sh CONFIG_SHELL=/bin/sh
# tcc-m links a dynamic executable without -static (14-seed README)
export LDFLAGS=-static
export TARGET=riscv64-unknown-linux-musl
MAKEJOBS=-j1
DESTDIR=

# 1. recipes to /steps (they write build/ and sources.SHA256SUM)
mkdir -p /steps /build /tmp
cp -R /initrd/steps/. /steps/

# 2. live-bootstrap's build() (steps/helpers.sh at the pin), cut down: the
# guest checks the tarball, unpacks it in /steps/PKG/build, runs the
# recipe's src_* functions from PKG/pass1.sh or the defaults below, and
# installs straight into /usr (no DESTDIR package, no preseed)
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

# 3. the packages (live-bootstrap's steps/manifest has grep, m4, flex,
# diffutils, coreutils-5.0 pass 2, gawk, ..., findutils; the extra coreutils
# programs before the first configure)
build grep-2.4
build gawk-3.0.4
build diffutils-2.7
build m4-1.4.7
build flex-2.6.4
build coreutils-extra
build findutils-4.2.33

# 4. tests: versions, one check per tool, a real configure
grep --version
gawk --version
diff --version
cmp --version
m4 --version
flex --version
find --version
xargs --version
uname --version
mkdir -p /build/t /build/cfgtest
cd /build/t
bash /steps/tests/tools-test.sh
cd /build/cfgtest
bash /steps/tests/configure-test.sh

# 5. /usr packed with mkustar into /dev/outbox (as rung tools2)
cd /
filelist /build/usr.list usr
mkustar /build/rootfs.tar . /build/usr.list
catm /dev/outbox /build/rootfs.tar
sha256sum -o /build/rootfs.sum /build/rootfs.tar
read sum rest < /build/rootfs.sum
echo "rootfs sha256 ${sum}"
echo rung done build1
