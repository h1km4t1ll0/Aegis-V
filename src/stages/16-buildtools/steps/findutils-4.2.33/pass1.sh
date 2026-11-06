# SPDX-FileCopyrightText: 2022 Andrius Štikonas <andrius@stikonas.eu>
# SPDX-FileCopyrightText: 2022,2025 Samuel Tyler <samuel@samuelt.me>
#
# SPDX-License-Identifier: GPL-3.0-or-later

# Aegis-V: the tarball's own configure, Makefiles and gnulib copy; no
# import-gnulib.sh, no autoreconf, doc/find.info and po/*.gmo kept
# (16-buildtools README)

src_configure() {
    # Musl is not recognized, pretend to be uClibc
    # Must use --host for config.charset reproducibility
    # Aegis-V: --build too (config.guess cannot guess Tilck); LD=tcc for
    # gettext's AC_LIB_PROG_LD (there is no ld before binutils); musl's
    # getcwd, not gnulib's, which walks ".." with openat on real dirfds
    # (Tilck: ENOSYS) (16-buildtools README)
    gl_cv_func_getcwd_path_max=yes \
    CC=tcc LD=tcc ./configure --prefix="${PREFIX}" \
        --build="${TARGET/-musl/-gnu}" \
        --host="${TARGET/-musl/-gnu}" \
        CPPFLAGS="-D__UCLIBC__"
}

# Aegis-V: AR (the Makefiles say AR = ar; there is no ar before binutils)
src_compile() {
    make "${MAKEJOBS}" MAKEINFO=true AR="tcc -ar" DESTDIR="${DESTDIR}"
}

src_install() {
    make MAKEINFO=true AR="tcc -ar" DESTDIR="${DESTDIR}" install
}
