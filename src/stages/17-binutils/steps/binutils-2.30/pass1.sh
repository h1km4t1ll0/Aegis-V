# Rung binutils (17-binutils README): binutils 2.30 with its release
# tarball's top-level configure. Not taken over from live-bootstrap's
# steps/binutils-2.30/pass1.sh: its src_prepare (removing and
# regenerating the bison/flex output, opcodes tables, bfd headers, info
# pages and po files, autoreconf of every directory) and its patches
# (they are for autoreconf); the generated files of the tarball are used
# as they are. Its configure of each directory on its own becomes the
# plan's one top-level configure.

# One step with its output in /build/logs/NAME.log: its last two lines
# and `binutils NAME ok`, or `binutils NAME FAIL`, the tail of the log and
# of the newest config.log, and exit 1.
bu_step() {
    name=$1
    shift
    if "$@" > "/build/logs/${name}.log" 2>&1; then
        tail -2 "/build/logs/${name}.log"
        echo "binutils ${name} ok"
    else
        echo "binutils ${name} FAIL"
        tail -50 "/build/logs/${name}.log"
        log=$(ls -t config.log */config.log 2>/dev/null | head -1)
        if [ -n "${log}" ]; then
            echo "--- ${log}"
            tail -50 "${log}"
        fi
        exit 1
    fi
}

# make's variables for every step: no info pages (no makeinfo), tcc's ar
# (the Makefiles say AR = ar), no ranlib before ours (tcc -ar writes the
# symbol index), and live-bootstrap's tooldir: /usr/bin and /usr/lib
# instead of /usr/riscv64-unknown-linux-musl/{bin,lib}
bu_make() {
    make "${MAKEJOBS}" MAKEINFO=true AR="tcc -ar" RANLIB=true tooldir="${PREFIX}" "$@"
}

src_configure() {
    # The plan's configure line. tcc-m without -static writes a dynamic
    # executable (14-seed README), which Tilck refuses to run. libtool,
    # which links every program here, takes -static as its own flag, from
    # CC="tcc -static" and from LDFLAGS alike, and drops it from the tcc
    # command (the first gate run installed dynamic as, ld, ...);
    # -Wc,-static is passed on to tcc (tcc ignores it in configure's
    # tests). LD=tcc: libtool's LT_PATH_LD finds no ld otherwise.
    # lt_cv_sys_max_cmd_len as live-bootstrap (skips libtool's probe;
    # tcc -ar must get every object in one command).
    export LDFLAGS="-static -Wc,-static"
    export lt_cv_sys_max_cmd_len=32768
    bu_step configure ./configure --prefix="${PREFIX}" \
        --build="${TARGET}" --host="${TARGET}" --target="${TARGET}" \
        --disable-nls --disable-werror --disable-shared --enable-static \
        --disable-gold --disable-plugins --enable-deterministic-archives \
        CC="tcc -static" AR="tcc -ar" RANLIB=true LD=tcc MAKEINFO=true
}

src_compile() {
    # the libraries and programs one at a time (each configures its
    # directory first), then whatever `all` has left (gprof, intl, etc)
    for dir in libiberty zlib bfd opcodes binutils gas ld; do
        bu_step "${dir}" bu_make "all-${dir}"
    done
    bu_step all bu_make all
}

src_install() {
    bu_step install bu_make install
}
