# Rung gmp (18-gmp-mpfr-mpc README): mpc 0.8.1 with its release
# tarball's configure, static only, on gmp and mpfr in /usr.

src_prepare() {
    default_src_prepare
    cp "${CONFIG_DIR}/config.sub" config.sub
    cp "${CONFIG_DIR}/config.guess" config.guess
}

src_configure() {
    step mpc configure ./configure --prefix="${PREFIX}" \
        --build="${TARGET}" --host="${TARGET}" \
        --disable-shared --enable-static \
        --with-gmp="${PREFIX}" --with-mpfr="${PREFIX}" \
        CC=tcc MAKEINFO=true
}

src_compile() {
    step mpc make make "${MAKEJOBS}" MAKEINFO=true
    check mpc
}

src_install() {
    step mpc install make install MAKEINFO=true
}
