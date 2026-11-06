# SPDX-FileCopyrightText: 2021 Andrius Štikonas <andrius@stikonas.eu>
# SPDX-FileCopyrightText: 2022 Samuel Tyler <samuel@samuelt.me>
#
# SPDX-License-Identifier: GPL-3.0-or-later

PACKAGE=grep
VERSION=2.4

CC      = tcc
LD      = tcc
AR      = tcc -ar
LDFLAGS = -static

CFLAGS  = -DPACKAGE=\"$(PACKAGE)\" \
          -DVERSION=\"$(VERSION)\" \
          -DHAVE_DIRENT_H=1 \
          -DSTDC_HEADERS=1 \
          -DHAVE_STRING_H=1 \
          -DHAVE_STDLIB_H=1 \
          -DHAVE_UNISTD_H=1 \
          -DHAVE_STRERROR=1 \
          -DREGEX_MALLOC=1

.PHONY: all

GREP_SRC = grep dfa kwset obstack regex stpcpy savedir getopt getopt1 search
GREP_OBJECTS = $(addprefix src/, $(addsuffix .o, $(GREP_SRC)))

# Aegis-V: egrep and fgrep as upstream's src/Makefile.am builds them (with
# egrepmat.o, fgrepmat.o): grep 2.4 picks its matcher from these objects,
# not from argv[0], so links to grep were a second plain grep
all: grep egrep fgrep

grep: $(GREP_OBJECTS) src/grepmat.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

egrep: $(GREP_OBJECTS) src/egrepmat.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

fgrep: $(GREP_OBJECTS) src/fgrepmat.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

install: all
	install -D grep $(DESTDIR)$(PREFIX)/bin/grep
	install egrep $(DESTDIR)$(PREFIX)/bin/egrep
	install fgrep $(DESTDIR)$(PREFIX)/bin/fgrep
