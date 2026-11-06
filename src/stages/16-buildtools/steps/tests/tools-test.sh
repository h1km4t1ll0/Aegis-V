# Rung build1: one check per new tool (16-buildtools README). Prints
# `build1 tools ok` only if every check gives the expected output;
# otherwise the failures and exit 1.
fail=0
check() {
    if [ "$2" = "$3" ]; then
        echo "check $1 ok"
    else
        echo "check $1 FAIL: want [$2] got [$3]"
        fail=1
    fi
}

printf 'apple\nbanana\napricot\ncherry\n' > fruits
check grep 2 "$(grep -c '^ap' fruits)"
check egrep 2 "$(egrep -c 'an|err' fruits)"
check fgrep 1 "$(fgrep -c 'ana' fruits)"

printf '1\n2\n3\n4\n' > nums
check awk 10 "$(awk '{s+=$1} END {print s}' nums)"
check awk-fs 'b:2' "$(echo 'a b c' | awk '{print $2 ":" NF-1}')"

printf 'a\nb\nc\n' > f1
printf 'a\nB\nc\n' > f2
cmp f1 f1
check cmp-same 0 $?
cmp f1 f2 > cmp.out
check cmp-differ '1 f1 f2 differ: char 3, line 2' "$? $(cat cmp.out)"
diff -u f1 f2 > d.diff
check diff-status 1 $?
check diff-u '-b +B' "$(grep '^[-+][a-zA-Z]' d.diff | tr '\n' ' ' | sed 's/ $//')"
cp f1 f3
patch f3 < d.diff > /dev/null
cmp f2 f3
check diff-patch 0 $?

check m4 'hello world' "$(printf 'define(`X'"'"', `hello'"'"')dnl\nX world\n' | m4)"

cp /steps/tests/words.l words.l
flex words.l
tcc -static -o words lex.yy.c
check flex 'flex words 3 lines 2' "$(printf 'one two\nthree\n' | ./words)"

mkdir -p ft/src/a ft/src/b/c
printf 'x\ny\n' > ft/src/a/one.c
printf 'z\n' > ft/src/b/c/two.c
printf 'w\n' > ft/src/b/notc.h
cd ft
check find './src/a/one.c ./src/b/c/two.c' "$(find . -name '*.c' | sort | tr '\n' ' ' | sed 's/ $//')"
check find-xargs-wc 3 "$(find . -name '*.c' | xargs wc -l | tail -1 | awk '{print $1}')"
check find-type-d 4 "$(find src -type d | wc -l | awk '{print $1}')"
check du 1 "$(du -s src > ../du.out && awk '{print ($1 > 0)}' ../du.out)"
cd ..

check uname riscv64 "$(uname -m)"
check env 'A=1' "$(env -i A=1 /usr/bin/env)"
check date 1970 "$(date -u -d '1970-01-01 00:00:00 UTC' +%Y)"
dd if=nums of=nums2 bs=1 2> /dev/null
cmp nums nums2
check dd 0 $?
check nice x "$(nice -n 5 echo x)"
chown 0:0 f1
check chown 0 $?

if [ $fail != 0 ]; then
    echo "build1 tools FAIL"
    exit 1
fi
echo "build1 tools ok"
