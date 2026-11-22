#!/bin/sh
set -eu
mkdir -p /build/fixdep-probe
cd /build/fixdep-probe
printf 'int aegis;\n' > probe.c
printf '#ifdef CONFIG_AEGIS_DEP\n#endif\n' > probe.h
printf 'probe.o: probe.c probe.h\n' > probe.d
/build/busybox/scripts/basic/fixdep probe.d probe.o 'gcc -static -c probe.c' > probe.cmd
grep -q 'include/config/aegis/dep.h' probe.cmd
grep -q 'probe.h' probe.cmd
grep -q 'cmd_probe.o' probe.cmd
echo 'busybox fixdep dependencies ok'

mkdir -p /build/split-probe/config
cd /build/split-probe
printf '#define CONFIG_KEEP 1\n#define CONFIG_NESTED_VALUE 2\n' > autoconf.h
printf '#define CONFIG_KEEP 0\n' > config/keep.h
printf '#define CONFIG_OLD 1\n' > config/old.h
find config -type f -name '*.h' -printf '%P\n' > old-headers
/build/busybox/scripts/basic/split-include /build/split-probe/autoconf.h /build/split-probe/config < old-headers
grep -q '^#define CONFIG_KEEP 1$' config/keep.h
grep -q '^#define CONFIG_NESTED_VALUE 2$' config/nested/value.h
test ! -s config/old.h
echo 'busybox split config cleanup ok'
