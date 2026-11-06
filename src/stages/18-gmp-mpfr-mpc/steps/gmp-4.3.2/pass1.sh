# Rung gmp (18-gmp-mpfr-mpc README): gmp 4.3.2 with its release
# tarball's configure, generic C (CPU "none"), static only.

src_prepare() {
    default_src_prepare
    # gmp's config.sub/config.guess are wrappers (gmp's CPU names) around
    # configfsf.sub/configfsf.guess: those two from binutils 2.30
    cp "${CONFIG_DIR}/config.sub" configfsf.sub
    cp "${CONFIG_DIR}/config.guess" configfsf.guess
}

src_configure() {
    # gmp 4.3.2 has no --disable-assembly; CPU "none" is its generic C
    # build (gmp manual, "Build Options"): mpn/generic only, ABI "long"
    # (64-bit limb). --build differs from --host, so configure takes it as
    # a cross build: CC_FOR_BUILD for the gen-* table programs, with
    # -static as every program here
    step gmp configure ./configure --prefix="${PREFIX}" \
        --build="${TARGET}" --host=none-unknown-linux-musl \
        --disable-shared --enable-static \
        CC=tcc CC_FOR_BUILD="tcc -static" MAKEINFO=true
}

src_compile() {
    step gmp make make "${MAKEJOBS}" MAKEINFO=true
    check gmp
}

src_install() {
    step gmp install make install MAKEINFO=true
}
