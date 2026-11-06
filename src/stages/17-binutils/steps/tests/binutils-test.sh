# Rung binutils: the new as, ld, ar, nm, ranlib (17-binutils README).
# Each test prints its mark only if its output is as expected; otherwise
# the failure and exit 1.
set -e
L=/usr/lib
fail() {
    echo "binutils test FAIL: $*"
    exit 1
}

# as + ld on a hand-written program
as -o hello-asm.o /steps/tests/hello.s
ld -static -o hello-asm hello-asm.o
got=$(./hello-asm) || fail "hello-asm exit $?"
[ "${got}" = "binutils asm ok" ] || fail "hello-asm printed [${got}]"
echo "binutils asm ok"

# tcc -c + ld with musl's crt files and libc.a, tcc's libtcc1.a
tcc -c -o hello.o /steps/tests/hello.c
ld -static -o hello ${L}/crt1.o ${L}/crti.o hello.o ${L}/libc.a ${L}/tcc/libtcc1.a ${L}/crtn.o
got=$(./hello) || fail "hello exit $?"
[ "${got}" = "binutils hello ok" ] || fail "hello printed [${got}]"
echo "binutils hello ok"

# nm and ar t on libc.a; ranlib of a copy, ld through its index; ar rcs
n=$(nm ${L}/libc.a 2>/dev/null | grep -c ' T ')
members=$(( $(ar t ${L}/libc.a | wc -l) ))
echo "nm: ${n} T symbols in libc.a, ar t: ${members} members"
[ "${n}" -gt 1000 ] || fail "nm libc.a: ${n} T symbols"
[ "${members}" -gt 1000 ] || fail "ar t libc.a: ${members} members"
cp ${L}/libc.a libc-copy.a
ranlib libc-copy.a
[ $(( $(ar t libc-copy.a | wc -l) )) = "${members}" ] || fail "ar t after ranlib"
ld -static -o hello2 ${L}/crt1.o ${L}/crti.o hello.o libc-copy.a ${L}/tcc/libtcc1.a ${L}/crtn.o
got=$(./hello2) || fail "hello2 exit $?"
[ "${got}" = "binutils hello ok" ] || fail "hello2 (the ranlib'ed copy) printed [${got}]"
ar rcs libh.a hello.o
[ "$(ar t libh.a)" = "hello.o" ] || fail "ar rcs: [$(ar t libh.a)]"
echo "binutils ar ok"
