# SPDX-FileCopyrightText: 2021 Andrius Štikonas <andrius@stikonas.eu>
# SPDX-FileCopyrightText: 2022 Samuel Tyler <samuel@samuelt.me>
#
# SPDX-License-Identifier: GPL-3.0-or-later

CC := tcc
LDFLAGS = -static
CFLAGS = -DVERSION=\"2.6.4\" \
         -DM4=\"m4\" \
         -DHAVE_LIMITS_H=1

all: flex

flex: buf.o ccl.o dfa.o ecs.o filter.o gen.o main.o misc.o nfa.o options.o parse.o regex.o scan.o scanflags.o scanopt.o skel.o sym.o tables.o tables_shared.o tblcmp.o yylex.o
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -g -c -I. -o $@ $<

main.o: parse.h
scan.o: parse.h
yylex.o: parse.h

install: all
	install -D flex $(DESTDIR)$(PREFIX)/bin/flex
	ln -sf flex $(DESTDIR)$(PREFIX)/bin/lex
