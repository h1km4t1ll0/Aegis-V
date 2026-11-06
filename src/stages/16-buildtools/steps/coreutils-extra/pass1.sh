# SPDX-FileCopyrightText: 2021 Paul Dersey <pdersey@gmail.com>
# SPDX-FileCopyrightText: 2022 Samuel Tyler <samuel@samuelt.me>
# SPDX-FileCopyrightText: 2022 Andrius Štikonas <andrius@stikonas.eu>
#
# SPDX-License-Identifier: GPL-3.0-or-later

# Aegis-V: live-bootstrap's coreutils-5.0/pass2.sh for the programs rung
# tools2 did not build (mk/main.mk); the pre-generated files stay
# (lib/getdate.c: getdate.y needs bison's %pure_parser, our yacc stops)

src_prepare() {
    default

    cp lib/fnmatch_.h lib/fnmatch.h
    cp lib/ftw_.h lib/ftw.h
    cp lib/search_.h lib/search.h
    touch config.h
}
