# Rung tools2: bash 2.05b smoke test (15-early-tools README), run as
# `bash sh-test.sh` in a scratch directory. Prints "bash ok" only if every
# check gives the expected output; otherwise prints the failures, exit 1.
fail=0
ck () {
  if test "x$2" = "x$3"; then
    :
  else
    echo "sh-test FAIL $1: got [$2] want [$3]"
    fail=1
  fi
}

# pipeline of three external programs
ck pipe "$(printf 'hello world\n' | sed 's/world/tilck/' | tr a-z A-Z)" "HELLO TILCK"

# command substitution, nested
ck subst "$(echo a$(echo b)c)" abc

# $? after false and true
false
ck false "$?" 1
true
ck true "$?" 0

# for + case
k=
for f in x.c y.h z.o; do
  case $f in
    *.c) k="${k}c" ;;
    *.h) k="${k}h" ;;
    *) k="${k}-" ;;
  esac
done
ck case "$k" "ch-"

# function with a local variable (called in this shell, then in $(...))
v=outer
fn () {
  local v
  v=inner
  echo "$v:$1"
}
fn x > fn.out
ck local "$v" outer
ck func "$(fn arg)" "inner:arg"
ck func-out "$(cat fn.out)" "inner:x"

# for
s=
for i in 1 2 3; do
  s="$s$i"
done
ck for "$s" 123

# redirection to a file, append, cat
echo line1 > r.txt
echo line2 >> r.txt
ck redir "$(cat r.txt)" "line1
line2"

# while read from a file
printf 'k1 v1\nk2 v2\n' > kv.txt
t=
n=0
while read a b; do
  t="$t$a=$b;"
  n=$((n + 1))
done < kv.txt
ck while "$t$n" "k1=v1;k2=v2;2"

test $fail = 0 || exit 1
echo bash ok
