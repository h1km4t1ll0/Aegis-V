#!/bin/sh
# Run only inside the bootstrapped riscv64 guest. Never invokes mke2fs on a disk.
set -eu
export PATH=/usr/bin:/usr/sbin:/bin:/sbin LC_ALL=C TZ=UTC0
export SHELL=/bin/sh CONFIG_SHELL=/bin/sh CONFIG_SITE=/dev/null
unset CC CXX CFLAGS CXXFLAGS CPPFLAGS LDFLAGS LIBS MAKEFLAGS MFLAGS

die() { echo "disk-tools: $*" >&2; exit 1; }
case $(uname -s) in Tilck|Linux) ;; *) die 'requires Tilck or Linux guest';; esac
test "$(uname -m)" = riscv64 || die 'requires native riscv64 guest'
test "$(gcc -dumpmachine)" = riscv64-unknown-linux-musl || die 'unexpected GCC target'
test "$(gcc -dumpfullversion)" = 9.5.0 || die 'requires guest GCC 9.5.0'

RECIPE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SRC=/build/e2fs-src
OBJ=/build/e2fs-build
OUT=/build/disk-tools
test -f "$SRC/configure" || die 'extract the pinned release into /build/e2fs-src first'
for path in "$OBJ" "$OUT"; do
    test ! -e "$path" && test ! -L "$path" || die "refusing existing output: $path"
done
test ! -f "$SRC/config.status" || die 'requires an unconfigured source tree'
mkdir "$OUT"
mkdir "$OUT/logs" "$OUT/recipe" "$OUT/sbin" "$OUT/etc" "$OBJ"
cp "$RECIPE/build.sh" "$RECIPE/persistence.sh" "$RECIPE/README.md" \
    "$RECIPE/sources" "$RECIPE/source-checksums.sha256" "$OUT/recipe/"
exec 3>&1
exec > "$OUT/logs/build.log" 2>&1
finish() {
    rc=$?
    trap - 0
    if test -f "$OBJ/config.log"; then
        cp "$OBJ/config.log" "$OUT/logs/config.log" || rc=1
    fi
    if test "$rc" != 0; then
        echo "disk-tools failed ($rc); see $OUT/logs/build.log" >&3
        tail -60 "$OUT/logs/build.log" >&3
    fi
    exit "$rc"
}
trap finish 0
run() { printf '+'; printf ' %s' "$@"; printf '\n'; "$@"; }

echo 'e2fsprogs 1.45.7, official tytso release; native guest source build'
cat "$OUT/recipe/sources"
uname -a
gcc -v
echo "source=$SRC build=$OBJ output=$OUT"
echo 'CC=gcc -static; CFLAGS=-O2 -g0; LDFLAGS=-static; BUILD_CC=gcc -static'
echo 'Optional libmagic and dlopen probes deliberately disabled through configure cache variables.'
(cd "$SRC"; sha256sum -c "$OUT/recipe/source-checksums.sha256")
if test -n "${DISTFILES-}"; then
    archive=$DISTFILES/e2fsprogs-1.45.7.tar.xz
    test -f "$archive" || die "missing archive: $archive"
    printf '%s  %s\n' \
        62d49c86d9d4becf305093edd65464484dc9ea41c6ff9ae4f536e4a341b171a2 \
        "$archive" > "$OUT/logs/archive.sha256"
    sha256sum -c "$OUT/logs/archive.sha256"
else
    echo 'Archive not supplied: caller must verify its hash before guest extraction.'
    echo 'The source-checksums manifest checks recipe inputs, not the entire extracted tree.'
fi

# Linux's minimal boot has no RTC userspace setup, so CLOCK_REALTIME starts
# at 1970. Future release mtimes make make regenerate its own Makefile forever.
# Normalize only source timestamps; the verified file contents stay unchanged.
find "$SRC" -type f -exec touch -t 197001010000.00 {} +
echo 'disk-tools: source mtimes normalized' >&3
cd "$OBJ"
echo 'disk-tools: configuring (full output in /build/disk-tools/logs/build.log)' >&3
run /bin/sh "$SRC/configure" \
    --enable-option-checking=fatal \
    --build=riscv64-unknown-linux-musl --host=riscv64-unknown-linux-musl \
    --prefix="$OUT" --sysconfdir="$OUT/etc" \
    --enable-libuuid --enable-libblkid \
    --disable-elf-shlibs --disable-bsd-shlibs --disable-profile \
    --disable-nls --disable-threads --disable-tls --disable-uuidd \
    --disable-backtrace --disable-testio-debug \
    --disable-debugfs --disable-imager --disable-resizer \
    --disable-defrag --disable-fuse2fs \
    CC='gcc -static' CFLAGS='-O2 -g0' LDFLAGS=-static \
    BUILD_CC='gcc -static' \
    ac_cv_lib_magic_magic_file=no ac_cv_lib_dl_dlopen=no ac_cv_func_dlopen=no
echo 'disk-tools: configured' >&3
run /bin/sh ./config.status --config
cp MCONFIG "$OUT/logs/MCONFIG"
cp lib/config.h "$OUT/logs/config.h"

# 'subs' builds/runs util/subst and prepares types, compile_et and error tables.
# No top-level 'all', 'libs', 'install', documentation or unrelated executables.
echo 'disk-tools: building native generators' >&3
run make -j1 subs
for pair in et:com_err uuid:uuid blkid:blkid e2p:e2p support:support ext2fs:ext2fs; do
    dir=${pair%:*}
    lib=${pair#*:}
    echo "disk-tools: building lib$lib.a" >&3
    run make -j1 -C "lib/$dir" "lib$lib.a"
done
echo 'disk-tools: linking mke2fs.static' >&3
run make -j1 -C misc mke2fs.static mke2fs.conf
cp misc/mke2fs.static "$OUT/sbin/mke2fs"
chmod 755 "$OUT/sbin/mke2fs"
cp misc/mke2fs.conf "$OUT/etc/mke2fs.conf"

# Verify both native executable generators as well as the delivered program.
for binary in util/subst lib/ext2fs/gen_crc32ctable "$OUT/sbin/mke2fs"; do
    test -x "$binary" || die "missing guest-built executable: $binary"
    tag=$(basename "$binary")
    readelf -h "$binary" > "$OUT/logs/$tag.elf-header"
    readelf -l "$binary" > "$OUT/logs/$tag.elf-program"
    readelf -d "$binary" > "$OUT/logs/$tag.elf-dynamic"
    grep -q 'RISC-V' "$OUT/logs/$tag.elf-header" || die "wrong machine: $binary"
    if grep -q INTERP "$OUT/logs/$tag.elf-program" || \
        grep -q NEEDED "$OUT/logs/$tag.elf-dynamic"; then
        die "not static: $binary"
    fi
done
MKE2FS_CONFIG=$OUT/etc/mke2fs.conf
export MKE2FS_CONFIG
"$OUT/sbin/mke2fs" -V > "$OUT/logs/mke2fs.version" 2>&1
cat "$OUT/logs/mke2fs.version"
grep -q '^mke2fs 1\.45\.7 ' "$OUT/logs/mke2fs.version"
(cd "$OUT"; sha256sum -o logs/artifacts.sha256 sbin/mke2fs etc/mke2fs.conf)
echo 'disk-tools guest build and static ELF checks passed'
echo "disk-tools ready: $OUT (no filesystem created)" >&3
