# Rung build1: a real configure (16-buildtools README). A fresh grep-2.4
# from its release tarball: ./configure, make, make install under
# /tmp/x, then the installed grep must work; prints `configure ok`.
set -e
gzip -dc "${DISTFILES}/grep-2.4.tar.gz" | tar xf -
# grep 2.4's config.sub (1999) does not know riscv64; findutils 4.2.33's
# (2007) accepts any CPU-unknown-OS
gzip -dc "${DISTFILES}/findutils-4.2.33.tar.gz" | tar xf - findutils-4.2.33/config.sub
cd grep-2.4
cp ../findutils-4.2.33/config.sub config.sub
t0=$(date +%s)
# autoconf 2.13 takes CC from the environment (as an argument: "CC=tcc:
# invalid host type"); --host: config.guess cannot guess Tilck (2.13
# takes the build type from the host type); --disable-nls: otherwise it
# builds its intl/ copy of gettext (musl has no msgfmt), whose alloca
# declaration clashes with musl's
CC=tcc ./configure --prefix=/tmp/x --host=riscv64-unknown-linux-gnu --disable-nls
t1=$(date +%s)
make
t2=$(date +%s)
make install
echo "cfgtest: configure $((t1 - t0)) s, make $((t2 - t1)) s"
printf 'one\ntwo\nthree\n' > in.txt
got=$(/tmp/x/bin/grep -c 't' in.txt)
if [ "$got" != 2 ]; then
    echo "configure FAIL: /tmp/x/bin/grep -c t gave [$got]"
    exit 1
fi
/tmp/x/bin/grep --version | head -1
echo "configure ok"
