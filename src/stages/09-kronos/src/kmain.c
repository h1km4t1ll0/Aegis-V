#include "k.h"

static char *av_init[2];
static char *av_ver[3];
static char *av_tcc2[16];
static char *av_ver2[3];
static char *av_tcc3[16];
static char *av_ver3[3];
static char *av_tcc4[16];
static char *av_cmp[4];
static char *av_tcc5[20];
static char *av_ver5[3];
static char *av_tcc6[20];
static char *av_ver6[3];
static char *av_hello6[16];
static char *av_runhello[2];
static char *av_zext6[13];
static char *av_runzext6[2];
static char *av_tcc6b[20];
static char *av_tcc6c[20];
static char *av_cmp66[4];
static char *av_tcc7[20];
static char *av_tcc7b[20];
static char *av_tcc7c[20];
static char *av_cmp77[4];
static char *av_hello7[13];
static char *av_runhello7[2];
static char *av_fl6[13];
static char *av_runfl6[2];
static char *av_t7cc_floatlit[14];
static char *av_t7run_floatlit[2];
static char *av_t7cc_kconst[14];
static char *av_t7run_kconst[2];
static char *av_t7cc_priv[14];
static char *av_t7run_priv[2];
static char *av_t7cc_macro[14];
static char *av_t7run_macro[2];
static char *av_t7cc_penum[14];
static char *av_t7run_penum[2];
static char *av_t7cc_predefs[14];
static char *av_t7run_predefs[2];
static char *av_t7cc_gasdir[14];
static char *av_t7run_gasdir[2];
static char *av_t7cc_bssym[14];
static char *av_t7run_bssym[2];
static char *av_t7cc_atomicn[14];
static char *av_t7run_atomicn[2];
static char *av_t7cc_nopic[15];
static char *av_t7run_nopic[2];
static char *av_t7cc_branch[14];
static char *av_t7run_branch[2];
static char *av_t7cc_cstore[14];
static char *av_t7run_cstore[2];
static char *av_t7cc_luiext[14];
static char *av_t7run_luiext[2];
static char *av_t7cc_epilog[14];
static char *av_t7run_epilog[2];
static char *av_arl_cc[7];
static char *av_arl_ar[6];
static char *av_t7cc_arlink[14];
static char *av_t7run_arlink[2];
static char *av_hinst_cc[14];
static char *av_hinst[2];
static char *av_uapi_cc[16];
static char *av_uapi[2];
static char *av_gencfg_cc[14];
static char *av_gencfg[2];
static char *av_tobj[9];
static char *av_tlink[3];
static char *av_sbi[3];
static char *av_tinit[14];
static char *av_tproc[14];
static char *av_tchild[14];
static char *av_mgen_cc[14];
static char *av_mgen[4];
static char *av_mar[3];
static char *av_mcrt1[9];
static char *av_mcrti[9];
static char *av_mcrtn[9];
static char *av_mhello[18];
static char *av_mproc[18];
static char *av_mmem[18];
static char *av_mhard[18];
static char *av_mtar_cc[14];
static char *av_mtar[6];
static char *av_sar[3];
static char *av_sprog[3];
static char *av_stree_cc[14];
static char *av_stree[13];
static char *av_star[6];
static char sp_rsp[256];
static char tl_src[256];
static char tl_dir[256];
static char tl_obj[256];

static void snap_file(const char *path, const char *tag)
{
    void *p;
    unsigned long n;

    p = kfs_get(path, &n);
    if (!p)
        return;
    uart_puts("snap ");
    uart_puts(tag);
    uart_puts(" ");
    uart_hex((unsigned long)p);
    uart_puts(" ");
    uart_hex(n);
    uart_puts("\n");
}

/* tcc7b build line for a tcc7 test (src2 may be 0).  -static: Kronos
   applies no dynamic relocations, so a GOT entry (the address of an extern
   symbol, e.g. priv_words) must be filled at link time. */
static void t7_cc(char **av, char *out, char *src1, char *src2)
{
    av[0] = "/bin/tcc7b";
    av[1] = "-o";
    av[2] = out;
    av[3] = "-I";
    av[4] = "/include";
    av[5] = "-nostdlib";
    av[6] = "-static";
    av[7] = "-Wl,-Ttext=0x10000";
    av[8] = "-Wl,-oformat=elf64-little";
    av[9] = "/src/tcc7/compat-crt.s";
    av[10] = "/lib/libc.c";
    av[11] = src1;
    av[12] = src2;
    av[13] = 0;
}

/* build, then run; a failing test does not stop the next one */
static void t7_test(char *name, char **cc, char **rv)
{
    unsigned long len;

    run(cc[0], cc);
    if (!kfs_get(rv[0], &len)) {
        uart_puts(name);
        uart_puts(" build fail\n");
        return;
    }
    run(rv[0], rv);
    if (proc_last_exit() != 0) {
        uart_puts(name);
        uart_puts(" fail\n");
    }
}

static unsigned long tl_cat(char *d, unsigned long n, const char *s)
{
    while (*s && n < 255) {
        d[n] = *s;
        n = n + 1;
        s = s + 1;
    }
    d[n] = 0;
    return n;
}

/* One tcc7b @cflags -c per line of the list: Tilck has three misc.c, so
   each source gets its own object in objroot, named after its path (/ ->
   __). A line starting with '/' is a KFS path (tcc's runtime atomics);
   others are under srcroot. Every failure prints "<tag> obj fail <path>"
   before returning 0; all built: "<tag> objs ok". */
static int build_objs(char *list, char *cflags, char *srcroot, char *objroot, char *tag)
{
    char *p;
    char *e;
    unsigned long n;
    unsigned long i;
    unsigned long j;
    unsigned long k;
    unsigned long len;
    int ok;

    av_tobj[1] = cflags;
    p = kfs_get(list, &n);
    if (!p) {
        uart_puts(tag);
        uart_puts(" obj fail ");
        uart_puts(list);
        uart_puts("\n");
        return 0;
    }
    e = p + n;
    ok = 1;
    while (p < e) {
        i = 0;
        while (p + i < e && p[i] != '\n')
            i = i + 1;
        if (i > 0 && i < 160) {
            k = 0;
            if (p[0] != '/')
                k = tl_cat(tl_src, 0, srcroot);
            memcpy(tl_src + k, p, i);
            tl_src[k + i] = 0;
            memcpy(tl_dir, tl_src, k + i + 1);
            k = k + i;
            while (k > 0 && tl_dir[k] != '/')
                k = k - 1;
            tl_dir[k] = 0;
            k = tl_cat(tl_obj, 0, objroot);
            j = 0;
            while (j < i) {
                if (p[j] == '/') {
                    tl_obj[k] = '_';
                    tl_obj[k + 1] = '_';
                    k = k + 2;
                } else {
                    tl_obj[k] = p[j];
                    k = k + 1;
                }
                j = j + 1;
            }
            tl_obj[k] = 0;
            tl_cat(tl_obj, k, ".o");
            run(av_tobj[0], av_tobj);
            if (proc_last_exit() != 0 || !kfs_get(tl_obj, &len)) {
                uart_puts(tag);
                uart_puts(" obj fail ");
                uart_puts(tl_src);
                uart_puts("\n");
                ok = 0;
            }
        }
        p = p + i + 1;
    }
    if (ok) {
        uart_puts(tag);
        uart_puts(" objs ok\n");
    }
    return ok;
}

/* crt*.o: musl's Makefile adds -DCRT for them */
static void musl_crt(char **av, char *src, char *out)
{
    av[0] = "/bin/tcc7b";
    av[1] = "@/src/musl/cflags.rsp";
    av[2] = "-DCRT";
    av[3] = "-c";
    av[4] = src;
    av[5] = "-o";
    av[6] = out;
    av[7] = 0;
}

/* a static Linux riscv64 program on the guest-built musl (for Tilck);
   tcc's lib-arm64.c gives the binary128 helpers (__addtf3, ...) that
   musl's long double code calls, libgcc's part in a GCC build */
static void musl_cc(char **av, char *out, char *src)
{
    av[0] = "/bin/tcc7b";
    av[1] = "-o";
    av[2] = out;
    av[3] = "-nostdinc";
    av[4] = "-I";
    av[5] = "/build/musl/include";
    av[6] = "-I";
    av[7] = "/lib/tcc7/include";
    av[8] = "-nostdlib";
    av[9] = "-static";
    av[10] = "-Wl,-Ttext=0x10000";
    av[11] = "/build/musl/lib/crt1.o";
    av[12] = "/build/musl/lib/crti.o";
    av[13] = src;
    av[14] = "/src/tcc7/lib/lib-arm64.c";
    av[15] = "/build/musl/lib/libc.a";
    av[16] = "/build/musl/lib/crtn.o";
    av[17] = 0;
}

/* Stage 13: musl 1.1.24 (/src/musl: the pinned archive and 13-musl/src).
   musl-gen writes the generated and installed headers, then one object
   per SOURCES line, libc.a, crt*.o; the two test programs for Tilck and
   musl-sysroot.tar are snapped. */
static void build_musl(void)
{
    unsigned long len;

    t7_test("musl-gen", av_mgen_cc, av_mgen);
    if (!build_objs("/src/musl/SOURCES", "@/src/musl/cflags.rsp",
                    "/src/musl/", "/build/musl/obj/", "musl"))
        return;
    run(av_mar[0], av_mar);
    run(av_mcrt1[0], av_mcrt1);
    run(av_mcrti[0], av_mcrti);
    run(av_mcrtn[0], av_mcrtn);
    if (!kfs_get("/build/musl/lib/libc.a", &len)
        || !kfs_get("/build/musl/lib/crt1.o", &len)
        || !kfs_get("/build/musl/lib/crti.o", &len)
        || !kfs_get("/build/musl/lib/crtn.o", &len)) {
        uart_puts("musl lib fail\n");
        return;
    }
    uart_puts("musl built\n");
    run(av_mhello[0], av_mhello);
    run(av_mproc[0], av_mproc);
    run(av_mmem[0], av_mmem);
    run(av_mhard[0], av_mhard);
    if (kfs_get("/build/musl/hello-musl", &len)
        && kfs_get("/build/musl/musl-proc", &len)
        && kfs_get("/build/tilck/mem", &len)
        && kfs_get("/build/tilck/hard", &len)) {
        uart_puts("musl progs built\n");
        snap_file("/build/musl/hello-musl", "mhello");
        snap_file("/build/musl/musl-proc", "mproc");
        snap_file("/build/tilck/mem", "tmem");
        snap_file("/build/tilck/hard", "thard");
    } else
        uart_puts("musl progs fail\n");
    t7_test("mkustar", av_mtar_cc, av_mtar);
    snap_file("/build/musl/musl-sysroot.tar", "msysroot");
}

/* Stage 14: one "/bin/tcc7b @RSP" per line of the list (each listfile is
   a whole command line). Every failure prints "<tag> fail <rsp>"; all
   built: "<tag> ok". */
static int run_rsps(char *list, char *tag)
{
    char *p;
    char *e;
    unsigned long n;
    unsigned long i;
    int ok;

    p = kfs_get(list, &n);
    if (!p) {
        uart_puts(tag);
        uart_puts(" fail ");
        uart_puts(list);
        uart_puts("\n");
        return 0;
    }
    e = p + n;
    ok = 1;
    while (p < e) {
        i = 0;
        while (p + i < e && p[i] != '\n')
            i = i + 1;
        if (i > 0 && i < 250) {
            sp_rsp[0] = '@';
            memcpy(sp_rsp + 1, p, i);
            sp_rsp[i + 1] = 0;
            run(av_sprog[0], av_sprog);
            if (proc_last_exit() != 0) {
                uart_puts(tag);
                uart_puts(" fail ");
                uart_puts(sp_rsp + 1);
                uart_puts("\n");
                ok = 0;
            }
        }
        p = p + i + 1;
    }
    if (ok) {
        uart_puts(tag);
        uart_puts(" ok\n");
    }
    return ok;
}

/* Stage 14: the seed for Tilck (/src/seed: 14-seed/src and the pinned
   archives). libtcc1.a, then tcc-m, kaem, the tools and rung-init (musl,
   static), the tree under /build/seed and seed-rootfs.tar; rung-init and
   the tar are snapped. */
static void build_seed(void)
{
    unsigned long len;

    if (!build_objs("/src/seed/LIBTCC1", "@/src/seed/libtcc1.rsp",
                    "/src/tcc7/lib/", "/build/seed/obj/", "libtcc1"))
        return;
    run(av_sar[0], av_sar);
    if (!kfs_get("/build/seed/usr/lib/tcc/libtcc1.a", &len)) {
        uart_puts("libtcc1 fail\n");
        return;
    }
    uart_puts("libtcc1 built\n");
    if (!run_rsps("/src/seed/SEED", "seed progs"))
        return;
    snap_file("/build/seed/rung-init", "rinit");
    t7_test("seed-tree", av_stree_cc, av_stree);
    if (!kfs_get("/build/seed/LIST", &len))
        return;
    run(av_star[0], av_star);
    if (proc_last_exit() == 0 && kfs_get("/build/seed/seed-rootfs.tar", &len)) {
        uart_puts("seed built\n");
        snap_file("/build/seed/seed-rootfs.tar", "seed");
    } else
        uart_puts("seed tar fail\n");
}

static void setup_args(void)
{
    av_init[0] = "/bin/init";
    av_init[1] = 0;
    av_ver[0] = "/bin/tcc";
    av_ver[1] = "-version";
    av_ver[2] = 0;
    av_tcc2[0] = "/bin/tcc";
    av_tcc2[1] = "-o";
    av_tcc2[2] = "/bin/tcc2";
    av_tcc2[3] = "-DONE_SOURCE";
    av_tcc2[4] = "-DTCC_TARGET_RISCV64";
    av_tcc2[5] = "-I";
    av_tcc2[6] = "/include";
    av_tcc2[7] = "-nostdlib";
    av_tcc2[8] = "-Wl,-Ttext=0x10000";
    av_tcc2[9] = "-Wl,-oformat=elf64-little";
    av_tcc2[10] = "/lib/crt.s";
    av_tcc2[11] = "/lib/libc.c";
    av_tcc2[12] = "/src/tcc/tcc.c";
    av_tcc2[13] = 0;
    av_ver2[0] = "/bin/tcc2";
    av_ver2[1] = "-version";
    av_ver2[2] = 0;
    av_tcc3[0] = "/bin/tcc2";
    av_tcc3[1] = "-o";
    av_tcc3[2] = "/bin/tcc3";
    av_tcc3[3] = "-DONE_SOURCE";
    av_tcc3[4] = "-DTCC_TARGET_RISCV64";
    av_tcc3[5] = "-I";
    av_tcc3[6] = "/include";
    av_tcc3[7] = "-nostdlib";
    av_tcc3[8] = "-Wl,-Ttext=0x10000";
    av_tcc3[9] = "-Wl,-oformat=elf64-little";
    av_tcc3[10] = "/lib/crt.s";
    av_tcc3[11] = "/lib/libc.c";
    av_tcc3[12] = "/src/tcc/tcc.c";
    av_tcc3[13] = 0;
    av_ver3[0] = "/bin/tcc3";
    av_ver3[1] = "-version";
    av_ver3[2] = 0;
    av_tcc4[0] = "/bin/tcc3";
    av_tcc4[1] = "-o";
    av_tcc4[2] = "/bin/tcc4";
    av_tcc4[3] = "-DONE_SOURCE";
    av_tcc4[4] = "-DTCC_TARGET_RISCV64";
    av_tcc4[5] = "-I";
    av_tcc4[6] = "/include";
    av_tcc4[7] = "-nostdlib";
    av_tcc4[8] = "-Wl,-Ttext=0x10000";
    av_tcc4[9] = "-Wl,-oformat=elf64-little";
    av_tcc4[10] = "/lib/crt.s";
    av_tcc4[11] = "/lib/libc.c";
    av_tcc4[12] = "/src/tcc/tcc.c";
    av_tcc4[13] = 0;
    av_cmp[0] = "/bin/cmp";
    av_cmp[1] = "/bin/tcc3";
    av_cmp[2] = "/bin/tcc4";
    av_cmp[3] = 0;
    av_tcc5[0] = "/bin/tcc4";
    av_tcc5[1] = "-o";
    av_tcc5[2] = "/bin/tcc5";
    av_tcc5[3] = "-DONE_SOURCE";
    av_tcc5[4] = "-DTCC_TARGET_RISCV64";
    av_tcc5[5] = "-DHAVE_FLOAT=1";
    av_tcc5[6] = "-DHAVE_LONG_LONG=1";
    av_tcc5[7] = "-I";
    av_tcc5[8] = "/src/tcc-full";
    av_tcc5[9] = "-I";
    av_tcc5[10] = "/include";
    av_tcc5[11] = "-nostdlib";
    av_tcc5[12] = "-Wl,-Ttext=0x10000";
    av_tcc5[13] = "-Wl,-oformat=elf64-little";
    av_tcc5[14] = "/lib/crt.s";
    av_tcc5[15] = "/lib/libc.c";
    av_tcc5[16] = "/src/tcc-full/tcc.c";
    av_tcc5[17] = 0;
    av_ver5[0] = "/bin/tcc5";
    av_ver5[1] = "-version";
    av_ver5[2] = 0;
    av_tcc6[0] = "/bin/tcc5";
    av_tcc6[1] = "-o";
    av_tcc6[2] = "/bin/tcc6";
    av_tcc6[3] = "-DONE_SOURCE";
    av_tcc6[4] = "-DTCC_TARGET_RISCV64";
    av_tcc6[5] = "-DHAVE_FLOAT=1";
    av_tcc6[6] = "-DHAVE_LONG_LONG=1";
    av_tcc6[7] = "-I";
    av_tcc6[8] = "/src/tcc-full";
    av_tcc6[9] = "-I";
    av_tcc6[10] = "/include";
    av_tcc6[11] = "-nostdlib";
    av_tcc6[12] = "-Wl,-Ttext=0x10000";
    av_tcc6[13] = "-Wl,-oformat=elf64-little";
    av_tcc6[14] = "/lib/crt.s";
    av_tcc6[15] = "/lib/libc.c";
    av_tcc6[16] = "/src/tcc-full/tcc.c";
    av_tcc6[17] = 0;
    av_ver6[0] = "/bin/tcc6";
    av_ver6[1] = "-version";
    av_ver6[2] = 0;
    av_hello6[0] = "/bin/tcc6";
    av_hello6[1] = "-o";
    av_hello6[2] = "/bin/hello6";
    av_hello6[3] = "-I";
    av_hello6[4] = "/include";
    av_hello6[5] = "-nostdlib";
    av_hello6[6] = "-Wl,-Ttext=0x10000";
    av_hello6[7] = "-Wl,-oformat=elf64-little";
    av_hello6[8] = "/lib/crt.s";
    av_hello6[9] = "/lib/libc.c";
    av_hello6[10] = "/src/hello/hello.c";
    av_hello6[11] = 0;
    av_runhello[0] = "/bin/hello6";
    av_runhello[1] = 0;
    av_zext6[0] = "/bin/tcc6";
    av_zext6[1] = "-o";
    av_zext6[2] = "/bin/zext6";
    av_zext6[3] = "-I";
    av_zext6[4] = "/include";
    av_zext6[5] = "-nostdlib";
    av_zext6[6] = "-Wl,-Ttext=0x10000";
    av_zext6[7] = "-Wl,-oformat=elf64-little";
    av_zext6[8] = "/lib/crt.s";
    av_zext6[9] = "/lib/libc.c";
    av_zext6[10] = "/src/hello/mob-zext.c";
    av_zext6[11] = 0;
    av_runzext6[0] = "/bin/zext6";
    av_runzext6[1] = 0;
    av_tcc6b[0] = "/bin/tcc6";
    av_tcc6b[1] = "-o";
    av_tcc6b[2] = "/bin/tcc6b";
    av_tcc6b[3] = "-DONE_SOURCE";
    av_tcc6b[4] = "-DTCC_TARGET_RISCV64";
    av_tcc6b[5] = "-DHAVE_FLOAT=1";
    av_tcc6b[6] = "-DHAVE_LONG_LONG=1";
    av_tcc6b[7] = "-I";
    av_tcc6b[8] = "/src/tcc-full";
    av_tcc6b[9] = "-I";
    av_tcc6b[10] = "/include";
    av_tcc6b[11] = "-nostdlib";
    av_tcc6b[12] = "-Wl,-Ttext=0x10000";
    av_tcc6b[13] = "-Wl,-oformat=elf64-little";
    av_tcc6b[14] = "/lib/crt.s";
    av_tcc6b[15] = "/lib/libc.c";
    av_tcc6b[16] = "/src/tcc-full/tcc.c";
    av_tcc6b[17] = 0;
    av_tcc6c[0] = "/bin/tcc6b";
    av_tcc6c[1] = "-o";
    av_tcc6c[2] = "/bin/tcc6c";
    av_tcc6c[3] = "-DONE_SOURCE";
    av_tcc6c[4] = "-DTCC_TARGET_RISCV64";
    av_tcc6c[5] = "-DHAVE_FLOAT=1";
    av_tcc6c[6] = "-DHAVE_LONG_LONG=1";
    av_tcc6c[7] = "-I";
    av_tcc6c[8] = "/src/tcc-full";
    av_tcc6c[9] = "-I";
    av_tcc6c[10] = "/include";
    av_tcc6c[11] = "-nostdlib";
    av_tcc6c[12] = "-Wl,-Ttext=0x10000";
    av_tcc6c[13] = "-Wl,-oformat=elf64-little";
    av_tcc6c[14] = "/lib/crt.s";
    av_tcc6c[15] = "/lib/libc.c";
    av_tcc6c[16] = "/src/tcc-full/tcc.c";
    av_tcc6c[17] = 0;
    av_cmp66[0] = "/bin/cmp";
    av_cmp66[1] = "/bin/tcc6b";
    av_cmp66[2] = "/bin/tcc6c";
    av_cmp66[3] = 0;
    av_tcc7[0] = "/bin/tcc6";
    av_tcc7[1] = "-o";
    av_tcc7[2] = "/bin/tcc7";
    av_tcc7[3] = "-DONE_SOURCE=1";
    av_tcc7[4] = "-I";
    av_tcc7[5] = "/src/tcc7";
    av_tcc7[6] = "-I";
    av_tcc7[7] = "/src/tcc7/compat-include";
    av_tcc7[8] = "-I";
    av_tcc7[9] = "/include";
    av_tcc7[10] = "-nostdlib";
    av_tcc7[11] = "-Wl,-Ttext=0x10000";
    av_tcc7[12] = "-Wl,-oformat=elf64-little";
    av_tcc7[13] = "/lib/crt.s";
    av_tcc7[14] = "/lib/libc.c";
    av_tcc7[15] = "/src/tcc7/compat.c";
    av_tcc7[16] = "/src/tcc7/tcc.c";
    av_tcc7[17] = 0;
    av_tcc7b[0] = "/bin/tcc7";
    av_tcc7b[1] = "-o";
    av_tcc7b[2] = "/bin/tcc7b";
    av_tcc7b[3] = "-DONE_SOURCE=1";
    av_tcc7b[4] = "-I";
    av_tcc7b[5] = "/src/tcc7";
    av_tcc7b[6] = "-I";
    av_tcc7b[7] = "/src/tcc7/compat-include";
    av_tcc7b[8] = "-I";
    av_tcc7b[9] = "/include";
    av_tcc7b[10] = "-nostdlib";
    av_tcc7b[11] = "-static";
    av_tcc7b[12] = "-Wl,-Ttext=0x10000";
    av_tcc7b[13] = "-Wl,-oformat=elf64-little";
    av_tcc7b[14] = "/src/tcc7/compat-crt.s";
    av_tcc7b[15] = "/lib/libc.c";
    av_tcc7b[16] = "/src/tcc7/compat.c";
    av_tcc7b[17] = "/src/tcc7/tcc.c";
    av_tcc7b[18] = 0;
    av_tcc7c[0] = "/bin/tcc7b";
    av_tcc7c[1] = "-o";
    av_tcc7c[2] = "/bin/tcc7c";
    av_tcc7c[3] = "-DONE_SOURCE=1";
    av_tcc7c[4] = "-I";
    av_tcc7c[5] = "/src/tcc7";
    av_tcc7c[6] = "-I";
    av_tcc7c[7] = "/src/tcc7/compat-include";
    av_tcc7c[8] = "-I";
    av_tcc7c[9] = "/include";
    av_tcc7c[10] = "-nostdlib";
    av_tcc7c[11] = "-static";
    av_tcc7c[12] = "-Wl,-Ttext=0x10000";
    av_tcc7c[13] = "-Wl,-oformat=elf64-little";
    av_tcc7c[14] = "/src/tcc7/compat-crt.s";
    av_tcc7c[15] = "/lib/libc.c";
    av_tcc7c[16] = "/src/tcc7/compat.c";
    av_tcc7c[17] = "/src/tcc7/tcc.c";
    av_tcc7c[18] = 0;
    av_cmp77[0] = "/bin/cmp";
    av_cmp77[1] = "/bin/tcc7b";
    av_cmp77[2] = "/bin/tcc7c";
    av_cmp77[3] = 0;
    av_hello7[0] = "/bin/tcc7b";
    av_hello7[1] = "-o";
    av_hello7[2] = "/bin/hello7";
    av_hello7[3] = "-I";
    av_hello7[4] = "/include";
    av_hello7[5] = "-nostdlib";
    av_hello7[6] = "-Wl,-Ttext=0x10000";
    av_hello7[7] = "-Wl,-oformat=elf64-little";
    av_hello7[8] = "/src/tcc7/compat-crt.s";
    av_hello7[9] = "/lib/libc.c";
    av_hello7[10] = "/src/hello7/hello.c";
    av_hello7[11] = 0;
    av_runhello7[0] = "/bin/hello7";
    av_runhello7[1] = 0;
    av_fl6[0] = "/bin/tcc6";
    av_fl6[1] = "-o";
    av_fl6[2] = "/bin/floatlit6";
    av_fl6[3] = "-DFLOATLIT6";
    av_fl6[4] = "-I";
    av_fl6[5] = "/include";
    av_fl6[6] = "-nostdlib";
    av_fl6[7] = "-Wl,-Ttext=0x10000";
    av_fl6[8] = "-Wl,-oformat=elf64-little";
    av_fl6[9] = "/lib/crt.s";
    av_fl6[10] = "/lib/libc.c";
    av_fl6[11] = "/src/t7/floatlit.c";
    av_fl6[12] = 0;
    av_runfl6[0] = "/bin/floatlit6";
    av_runfl6[1] = 0;
    t7_cc(av_t7cc_floatlit, "/bin/t7-floatlit", "/src/t7/floatlit.c", 0);
    av_t7run_floatlit[0] = "/bin/t7-floatlit";
    av_t7run_floatlit[1] = 0;
    t7_cc(av_t7cc_kconst, "/bin/t7-kconst", "/src/t7/kconst.c", 0);
    av_t7run_kconst[0] = "/bin/t7-kconst";
    av_t7run_kconst[1] = 0;
    t7_cc(av_t7cc_priv, "/bin/t7-priv", "/src/t7/priv.c", 0);
    av_t7run_priv[0] = "/bin/t7-priv";
    av_t7run_priv[1] = 0;
    t7_cc(av_t7cc_macro, "/bin/t7-macro", "/src/t7/macro.S", "/src/t7/macro-main.c");
    av_t7run_macro[0] = "/bin/t7-macro";
    av_t7run_macro[1] = 0;
    t7_cc(av_t7cc_penum, "/bin/t7-penum", "/src/t7/penum.c", 0);
    av_t7run_penum[0] = "/bin/t7-penum";
    av_t7run_penum[1] = 0;
    t7_cc(av_t7cc_predefs, "/bin/t7-predefs", "/src/t7/predefs.c", 0);
    av_t7run_predefs[0] = "/bin/t7-predefs";
    av_t7run_predefs[1] = 0;
    t7_cc(av_t7cc_gasdir, "/bin/t7-gasdir", "/src/t7/gasdir.S", "/src/t7/gasdir-main.c");
    av_t7run_gasdir[0] = "/bin/t7-gasdir";
    av_t7run_gasdir[1] = 0;
    t7_cc(av_t7cc_bssym, "/bin/t7-bssym", "/src/t7/bssym.c", 0);
    av_t7run_bssym[0] = "/bin/t7-bssym";
    av_t7run_bssym[1] = 0;
    t7_cc(av_t7cc_atomicn, "/bin/t7-atomicn", "/src/t7/atomicn.c", "/src/tcc7/lib/atomic.S");
    av_t7run_atomicn[0] = "/bin/t7-atomicn";
    av_t7run_atomicn[1] = 0;
    /* -fno-pic: globals PC-relative, no GOT (Tilck runs C before paging) */
    t7_cc(av_t7cc_nopic, "/bin/t7-nopic", "/src/t7/nopic.c", "/src/t7/nopic-ext.c");
    av_t7cc_nopic[13] = "-fno-pic";
    av_t7cc_nopic[14] = 0;
    av_t7run_nopic[0] = "/bin/t7-nopic";
    av_t7run_nopic[1] = 0;
    t7_cc(av_t7cc_branch, "/bin/t7-branch", "/src/t7/branch.S", "/src/t7/branch-main.c");
    av_t7run_branch[0] = "/bin/t7-branch";
    av_t7run_branch[1] = 0;
    t7_cc(av_t7cc_cstore, "/bin/t7-cstore", "/src/t7/cstore.c", 0);
    av_t7run_cstore[0] = "/bin/t7-cstore";
    av_t7run_cstore[1] = 0;
    t7_cc(av_t7cc_luiext, "/bin/t7-luiext", "/src/t7/luiext.c", 0);
    av_t7run_luiext[0] = "/bin/t7-luiext";
    av_t7run_luiext[1] = 0;
    t7_cc(av_t7cc_epilog, "/bin/t7-epilog", "/src/t7/epilog.c", 0);
    av_t7run_epilog[0] = "/bin/t7-epilog";
    av_t7run_epilog[1] = 0;
    /* tcc -ar: an archive made on Kronos, then linked against */
    av_arl_cc[0] = "/bin/tcc7b";
    av_arl_cc[1] = "-c";
    av_arl_cc[2] = "/src/t7/arlink-lib.c";
    av_arl_cc[3] = "-o";
    av_arl_cc[4] = "/build/t7/arlink-lib.o";
    av_arl_cc[5] = 0;
    av_arl_ar[0] = "/bin/tcc7b";
    av_arl_ar[1] = "-ar";
    av_arl_ar[2] = "rcs";
    av_arl_ar[3] = "/build/t7/libarlink.a";
    av_arl_ar[4] = "/build/t7/arlink-lib.o";
    av_arl_ar[5] = 0;
    t7_cc(av_t7cc_arlink, "/bin/t7-arlink", "/src/t7/arlink-main.c", "/build/t7/libarlink.a");
    av_t7run_arlink[0] = "/bin/t7-arlink";
    av_t7run_arlink[1] = 0;
    /* Tilck stage: our tools and the uapi test are Kronos programs */
    t7_cc(av_hinst_cc, "/bin/headers-install", "/src/tilck-tools/headers-install.c", 0);
    av_hinst[0] = "/bin/headers-install";
    av_hinst[1] = 0;
    av_uapi_cc[0] = "/bin/tcc7b";
    av_uapi_cc[1] = "-o";
    av_uapi_cc[2] = "/bin/uapi-check";
    av_uapi_cc[3] = "-I";
    av_uapi_cc[4] = "/usr/include";
    av_uapi_cc[5] = "-I";
    av_uapi_cc[6] = "/include";
    av_uapi_cc[7] = "-nostdlib";
    av_uapi_cc[8] = "-static";
    av_uapi_cc[9] = "-Wl,-Ttext=0x10000";
    av_uapi_cc[10] = "-Wl,-oformat=elf64-little";
    av_uapi_cc[11] = "/src/tcc7/compat-crt.s";
    av_uapi_cc[12] = "/lib/libc.c";
    av_uapi_cc[13] = "/src/tilck-tests/uapi-check.c";
    av_uapi_cc[14] = 0;
    av_uapi[0] = "/bin/uapi-check";
    av_uapi[1] = 0;
    t7_cc(av_gencfg_cc, "/bin/gen-tilck-config", "/src/tilck-tools/gen-tilck-config.c", 0);
    av_gencfg[0] = "/bin/gen-tilck-config";
    av_gencfg[1] = 0;
    av_tobj[0] = "/bin/tcc7b";
    av_tobj[1] = "@/src/tilck/cflags.rsp";
    av_tobj[2] = "-I";
    av_tobj[3] = tl_dir;
    av_tobj[4] = "-c";
    av_tobj[5] = tl_src;
    av_tobj[6] = "-o";
    av_tobj[7] = tl_obj;
    av_tobj[8] = 0;
    av_tlink[0] = "/bin/tcc7b";
    av_tlink[1] = "@/src/tilck/link.rsp";
    av_tlink[2] = 0;
    av_sbi[0] = "/bin/tcc7b";
    av_sbi[1] = "@/src/tilck/sbi/sbi.rsp";
    av_sbi[2] = 0;
    /* programs for Tilck's initrd: Linux ABI, Kronos libc + ecall */
    t7_cc(av_tinit, "/build/tilck/init", "/src/tilck/init.c", 0);
    t7_cc(av_tproc, "/build/tilck/proc", "/src/tilck-tests/proc.c", 0);
    t7_cc(av_tchild, "/build/tilck/child", "/src/tilck-tests/child.c", 0);
    /* musl: our tools are Kronos programs */
    t7_cc(av_mgen_cc, "/bin/musl-gen", "/src/musl-tools/musl-gen.c", 0);
    av_mgen[0] = "/bin/musl-gen";
    av_mgen[1] = "/src/musl";
    av_mgen[2] = "/build/musl";
    av_mgen[3] = 0;
    /* @file before -ar: mob expands listfiles only up to "-ar" */
    av_mar[0] = "/bin/tcc7b";
    av_mar[1] = "@/src/musl/ar.rsp";
    av_mar[2] = 0;
    musl_crt(av_mcrt1, "/src/musl/crt/crt1.c", "/build/musl/lib/crt1.o");
    musl_crt(av_mcrti, "/src/musl/crt/crti.c", "/build/musl/lib/crti.o");
    musl_crt(av_mcrtn, "/src/musl/crt/crtn.c", "/build/musl/lib/crtn.o");
    musl_cc(av_mhello, "/build/musl/hello-musl", "/src/musl-tests/hello-musl.c");
    musl_cc(av_mproc, "/build/musl/musl-proc", "/src/musl-tests/musl-proc.c");
    /* stage 12's memory test is a musl program too */
    musl_cc(av_mmem, "/build/tilck/mem", "/src/tilck-tests/mem.c");
    musl_cc(av_mhard, "/build/tilck/hard", "/src/tilck-tests/hard.c");
    t7_cc(av_mtar_cc, "/bin/mkustar", "/src/musl-tools/mkustar.c", 0);
    /* -e: modes by content, Kronos's stat() says 0777 for every file */
    av_mtar[0] = "/bin/mkustar";
    av_mtar[1] = "-e";
    av_mtar[2] = "/build/musl/musl-sysroot.tar";
    av_mtar[3] = "/build/musl";
    av_mtar[4] = "/build/musl/SYSROOT";
    av_mtar[5] = 0;
    /* stage 14: listfiles from kronos.py; seed-tree is a Kronos program */
    av_sar[0] = "/bin/tcc7b";
    av_sar[1] = "@/src/seed/ar.rsp";
    av_sar[2] = 0;
    av_sprog[0] = "/bin/tcc7b";
    av_sprog[1] = sp_rsp;
    av_sprog[2] = 0;
    t7_cc(av_stree_cc, "/bin/seed-tree", "/src/seed-tools/seed-tree.c", 0);
    av_stree[0] = "/bin/seed-tree";
    av_stree[1] = "/build/seed/LIST";
    av_stree[2] = "/build/seed";
    av_stree[3] = "/build/musl";
    av_stree[4] = "usr";
    av_stree[5] = "/build/musl/SYSROOT";
    av_stree[6] = "/lib/tcc7";
    av_stree[7] = "usr/lib/tcc";
    av_stree[8] = "/src/seed/TCCINC";
    av_stree[9] = "-";
    av_stree[10] = "usr";
    av_stree[11] = "/src/seed/BUILT";
    av_stree[12] = 0;
    av_star[0] = "/bin/mkustar";
    av_star[1] = "-e";
    av_star[2] = "/build/seed/seed-rootfs.tar";
    av_star[3] = "/build/seed";
    av_star[4] = "/build/seed/LIST";
    av_star[5] = 0;
}

void kmain(void)
{
    unsigned long len;
    uart_puts("Kronos\n");
    kmem_init();
    vm_init();
    ramfs_init((void *)kphys(0x80, 0x20));
    setup_args();
    if (!kfs_get("/bin/init", &len)) {
        for (;;)
            ;
    }
    run(av_init[0], av_init);
    if (!kfs_get("/bin/tcc", &len)) {
        for (;;)
            ;
    }
    run(av_ver[0], av_ver);
    run(av_tcc2[0], av_tcc2);
    run(av_ver2[0], av_ver2);
    run(av_tcc3[0], av_tcc3);
    run(av_ver3[0], av_ver3);
    run(av_tcc4[0], av_tcc4);
    {
        unsigned char *p3;
        unsigned char *p4;
        unsigned long n3;
        unsigned long n4;
        unsigned long i;
        unsigned long lim;
        unsigned long o3;
        unsigned long z3;
        unsigned long o4;
        unsigned long z4;

        p3 = kfs_get("/bin/tcc3", &n3);
        p4 = kfs_get("/bin/tcc4", &n4);
        uart_puts("tcc3sz ");
        uart_hex(n3);
        uart_puts("\n");
        uart_puts("tcc4sz ");
        uart_hex(n4);
        uart_puts("\n");
        if (p3 && p4 && n3 > 0x70 && n4 > 0x70) {
            memcpy(&o3, p3 + 0x48, 8);
            memcpy(&z3, p3 + 0x60, 8);
            memcpy(&o4, p4 + 0x48, 8);
            memcpy(&z4, p4 + 0x60, 8);
            uart_puts("tcc3ph ");
            uart_hex(o3);
            uart_puts(" ");
            uart_hex(z3);
            uart_puts("\n");
            uart_puts("tcc4ph ");
            uart_hex(o4);
            uart_puts(" ");
            uart_hex(z4);
            uart_puts("\n");
            i = 0x1000;
            lim = 0x1000 + z3;
            if (z4 < z3)
                lim = 0x1000 + z4;
            while (i < lim) {
                if (p3[i] != p4[i]) {
                    uart_puts("tdiff ");
                    uart_hex(i);
                    uart_puts(" ");
                    uart_hex((unsigned long)p3[i]);
                    uart_puts(" ");
                    uart_hex((unsigned long)p4[i]);
                    uart_puts("\n");
                    break;
                }
                i = i + 1;
            }
        }
    }
    run(av_cmp[0], av_cmp);
    if (kfs_get("/src/tcc-full/tcc.c", &len)) {
        uart_puts("tcc5 start\n");
        run(av_tcc5[0], av_tcc5);
        uart_puts("tcc5 built\n");
        run(av_ver5[0], av_ver5);
        uart_puts("tcc6 start\n");
        run(av_tcc6[0], av_tcc6);
        uart_puts("tcc6 built\n");
        run(av_ver6[0], av_ver6);
        if (kfs_get("/src/hello/hello.c", &len)) {
            uart_puts("hello6 start\n");
            run(av_hello6[0], av_hello6);
            uart_puts("hello6 built\n");
            snap_file("/bin/tcc6", "tcc6");
            snap_file("/bin/hello6", "hello6");
            run(av_runhello[0], av_runhello);
        }
        /* tcc5 (built by pre-zext tcc4) and tcc6 now differ by design.
           tcc4 has no float literals, so the float constants tcc5 puts
           in tcc6 (the -0.0 of a negation) differ from tcc6b's; the
           fixed point is tcc6b rebuilding itself. */
        if (kfs_get("/src/hello/mob-zext.c", &len)) {
            run(av_zext6[0], av_zext6);
            run(av_runzext6[0], av_runzext6);
        }
        if (kfs_get("/src/t7/floatlit.c", &len))
            t7_test("floatlit6", av_fl6, av_runfl6);
        run(av_tcc6b[0], av_tcc6b);
        run(av_tcc6c[0], av_tcc6c);
        run(av_cmp66[0], av_cmp66);
        if (proc_last_exit() == 0)
            uart_puts("tcc66 ok\n");
        if (kfs_get("/src/tcc7/tcc.c", &len)) {
            run(av_tcc7[0], av_tcc7);
            if (!kfs_get("/bin/tcc7", &len)) {
                uart_puts("tcc7 fail\n");
                for (;;)
                    ;
            }
            uart_puts("tcc7 built\n");
            run(av_tcc7b[0], av_tcc7b);
            if (!kfs_get("/bin/tcc7b", &len)) {
                uart_puts("tcc7b fail\n");
                for (;;)
                    ;
            }
            uart_puts("tcc7b built\n");
            run(av_tcc7c[0], av_tcc7c);
            if (!kfs_get("/bin/tcc7c", &len)) {
                uart_puts("tcc7c fail\n");
                for (;;)
                    ;
            }
            uart_puts("tcc7c built\n");
            run(av_cmp77[0], av_cmp77);
            if (proc_last_exit() == 0)
                uart_puts("tcc77 ok\n");
            snap_file("/bin/tcc7b", "tcc7b");
            run(av_hello7[0], av_hello7);
            run(av_runhello7[0], av_runhello7);
            t7_test("floatlit", av_t7cc_floatlit, av_t7run_floatlit);
            t7_test("kconst", av_t7cc_kconst, av_t7run_kconst);
            t7_test("priv", av_t7cc_priv, av_t7run_priv);
            t7_test("macro", av_t7cc_macro, av_t7run_macro);
            t7_test("penum", av_t7cc_penum, av_t7run_penum);
            t7_test("predefs", av_t7cc_predefs, av_t7run_predefs);
            t7_test("gasdir", av_t7cc_gasdir, av_t7run_gasdir);
            t7_test("bssym", av_t7cc_bssym, av_t7run_bssym);
            t7_test("atomicn", av_t7cc_atomicn, av_t7run_atomicn);
            t7_test("nopic", av_t7cc_nopic, av_t7run_nopic);
            snap_file("/bin/t7-nopic", "nopic");
            t7_test("branch", av_t7cc_branch, av_t7run_branch);
            t7_test("cstore", av_t7cc_cstore, av_t7run_cstore);
            t7_test("luiext", av_t7cc_luiext, av_t7run_luiext);
            t7_test("epilog", av_t7cc_epilog, av_t7run_epilog);
            run(av_arl_cc[0], av_arl_cc);
            run(av_arl_ar[0], av_arl_ar);
            t7_test("arlink", av_t7cc_arlink, av_t7run_arlink);
            uart_puts("t7 done\n");
            /* Tilck: uapi headers, config headers, objects, link */
            if (kfs_get("/src/tilck/SOURCES", &len)) {
                run(av_hinst_cc[0], av_hinst_cc);
                if (kfs_get("/bin/headers-install", &len)) {
                    uart_puts("headers-install built\n");
                    run(av_hinst[0], av_hinst);
                }
                t7_test("uapi-check", av_uapi_cc, av_uapi);
                t7_test("gen-tilck-config", av_gencfg_cc, av_gencfg);
                if (build_objs("/src/tilck/SOURCES", "@/src/tilck/cflags.rsp",
                               "/src/tilck/", "/build/tilck/obj/", "tilck")) {
                    run(av_tlink[0], av_tlink);
                    if (kfs_get("/build/tilck/tilck.elf", &len)) {
                        uart_puts("tilck linked\n");
                        snap_file("/build/tilck/tilck.elf", "tilck");
                    } else
                        uart_puts("tilck link fail\n");
                }
                /* M-mode SBI shim, raw image at 0x80000000 */
                run(av_sbi[0], av_sbi);
                if (kfs_get("/build/tilck/sbi.bin", &len)) {
                    uart_puts("sbi built\n");
                    snap_file("/build/tilck/sbi.bin", "sbi");
                } else
                    uart_puts("sbi build fail\n");
                run(av_tinit[0], av_tinit);
                run(av_tproc[0], av_tproc);
                run(av_tchild[0], av_tchild);
                if (kfs_get("/build/tilck/init", &len)
                    && kfs_get("/build/tilck/proc", &len)
                    && kfs_get("/build/tilck/child", &len)) {
                    uart_puts("tilck progs built\n");
                    snap_file("/build/tilck/init", "tinit");
                    snap_file("/build/tilck/proc", "tproc");
                    snap_file("/build/tilck/child", "tchild");
                } else
                    uart_puts("tilck progs fail\n");
                uart_puts("tilck done\n");
            }
            if (kfs_get("/src/musl/SOURCES", &len)) {
                build_musl();
                uart_puts("musl done\n");
            }
            if (kfs_get("/src/seed/SEED", &len)) {
                build_seed();
                uart_puts("seed done\n");
            }
        }
    }
    for (;;)
        ;
}
