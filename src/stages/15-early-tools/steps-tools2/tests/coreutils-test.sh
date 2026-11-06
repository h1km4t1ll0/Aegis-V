# Rung tools2: one-line checks of coreutils 5.0 (15-early-tools README), run
# as `bash coreutils-test.sh` in an empty scratch directory. Prints
# "coreutils ok" only if every check gives the expected output; otherwise
# prints the failures, exit 1. test is bash's builtin too: /usr/bin/test is
# called by path.
fail=0
ck () {
  if test "x$2" = "x$3"; then
    :
  else
    echo "coreutils-test FAIL $1: got [$2] want [$3]"
    fail=1
  fi
}

mkdir -p w/a/b/c
ck mkdir-p "$(ls -d w/a/b/c)" w/a/b/c
printf 'one\ntwo\nthree\ntwo\n' > w/f
cat w/f > w/g
ck cat "$(cat w/g)" "one
two
three
two"
cp w/f w/a/f2
ck cp "$(cat w/a/f2)" "$(cat w/f)"
mv w/a/f2 w/a/b/f3
ck mv "$(ls w/a w/a/b)" "w/a:
b

w/a/b:
c
f3"
rm w/a/b/f3
ck rm "$(ls w/a/b)" c
ck ls "$(ls w)" "a
f
g"
install -m 755 w/f w/inst
ck install "$(ls -l w/inst | cut -c1-10)" "-rwxr-xr-x"
chmod 640 w/inst
ck chmod "$(ls -l w/inst | cut -c1-10)" "-rw-r-----"
ck test "$(/usr/bin/test -d w/a && /usr/bin/test -f w/f && /usr/bin/test ! -e w/none && /usr/bin/test 3 -gt 2; echo $?)" 0
ck test-false "$(/usr/bin/test -d w/f; echo $?)" 1
ck expr "$(expr 6 \* 7)" 42
ck head "$(head -n 2 w/f | tr '\n' ' ')" "one two "
ck tail "$(tail -n 1 w/f)" two
ck wc "$(wc -l < w/f | tr -d ' ')" 4
ck sort "$(sort w/f | tr '\n' ' ')" "one three two two "
ck uniq "$(sort w/f | uniq -c | tr '\t' ' ' | tr -s ' ' | tr '\n' ',')" " 1 one, 1 three, 2 two,"
ck tr "$(echo hello | tr a-z A-Z)" HELLO
ck cut "$(echo a:b:c | cut -d: -f2)" b

test $fail = 0 || exit 1
echo coreutils ok
