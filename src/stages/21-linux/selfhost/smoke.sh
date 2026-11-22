#!/bin/sh
set -e
export PATH=/usr/bin:/usr/sbin:/bin:/sbin
/bin/sh /smoke-base.sh
/usr/bin/bash -c 'declare -A a; a[answer]=42; test "${a[answer]}" = 42; echo "linux bash ok $BASH_VERSION"'
gcc -static -O2 -pthread /checks/pthread.c -o /tmp/pthread-check
/tmp/pthread-check
