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
static char *av_cmp56[4];
static char *av_hello6[16];
static char *av_runhello[2];
static char *av_gccver[16];
static char *av_rungccver[2];
static char *av_xstrdup[4];
static char *av_runxstrdup[2];
static char *av_gengen[4];
static char *av_rungenh[3];
static char *av_gmhdr[4];
static char *av_rungmhdr[2];
static char *av_genmodes[4];
static char *av_rungenm[4];
static char *av_rungenmc[3];
static char *av_rungenh2[4];
static char *av_rungenmin[4];
static char *av_genflags[4];
static char *av_rungenf[4];
static char *av_gencodes[4];
static char *av_rungenc[4];
static char *av_genconfig[4];
static char *av_rungencfg[4];
static char *av_genconst[4];
static char *av_runconst[4];
static char *av_genemit[4];
static char *av_runemit[4];
static char *av_genrecog[4];
static char *av_runrecog[4];
static char *av_genextract[4];
static char *av_runextract[4];
static char *av_genoutput[4];
static char *av_runoutput[4];
static char *av_genopinit[4];
static char *av_runopinit[4];
static char *av_genpeep[4];
static char *av_runpeep[4];
static char *av_genattr[4];
static char *av_runattr[4];
static char *av_genpreds[4];
static char *av_runpredsh[5];
static char *av_runpredsc[4];
static char *av_gencheck[4];
static char *av_runcheck[3];
static char *av_genattrtab[4];
static char *av_runattrtab[4];
static char *av_rungencrtl[3];
static char *av_gengtype[4];
static char *av_rungty[2];
static char *av_cc1[4];
static char *av_runc1[8];
static char *av_hellogcc[16];
static char *av_runhelg[2];
static char *av_cc1src[22];
static char *av_cc12[4];
static char *av_runc12[8];
static char *av_helloself[16];
static char *av_runhs[2];
static char srcbuf[256];
static char dstbuf[256];

static int blob_has(const unsigned char *p, unsigned long n, const char *s)
{
    unsigned long i;
    unsigned long j;
    unsigned long m;

    m = 0;
    while (s[m])
        m = m + 1;
    if (n < m)
        return 0;
    i = 0;
    while (i + m <= n) {
        j = 0;
        while (j < m && p[i + j] == (unsigned char)s[j])
            j = j + 1;
        if (j == m)
            return 1;
        i = i + 1;
    }
    return 0;
}

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

static void c_to_s(const char *src, char *dst)
{
    unsigned long n;
    unsigned long i;
    const char *prefix;
    unsigned long plen;

    prefix = "/src/gcc/";
    plen = 9;
    n = 0;
    i = 0;
    while (prefix[i] && src[i] == prefix[i])
        i = i + 1;
    if (i == plen) {
        dst[0] = '/';
        dst[1] = 's';
        dst[2] = 'r';
        dst[3] = 'c';
        dst[4] = '/';
        dst[5] = 's';
        dst[6] = 'e';
        dst[7] = 'l';
        dst[8] = 'f';
        dst[9] = '/';
        n = 10;
        while (src[i] && n < 255) {
            dst[n] = src[i];
            n = n + 1;
            i = i + 1;
        }
    } else {
        while (src[n] && n < 255) {
            dst[n] = src[n];
            n = n + 1;
        }
    }
    dst[n] = 0;
    if (n >= 2 && dst[n - 2] == '.' && dst[n - 1] == 'c')
        dst[n - 1] = 's';
}

static int self_compile_all(void)
{
    unsigned char *list;
    unsigned long n;
    unsigned long i;
    unsigned long s;
    unsigned long slen;

    list = kfs_get("/src/gcc/cc1-self.list", &n);
    if (!list)
        return 0;
    i = 0;
    while (i < n) {
        while (i < n && (list[i] == '\n' || list[i] == '\r' || list[i] == ' '))
            i = i + 1;
        if (i >= n)
            break;
        s = 0;
        while (i < n && list[i] != '\n' && list[i] != '\r' && s < 255) {
            srcbuf[s] = list[i];
            s = s + 1;
            i = i + 1;
        }
        srcbuf[s] = 0;
        if (s == 0)
            continue;
        c_to_s(srcbuf, dstbuf);
        uart_puts("self ");
        uart_puts(srcbuf);
        uart_puts("\n");
        run(av_cc1src[0], av_cc1src);
        if (proc_last_exit() != 0 || !kfs_get(dstbuf, &slen) || slen < 8) {
            uart_puts("self fail\n");
            return 0;
        }
        uart_puts("self ok\n");
    }
    return 1;
}

static void md_gen(char **tcc_av, const char *start, const char *bin,
                   const char *built, const char *snap, char **run_av,
                   const char *out, const char *ok, const char *needle)
{
    unsigned long len;
    unsigned char *p;

    uart_puts(start);
    uart_puts("\n");
    run(tcc_av[0], tcc_av);
    if (!kfs_get(bin, &len))
        return;
    uart_puts(built);
    uart_puts("\n");
    snap_file(bin, snap);
    run(run_av[0], run_av);
    p = kfs_get(out, &len);
    if (p && blob_has(p, len, needle)) {
        uart_puts(ok);
        uart_puts("\n");
    }
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
    av_cmp56[0] = "/bin/cmp";
    av_cmp56[1] = "/bin/tcc5";
    av_cmp56[2] = "/bin/tcc6";
    av_cmp56[3] = 0;
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
    av_gccver[0] = "/bin/tcc6";
    av_gccver[1] = "-o";
    av_gccver[2] = "/bin/gccver";
    av_gccver[3] = "-I";
    av_gccver[4] = "/src/gcc";
    av_gccver[5] = "-I";
    av_gccver[6] = "/include";
    av_gccver[7] = "-nostdlib";
    av_gccver[8] = "-Wl,-Ttext=0x10000";
    av_gccver[9] = "-Wl,-oformat=elf64-little";
    av_gccver[10] = "/lib/crt.s";
    av_gccver[11] = "/lib/libc.c";
    av_gccver[12] = "/src/gcc/printver.c";
    av_gccver[13] = "/src/gcc/version.c";
    av_gccver[14] = 0;
    av_rungccver[0] = "/bin/gccver";
    av_rungccver[1] = 0;
    av_xstrdup[0] = "/bin/tcc6";
    av_xstrdup[1] = "@/src/gcc/xstrdup.rsp";
    av_xstrdup[2] = 0;
    av_runxstrdup[0] = "/bin/xstrdup";
    av_runxstrdup[1] = 0;
    av_gengen[0] = "/bin/tcc6";
    av_gengen[1] = "@/src/gcc/gengenrtl.rsp";
    av_gengen[2] = 0;
    av_rungenh[0] = "/bin/gengenrtl";
    av_rungenh[1] = "-h";
    av_rungenh[2] = 0;
    av_gmhdr[0] = "/bin/tcc6";
    av_gmhdr[1] = "@/src/gcc/gmhdr.rsp";
    av_gmhdr[2] = 0;
    av_rungmhdr[0] = "/bin/gmhdr";
    av_rungmhdr[1] = 0;
    av_genmodes[0] = "/bin/tcc6";
    av_genmodes[1] = "@/src/gcc/genmodes.rsp";
    av_genmodes[2] = 0;
    av_rungenm[0] = "/bin/genmodes";
    av_rungenm[1] = "-h";
    av_rungenm[2] = "/src/gcc/insn-modes.h";
    av_rungenm[3] = 0;
    av_rungenmc[0] = "/bin/genmodes";
    av_rungenmc[1] = "/src/gcc/insn-modes.c";
    av_rungenmc[2] = 0;
    av_rungenh2[0] = "/bin/gengenrtl";
    av_rungenh2[1] = "-h";
    av_rungenh2[2] = "/src/gcc/genrtl.h";
    av_rungenh2[3] = 0;
    av_rungenmin[0] = "/bin/genmodes";
    av_rungenmin[1] = "-m";
    av_rungenmin[2] = "/src/gcc/min-insn-modes.c";
    av_rungenmin[3] = 0;
    av_genflags[0] = "/bin/tcc6";
    av_genflags[1] = "@/src/gcc/genflags.rsp";
    av_genflags[2] = 0;
    av_rungenf[0] = "/bin/genflags";
    av_rungenf[1] = "/src/gcc/riscv64.md";
    av_rungenf[2] = "/src/gcc/insn-flags.h";
    av_rungenf[3] = 0;
    av_gencodes[0] = "/bin/tcc6";
    av_gencodes[1] = "@/src/gcc/gencodes.rsp";
    av_gencodes[2] = 0;
    av_rungenc[0] = "/bin/gencodes";
    av_rungenc[1] = "/src/gcc/riscv64.md";
    av_rungenc[2] = "/src/gcc/insn-codes.h";
    av_rungenc[3] = 0;
    av_genconfig[0] = "/bin/tcc6";
    av_genconfig[1] = "@/src/gcc/genconfig.rsp";
    av_genconfig[2] = 0;
    av_rungencfg[0] = "/bin/genconfig";
    av_rungencfg[1] = "/src/gcc/riscv64.md";
    av_rungencfg[2] = "/src/gcc/insn-config.h";
    av_rungencfg[3] = 0;
    av_genconst[0] = "/bin/tcc6";
    av_genconst[1] = "@/src/gcc/genconstants.rsp";
    av_genconst[2] = 0;
    av_runconst[0] = "/bin/genconstants";
    av_runconst[1] = "/src/gcc/riscv64.md";
    av_runconst[2] = "/src/gcc/insn-constants.h";
    av_runconst[3] = 0;
    av_genemit[0] = "/bin/tcc6";
    av_genemit[1] = "@/src/gcc/genemit.rsp";
    av_genemit[2] = 0;
    av_runemit[0] = "/bin/genemit";
    av_runemit[1] = "/src/gcc/riscv64.md";
    av_runemit[2] = "/src/gcc/insn-emit.c";
    av_runemit[3] = 0;
    av_genrecog[0] = "/bin/tcc6";
    av_genrecog[1] = "@/src/gcc/genrecog.rsp";
    av_genrecog[2] = 0;
    av_runrecog[0] = "/bin/genrecog";
    av_runrecog[1] = "/src/gcc/riscv64.md";
    av_runrecog[2] = "/src/gcc/insn-recog.c";
    av_runrecog[3] = 0;
    av_genextract[0] = "/bin/tcc6";
    av_genextract[1] = "@/src/gcc/genextract.rsp";
    av_genextract[2] = 0;
    av_runextract[0] = "/bin/genextract";
    av_runextract[1] = "/src/gcc/riscv64.md";
    av_runextract[2] = "/src/gcc/insn-extract.c";
    av_runextract[3] = 0;
    av_genoutput[0] = "/bin/tcc6";
    av_genoutput[1] = "@/src/gcc/genoutput.rsp";
    av_genoutput[2] = 0;
    av_runoutput[0] = "/bin/genoutput";
    av_runoutput[1] = "/src/gcc/riscv64.md";
    av_runoutput[2] = "/src/gcc/insn-output.c";
    av_runoutput[3] = 0;
    av_genopinit[0] = "/bin/tcc6";
    av_genopinit[1] = "@/src/gcc/genopinit.rsp";
    av_genopinit[2] = 0;
    av_runopinit[0] = "/bin/genopinit";
    av_runopinit[1] = "/src/gcc/riscv64.md";
    av_runopinit[2] = "/src/gcc/insn-opinit.c";
    av_runopinit[3] = 0;
    av_genpeep[0] = "/bin/tcc6";
    av_genpeep[1] = "@/src/gcc/genpeep.rsp";
    av_genpeep[2] = 0;
    av_runpeep[0] = "/bin/genpeep";
    av_runpeep[1] = "/src/gcc/riscv64.md";
    av_runpeep[2] = "/src/gcc/insn-peep.c";
    av_runpeep[3] = 0;
    av_genattr[0] = "/bin/tcc6";
    av_genattr[1] = "@/src/gcc/genattr.rsp";
    av_genattr[2] = 0;
    av_runattr[0] = "/bin/genattr";
    av_runattr[1] = "/src/gcc/riscv64.md";
    av_runattr[2] = "/src/gcc/insn-attr.h";
    av_runattr[3] = 0;
    av_genpreds[0] = "/bin/tcc6";
    av_genpreds[1] = "@/src/gcc/genpreds.rsp";
    av_genpreds[2] = 0;
    av_runpredsh[0] = "/bin/genpreds";
    av_runpredsh[1] = "-h";
    av_runpredsh[2] = "/src/gcc/riscv64.md";
    av_runpredsh[3] = "/src/gcc/tm-preds.h";
    av_runpredsh[4] = 0;
    av_runpredsc[0] = "/bin/genpreds";
    av_runpredsc[1] = "/src/gcc/riscv64.md";
    av_runpredsc[2] = "/src/gcc/insn-preds.c";
    av_runpredsc[3] = 0;
    av_gencheck[0] = "/bin/tcc6";
    av_gencheck[1] = "@/src/gcc/gencheck.rsp";
    av_gencheck[2] = 0;
    av_runcheck[0] = "/bin/gencheck";
    av_runcheck[1] = "/src/gcc/tree-check.h";
    av_runcheck[2] = 0;
    av_genattrtab[0] = "/bin/tcc6";
    av_genattrtab[1] = "@/src/gcc/genattrtab.rsp";
    av_genattrtab[2] = 0;
    av_runattrtab[0] = "/bin/genattrtab";
    av_runattrtab[1] = "/src/gcc/riscv64.md";
    av_runattrtab[2] = "/src/gcc/insn-attrtab.c";
    av_runattrtab[3] = 0;
    av_rungencrtl[0] = "/bin/gengenrtl";
    av_rungencrtl[1] = "/src/gcc/genrtl.c";
    av_rungencrtl[2] = 0;
    av_gengtype[0] = "/bin/tcc6";
    av_gengtype[1] = "@/src/gcc/gengtype.rsp";
    av_gengtype[2] = 0;
    av_rungty[0] = "/bin/gengtype";
    av_rungty[1] = 0;
    av_cc1[0] = "/bin/tcc6";
    av_cc1[1] = "@/src/gcc/cc1.rsp";
    av_cc1[2] = 0;
    av_runc1[0] = "/bin/cc1";
    av_runc1[1] = "-quiet";
    av_runc1[2] = "-I";
    av_runc1[3] = "/include";
    av_runc1[4] = "-o";
    av_runc1[5] = "/src/hello/hello-gcc.s";
    av_runc1[6] = "/src/hello/hello-gcc.c";
    av_runc1[7] = 0;
    av_hellogcc[0] = "/bin/tcc6";
    av_hellogcc[1] = "-o";
    av_hellogcc[2] = "/bin/hello-gcc";
    av_hellogcc[3] = "-I";
    av_hellogcc[4] = "/include";
    av_hellogcc[5] = "-nostdlib";
    av_hellogcc[6] = "-Wl,-Ttext=0x10000";
    av_hellogcc[7] = "-Wl,-oformat=elf64-little";
    av_hellogcc[8] = "/lib/crt.s";
    av_hellogcc[9] = "/lib/libc.c";
    av_hellogcc[10] = "/src/hello/hello-gcc.s";
    av_hellogcc[11] = 0;
    av_runhelg[0] = "/bin/hello-gcc";
    av_runhelg[1] = 0;
    av_cc1src[0] = "/bin/cc1";
    av_cc1src[1] = "-quiet";
    av_cc1src[2] = "-fno-unit-at-a-time";
    av_cc1src[3] = "-fno-builtin";
    av_cc1src[4] = "-I";
    av_cc1src[5] = "/include";
    av_cc1src[6] = "-o";
    av_cc1src[7] = dstbuf;
    av_cc1src[8] = srcbuf;
    av_cc1src[9] = 0;
    av_cc12[0] = "/bin/tcc6";
    av_cc12[1] = "@/src/gcc/cc1-2.rsp";
    av_cc12[2] = 0;
    av_runc12[0] = "/bin/cc1-2";
    av_runc12[1] = "-quiet";
    av_runc12[2] = "-I";
    av_runc12[3] = "/include";
    av_runc12[4] = "-o";
    av_runc12[5] = "/src/hello/hello-self.s";
    av_runc12[6] = "/src/hello/hello-self.c";
    av_runc12[7] = 0;
    av_helloself[0] = "/bin/tcc6";
    av_helloself[1] = "-o";
    av_helloself[2] = "/bin/hello-self";
    av_helloself[3] = "-I";
    av_helloself[4] = "/include";
    av_helloself[5] = "-nostdlib";
    av_helloself[6] = "-Wl,-Ttext=0x10000";
    av_helloself[7] = "-Wl,-oformat=elf64-little";
    av_helloself[8] = "/lib/crt.s";
    av_helloself[9] = "/lib/libc.c";
    av_helloself[10] = "/src/hello/hello-self.s";
    av_helloself[11] = 0;
    av_runhs[0] = "/bin/hello-self";
    av_runhs[1] = 0;
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
        run(av_cmp56[0], av_cmp56);
        uart_puts("tcc56 ok\n");
        if (kfs_get("/src/hello/hello.c", &len)) {
            uart_puts("hello6 start\n");
            run(av_hello6[0], av_hello6);
            uart_puts("hello6 built\n");
            snap_file("/bin/tcc6", "tcc6");
            snap_file("/bin/hello6", "hello6");
            run(av_runhello[0], av_runhello);
        }
        if (kfs_get("/src/gcc/version.c", &len)) {
            uart_puts("gccver start\n");
            run(av_gccver[0], av_gccver);
            uart_puts("gccver built\n");
            snap_file("/bin/gccver", "gccver");
            run(av_rungccver[0], av_rungccver);
        }
        if (kfs_get("/src/gcc/xstrdup.rsp", &len)) {
            uart_puts("xstrdup start\n");
            run(av_xstrdup[0], av_xstrdup);
            uart_puts("xstrdup built\n");
            snap_file("/bin/xstrdup", "xstrdup");
            run(av_runxstrdup[0], av_runxstrdup);
        }
        if (kfs_get("/src/gcc/gengenrtl.c", &len)) {
            uart_puts("gengenrtl start\n");
            run(av_gengen[0], av_gengen);
            if (kfs_get("/bin/gengenrtl", &len)) {
                uart_puts("gengenrtl built\n");
                snap_file("/bin/gengenrtl", "gengenrtl");
                run(av_rungenh[0], av_rungenh);
            }
        }
        if (kfs_get("/src/gcc/gmhdr.rsp", &len)) {
            uart_puts("gmhdr start\n");
            run(av_gmhdr[0], av_gmhdr);
            if (kfs_get("/bin/gmhdr", &len)) {
                uart_puts("gmhdr built\n");
                run(av_rungmhdr[0], av_rungmhdr);
            }
        }
        if (kfs_get("/src/gcc/genmodes.rsp", &len)) {
            uart_puts("genmodes start\n");
            run(av_genmodes[0], av_genmodes);
            if (kfs_get("/bin/genmodes", &len)) {
                unsigned char *p;
                uart_puts("genmodes built\n");
                snap_file("/bin/genmodes", "genmodes");
                run(av_rungenm[0], av_rungenm);
                p = kfs_get("/src/gcc/insn-modes.h", &len);
                if (p && blob_has(p, len, "VOIDmode")
                    && blob_has(p, len, "MAX_MACHINE_MODE")) {
                    uart_puts("insn-modes VOIDmode\n");
                    snap_file("/src/gcc/insn-modes.h", "modes");
                }
                run(av_rungenmc[0], av_rungenmc);
                p = kfs_get("/src/gcc/insn-modes.c", &len);
                if (p && len > 64)
                    uart_puts("insn-modes.c ok\n");
                run(av_rungenh2[0], av_rungenh2);
                p = kfs_get("/src/gcc/genrtl.h", &len);
                if (p && blob_has(p, len, "GCC_GENRTL_H"))
                    uart_puts("genrtl.h ok\n");
                run(av_rungenmin[0], av_rungenmin);
                p = kfs_get("/src/gcc/min-insn-modes.c", &len);
                if (p && blob_has(p, len, "mode_name"))
                    uart_puts("min-insn-modes ok\n");
                if (kfs_get("/src/gcc/genflags.rsp", &len)) {
                    uart_puts("genflags start\n");
                    run(av_genflags[0], av_genflags);
                    if (kfs_get("/bin/genflags", &len)) {
                        uart_puts("genflags built\n");
                        snap_file("/bin/genflags", "genflags");
                        run(av_rungenf[0], av_rungenf);
                        p = kfs_get("/src/gcc/insn-flags.h", &len);
                        if (p && blob_has(p, len, "HAVE_nop")) {
                            uart_puts("insn-flags HAVE_nop\n");
                            if (blob_has(p, len, "gen_nop"))
                                uart_puts("insn-flags gen_nop\n");
                            if (blob_has(p, len, "HAVE_beq"))
                                uart_puts("insn-flags HAVE_beq\n");
                            if (blob_has(p, len, "gen_beq"))
                                uart_puts("insn-flags gen_beq\n");
                            snap_file("/src/gcc/insn-flags.h", "iflags");
                            md_gen(av_gencodes, "gencodes start", "/bin/gencodes",
                                   "gencodes built", "gencodes", av_rungenc,
                                   "/src/gcc/insn-codes.h",
                                   "insn-codes CODE_FOR_nop", "CODE_FOR_nop");
                            md_gen(av_genconfig, "genconfig start", "/bin/genconfig",
                                   "genconfig built", "genconfig", av_rungencfg,
                                   "/src/gcc/insn-config.h",
                                   "insn-config MAX_RECOG_OPERANDS",
                                   "MAX_RECOG_OPERANDS");
                            md_gen(av_genconst, "genconstants start", "/bin/genconstants",
                                   "genconstants built", "genconst", av_runconst,
                                   "/src/gcc/insn-constants.h",
                                   "insn-constants ok", "GCC_INSN_CONSTANTS_H");
                            md_gen(av_genemit, "genemit start", "/bin/genemit",
                                   "genemit built", "genemit", av_runemit,
                                   "/src/gcc/insn-emit.c",
                                   "insn-emit gen_nop", "gen_nop");
                            md_gen(av_genrecog, "genrecog start", "/bin/genrecog",
                                   "genrecog built", "genrecog", av_runrecog,
                                   "/src/gcc/insn-recog.c",
                                   "insn-recog split_insns", "split_insns");
                            md_gen(av_genextract, "genextract start", "/bin/genextract",
                                   "genextract built", "genextract", av_runextract,
                                   "/src/gcc/insn-extract.c",
                                   "insn-extract insn_extract", "insn_extract");
                            md_gen(av_genoutput, "genoutput start", "/bin/genoutput",
                                   "genoutput built", "genoutput", av_runoutput,
                                   "/src/gcc/insn-output.c",
                                   "insn-output insn_data", "insn_data");
                            md_gen(av_genopinit, "genopinit start", "/bin/genopinit",
                                   "genopinit built", "genopinit", av_runopinit,
                                   "/src/gcc/insn-opinit.c",
                                   "insn-opinit init_all_optabs", "init_all_optabs");
                            md_gen(av_genpeep, "genpeep start", "/bin/genpeep",
                                   "genpeep built", "genpeep", av_runpeep,
                                   "/src/gcc/insn-peep.c",
                                   "insn-peep peephole", "peephole");
                            md_gen(av_genattr, "genattr start", "/bin/genattr",
                                   "genattr built", "genattr", av_runattr,
                                   "/src/gcc/insn-attr.h",
                                   "insn-attr GCC_INSN_ATTR_H", "GCC_INSN_ATTR_H");
                            md_gen(av_genpreds, "genpreds start", "/bin/genpreds",
                                   "genpreds built", "genpreds", av_runpredsh,
                                   "/src/gcc/tm-preds.h",
                                   "tm-preds GCC_TM_PREDS_H", "GCC_TM_PREDS_H");
                            run(av_runpredsc[0], av_runpredsc);
                            p = kfs_get("/src/gcc/insn-preds.c", &len);
                            if (p && blob_has(p, len, "hard-reg-set.h"))
                                uart_puts("insn-preds ok\n");
                            md_gen(av_gencheck, "gencheck start", "/bin/gencheck",
                                   "gencheck built", "gencheck", av_runcheck,
                                   "/src/gcc/tree-check.h",
                                   "tree-check GCC_TREE_CHECK_H", "GCC_TREE_CHECK_H");
                            md_gen(av_genattrtab, "genattrtab start", "/bin/genattrtab",
                                   "genattrtab built", "genattrtab", av_runattrtab,
                                   "/src/gcc/insn-attrtab.c",
                                   "insn-attrtab genattrtab", "genattrtab");
                            run(av_rungencrtl[0], av_rungencrtl);
                            p = kfs_get("/src/gcc/genrtl.c", &len);
                            if (p && blob_has(p, len, "gen_rtx_"))
                                uart_puts("genrtl.c ok\n");
                            if (kfs_get("/src/gcc/gengtype.rsp", &len)) {
                                uart_puts("gengtype start\n");
                                run(av_gengtype[0], av_gengtype);
                                if (kfs_get("/bin/gengtype", &len)) {
                                    uart_puts("gengtype built\n");
                                    snap_file("/bin/gengtype", "gengtype");
                                    proc_set_cwd("/src/gcc");
                                    run(av_rungty[0], av_rungty);
                                    proc_set_cwd("/");
                                    p = kfs_get("/src/gcc/gtype-desc.c", &len);
                                    if (p && len > 200)
                                        uart_puts("gtype-desc ok\n");
                                }
                            }
                            if (kfs_get("/src/gcc/cc1.rsp", &len)
                                && kfs_get("/src/gcc/gtype-desc.c", &len)) {
                                uart_puts("cc1 start\n");
                                run(av_cc1[0], av_cc1);
                                if (kfs_get("/bin/cc1", &len)) {
                                    uart_puts("cc1 built\n");
                                    snap_file("/bin/cc1", "cc1");
                                    uart_puts("cc1 hello start\n");
                                    run(av_runc1[0], av_runc1);
                                    p = kfs_get("/src/hello/hello-gcc.s", &len);
                                    if (p && len > 8) {
                                        uart_puts("hello.s ok\n");
                                        uart_puts("hello-gcc start\n");
                                        run(av_hellogcc[0], av_hellogcc);
                                        if (kfs_get("/bin/hello-gcc", &len)) {
                                            uart_puts("hello-gcc built\n");
                                            snap_file("/bin/hello-gcc", "hello-gcc");
                                            run(av_runhelg[0], av_runhelg);
                                            uart_puts("cc1-2 start\n");
                                            if (self_compile_all()) {
                                                uart_puts("cc1-2 compiled\n");
                                                run(av_cc12[0], av_cc12);
                                                if (kfs_get("/bin/cc1-2", &len)) {
                                                    uart_puts("cc1-2 built\n");
                                                    snap_file("/bin/cc1-2", "cc1-2");
                                                    run(av_runc12[0], av_runc12);
                                                    p = kfs_get("/src/hello/hello-self.s", &len);
                                                    if (p && len > 8) {
                                                        run(av_helloself[0], av_helloself);
                                                        if (kfs_get("/bin/hello-self", &len)) {
                                                            uart_puts("hello-self built\n");
                                                            snap_file("/bin/hello-self", "hello-self");
                                                            run(av_runhs[0], av_runhs);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    for (;;)
        ;
}
