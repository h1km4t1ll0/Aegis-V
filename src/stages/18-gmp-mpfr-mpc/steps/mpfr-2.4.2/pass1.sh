# Rung gmp (18-gmp-mpfr-mpc README): mpfr 2.4.2 with its release
# tarball's configure, static only, on the gmp just installed in /usr.

src_prepare() {
    default_src_prepare
    cp "${CONFIG_DIR}/config.sub" config.sub
    cp "${CONFIG_DIR}/config.guess" config.guess
}

src_configure() {
    step mpfr configure ./configure --prefix="${PREFIX}" \
        --build="${TARGET}" --host="${TARGET}" \
        --disable-shared --enable-static --with-gmp="${PREFIX}" \
        CC=tcc MAKEINFO=true
}

src_compile() {
    step mpfr make make "${MAKEJOBS}" MAKEINFO=true
    check mpfr
}

src_install() {
    step mpfr install make install MAKEINFO=true
}
