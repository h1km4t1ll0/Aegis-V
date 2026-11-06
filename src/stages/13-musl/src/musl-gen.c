/* musl-gen SRC OUT: the generation steps of musl 1.1.24's Makefile for
 * riscv64, without sed or make, then its "make install-headers".
 *
 *   OUT/obj/include/bits/alltypes.h  sed -f tools/mkalltypes.sed
 *                                    arch/riscv64/bits/alltypes.h.in
 *                                    include/alltypes.h.in
 *   OUT/obj/include/bits/syscall.h   arch/riscv64/bits/syscall.h.in, then
 *                                    its __NR_ lines with SYS_ (sed -n s/__NR_/SYS_/p)
 *   OUT/obj/src/internal/version.h   #define VERSION "1.1.24"
 *   OUT/include/...                  install-headers: include/X -> include/X;
 *                                    arch/generic/bits/X, then arch/riscv64/bits/X
 *                                    on top, -> include/bits/X; the two generated
 *                                    headers -> include/bits/
 *   OUT/SYSROOT                      the installed headers, then lib/libc.a and
 *                                    lib/crt{1,i,n}.o: the members of musl-sysroot.tar
 *
 * SRC/HEADERS lists the headers to install (archive paths, one per line):
 * Kronos has no getdents. Kronos's ramfs needs no directories; on a host,
 * create OUT's directories first. Prints "musl-gen ok". */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VERSION "1.1.24"

static char *out;
static unsigned long olen;
static unsigned long ocap;
static const char *src;
static const char *dst;
static char path[512];

static void die(const char *a, const char *b)
{
    printf("musl-gen: %s%s\n", a, b);
    exit(1);
}

/* whole file, NUL-terminated */
static char *slurp(const char *p, unsigned long *len)
{
    int fd;
    long n;
    unsigned long sz;
    unsigned long cap;
    char *buf;
    char *q;

    fd = open(p, O_RDONLY, 0);
    if (fd < 0)
        die("cannot open ", p);
    cap = 65536;
    buf = malloc(cap + 1);
    sz = 0;
    for (;;) {
        if (sz == cap) {
            q = malloc(cap * 2 + 1);
            memcpy(q, buf, sz);
            buf = q;
            cap = cap * 2;
        }
        n = read(fd, buf + sz, cap - sz);
        if (n <= 0)
            break;
        sz = sz + n;
    }
    close(fd);
    buf[sz] = 0;
    *len = sz;
    return buf;
}

static void emit(const char *s, unsigned long n)
{
    char *q;

    if (olen + n > ocap) {
        ocap = (olen + n) * 2 + 4096;
        q = malloc(ocap);
        if (olen)
            memcpy(q, out, olen);
        out = q;
    }
    memcpy(out + olen, s, n);
    olen = olen + n;
}

static void emits(const char *s)
{
    emit(s, strlen(s));
}

static void spill(const char *p, const char *data, unsigned long n)
{
    int fd;

    fd = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        die("cannot create ", p);
    if (n && write(fd, data, n) != (long)n)
        die("short write ", p);
    close(fd);
}

static const char *join(const char *a, const char *b)
{
    unsigned long n;

    n = strlen(a);
    if (n + strlen(b) + 2 > sizeof(path))
        die("path too long ", b);
    memcpy(path, a, n);
    path[n] = '/';
    strcpy(path + n + 1, b);
    return path;
}

static int starts(const char *s, unsigned long n, const char *pre)
{
    unsigned long k;

    k = strlen(pre);
    return n >= k && memcmp(s, pre, k) == 0;
}

/* one line of an alltypes.h.in through tools/mkalltypes.sed */
static void alltypes_line(const char *s, unsigned long n)
{
    unsigned long i;
    unsigned long sp;
    unsigned long a;
    const char *kind;
    const char *tag;

    /* TYPEDEF \(.*\) \([^ ]*\);$  -> \1 = type, \2 = name (last word) */
    if (starts(s, n, "TYPEDEF") && n > 9 && s[n - 1] == ';' && s[7] == ' ') {
        sp = n - 1;
        while (sp > 7 && s[sp - 1] != ' ')
            sp = sp - 1;
        if (sp > 8) {
            /* name = s[sp .. n-1), type = s[8 .. sp-1) */
            emits("#if defined(__NEED_");
            emit(s + sp, n - 1 - sp);
            emits(") && !defined(__DEFINED_");
            emit(s + sp, n - 1 - sp);
            emits(")\ntypedef ");
            emit(s + 8, sp - 1 - 8);
            emits(" ");
            emit(s + sp, n - 1 - sp);
            emits(";\n#define __DEFINED_");
            emit(s + sp, n - 1 - sp);
            emits("\n#endif\n\n");
            return;
        }
    }
    /* STRUCT * \([^ ]*\) \(.*\);$ (UNION alike) -> \1 = tag, \2 = body */
    kind = 0;
    a = 0;
    if (starts(s, n, "STRUCT")) {
        kind = "struct";
        a = 6;
    } else if (starts(s, n, "UNION")) {
        kind = "union";
        a = 5;
    }
    if (kind && s[n - 1] == ';') {
        while (a < n && s[a] == ' ')
            a = a + 1;
        i = a;
        while (i < n && s[i] != ' ')
            i = i + 1;
        if (i < n - 1) {
            tag = s + a;
            emits("#if defined(__NEED_");
            emits(kind);
            emits("_");
            emit(tag, i - a);
            emits(") && !defined(__DEFINED_");
            emits(kind);
            emits("_");
            emit(tag, i - a);
            emits(")\n");
            emits(kind);
            emits(" ");
            emit(tag, i - a);
            emits(" ");
            emit(s + i + 1, n - 1 - (i + 1));
            emits(";\n#define __DEFINED_");
            emits(kind);
            emits("_");
            emit(tag, i - a);
            emits("\n#endif\n\n");
            return;
        }
    }
    emit(s, n);
    emits("\n");
}

static void alltypes_file(const char *rel)
{
    char *buf;
    unsigned long n;
    unsigned long i;
    unsigned long j;

    buf = slurp(join(src, rel), &n);
    i = 0;
    while (i < n) {
        j = i;
        while (j < n && buf[j] != '\n')
            j = j + 1;
        alltypes_line(buf + i, j - i);
        i = j + 1;
    }
}

static void gen_syscall(void)
{
    char *buf;
    unsigned long n;
    unsigned long i;
    unsigned long j;
    unsigned long k;

    buf = slurp(join(src, "arch/riscv64/bits/syscall.h.in"), &n);
    olen = 0;
    emit(buf, n);
    i = 0;
    while (i < n) {
        j = i;
        while (j < n && buf[j] != '\n')
            j = j + 1;
        /* s/__NR_/SYS_/p: the first __NR_ of the line */
        k = i;
        while (k + 5 <= j && memcmp(buf + k, "__NR_", 5) != 0)
            k = k + 1;
        if (k + 5 <= j) {
            emit(buf + i, k - i);
            emits("SYS_");
            emit(buf + k + 5, j - (k + 5));
            emits("\n");
        }
        i = j + 1;
    }
}

/* the installed name of a HEADERS line, or 0 if it is not installed */
static const char *installed(const char *line, char *name)
{
    const char *gen = "arch/generic/bits/";
    const char *arch = "arch/riscv64/bits/";

    if (strncmp(line, "include/", 8) == 0) {
        strcpy(name, line);
        return name;
    }
    if (strncmp(line, gen, strlen(gen)) == 0) {
        strcpy(name, "include/bits/");
        strcat(name, line + strlen(gen));
        return name;
    }
    if (strncmp(line, arch, strlen(arch)) == 0) {
        strcpy(name, "include/bits/");
        strcat(name, line + strlen(arch));
        return name;
    }
    return 0;
}

static char *list;
static unsigned long nlist;
static char *sysroot;
static unsigned long nsys;
static unsigned long capsys;

static void sysroot_add(const char *name)
{
    char *q;
    unsigned long k;

    k = strlen(name);
    if (nsys + k + 1 > capsys) {
        capsys = (nsys + k + 1) * 2 + 4096;
        q = malloc(capsys);
        if (nsys)
            memcpy(q, sysroot, nsys);
        sysroot = q;
    }
    memcpy(sysroot + nsys, name, k);
    sysroot[nsys + k] = '\n';
    nsys = nsys + k + 1;
}

/* install the HEADERS lines that start with pre (pass order = precedence) */
static void install_pass(const char *pre)
{
    char line[256];
    char name[256];
    char *buf;
    unsigned long n;
    unsigned long i;
    unsigned long j;

    i = 0;
    while (i < nlist) {
        j = i;
        while (j < nlist && list[j] != '\n')
            j = j + 1;
        if (j > i && j - i < sizeof(line)) {
            memcpy(line, list + i, j - i);
            line[j - i] = 0;
            if (strncmp(line, pre, strlen(pre)) == 0 && installed(line, name)) {
                buf = slurp(join(src, line), &n);
                spill(join(dst, name), buf, n);
            }
        }
        i = j + 1;
    }
}

/* SYSROOT: each installed header once (arch overrides generic) */
static void sysroot_headers(void)
{
    char line[256];
    char name[256];
    char other[256];
    unsigned long i;
    unsigned long j;
    const char *gen = "arch/generic/bits/";

    i = 0;
    while (i < nlist) {
        j = i;
        while (j < nlist && list[j] != '\n')
            j = j + 1;
        if (j > i && j - i < sizeof(line)) {
            memcpy(line, list + i, j - i);
            line[j - i] = 0;
            if (installed(line, name)) {
                /* a generic bits header the arch replaces is listed by the arch line */
                if (strncmp(line, gen, strlen(gen)) == 0) {
                    strcpy(other, "\narch/riscv64/bits/");
                    strcat(other, line + strlen(gen));
                    strcat(other, "\n");
                    if (strstr(list - 1, other))
                        name[0] = 0;
                }
                if (name[0])
                    sysroot_add(name);
            }
        }
        i = j + 1;
    }
}

int main(int argc, char **argv)
{
    char *gen;
    unsigned long n;
    char *l2;

    if (argc != 3)
        die("usage: musl-gen SRC OUT", "");
    src = argv[1];
    dst = argv[2];

    olen = 0;
    alltypes_file("arch/riscv64/bits/alltypes.h.in");
    alltypes_file("include/alltypes.h.in");
    spill(join(dst, "obj/include/bits/alltypes.h"), out, olen);

    gen_syscall();
    spill(join(dst, "obj/include/bits/syscall.h"), out, olen);

    olen = 0;
    emits("#define VERSION \"" VERSION "\"\n");
    spill(join(dst, "obj/src/internal/version.h"), out, olen);

    /* "\n" before the list and a NUL after it, so every line is found as
       "\nLINE\n" with strstr */
    gen = slurp(join(src, "HEADERS"), &n);
    l2 = malloc(n + 3);
    l2[0] = '\n';
    memcpy(l2 + 1, gen, n);
    if (n == 0 || gen[n - 1] != '\n') {
        l2[n + 1] = '\n';
        n = n + 1;
    }
    l2[n + 1] = 0;
    list = l2 + 1;
    nlist = n;
    install_pass("include/");
    install_pass("arch/generic/bits/");
    install_pass("arch/riscv64/bits/");
    gen = slurp(join(dst, "obj/include/bits/alltypes.h"), &n);
    spill(join(dst, "include/bits/alltypes.h"), gen, n);
    gen = slurp(join(dst, "obj/include/bits/syscall.h"), &n);
    spill(join(dst, "include/bits/syscall.h"), gen, n);

    nsys = 0;
    sysroot_headers();
    sysroot_add("include/bits/alltypes.h");
    sysroot_add("include/bits/syscall.h");
    sysroot_add("lib/libc.a");
    sysroot_add("lib/crt1.o");
    sysroot_add("lib/crti.o");
    sysroot_add("lib/crtn.o");
    spill(join(dst, "SYSROOT"), sysroot, nsys);
    printf("musl-gen ok\n");
    return 0;
}
