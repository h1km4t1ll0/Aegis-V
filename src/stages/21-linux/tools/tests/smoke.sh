set -e
export LC_ALL=C
mkdir -p /build/tests
cd /build/tests

bash --version > bash.version
sed --version > sed.version
bison --version > bison.version
bc --version > bc.version
cat bash.version sed.version bison.version bc.version
grep -q '^GNU bash, version 4\.4\.0' bash.version
grep -q '^sed (GNU sed) 4\.8$' sed.version
grep -q '^bison (GNU Bison) 3\.0\.4$' bison.version
grep -q '^bc 1\.07\.1$' bc.version
echo 'bash version 4.4'
echo 'sed version 4.8'
echo 'bison version 3.0.4'
echo 'bc version 1.07.1'

/bin/sh /steps/tests/bash.sh
printf 'linux-6.6\n' | sed -E 's/^([a-z]+)-([0-9]+)\.([0-9]+)$/\1 \2 \3/' > sed.out
test "$(cat sed.out)" = 'linux 6 6'
echo 'sed extended regex ok'

bison --defines=parser.h -o parser.c /steps/tests/parser.y
gcc -static -O2 -Wall -o parser parser.c
printf '6 * (5 + 2)\n' | ./parser > parser.out
test "$(cat parser.out)" = 42
printf '2 + 3 * 4\n' | ./parser > parser.out
test "$(cat parser.out)" = 14
if printf '2 + * 3\n' | ./parser > parser-bad.out 2>&1; then
    echo 'invalid grammar input accepted'
    exit 1
fi
grep -q 'syntax error' parser-bad.out
echo 'bison parser ok'

printf '2^100\nscale=20\nsqrt(2)\n' | bc -l > bc.out
test "$(sed -n 1p bc.out)" = 1267650600228229401496703205376
test "$(sed -n 2p bc.out)" = 1.41421356237309504880
echo 'bc arithmetic ok'

for f in /usr/bin/bash /usr/bin/sh /usr/bin/sed /usr/bin/bison /usr/bin/bc \
    /usr/bin/dc /build/paxx /build/mkustar /build/tests/parser; do
    readelf -h "${f}" > elf-header
    grep -q 'RISC-V' elf-header
    readelf -l "${f}" > elf-program
    readelf -d "${f}" > elf-dynamic
    if grep -q INTERP elf-program || grep -q NEEDED elf-dynamic; then
        echo "dynamic executable: ${f}"
        exit 1
    fi
    echo "static RISC-V ${f}: $(wc -c < "${f}") bytes"
done
echo 'linux tools static ok'
