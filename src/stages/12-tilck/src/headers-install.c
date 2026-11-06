/* headers-install: Linux v6.6 "make headers_install" for Kronos (no sed,
 * no getdents).  The file list is /src/uapi/FILES (archive paths, one per
 * line); the files are /src/uapi/<archive path>.  Every header goes to
 * /usr/include/<path below .../uapi/>, with the sed rules of
 * scripts/headers_install.sh applied line by line, then its
 * "unifdef -U__KERNEL__ -D__EXPORTED_HEADERS__" for the plain forms
 * (#ifdef X, #ifndef X, #if defined(X), #if !defined(X)); other #if lines
 * stay as they are.  Each asm-generic/X.h without an arch asm/X.h gets a
 * wrapper asm/X.h. */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

long ecall6(long n, long a, long b, long c, long d, long e, long f);

#define MAXF 2048

static char *names[MAXF];
static int nnames;
static char *out;
static unsigned long olen;
static unsigned long ocap;
static char *spare;
static unsigned long spare_cap;
static unsigned long in_cap;
static char *rbuf;
static unsigned long rcap;

static void die(const char *a, const char *b)
{
    write(1, "headers-install: ", 17);
    write(1, a, strlen(a));
    write(1, b, strlen(b));
    write(1, "\n", 1);
    exit(1);
}

/* Whole file into *buf (grown as needed, *cap bytes plus a NUL). */
static char *slurp_into(const char *path, unsigned long *len, char **buf, unsigned long *cap)
{
    int fd;
    long n;
    unsigned long sz;
    char *q;

    fd = open(path, O_RDONLY, 0);
    if (fd < 0)
        die("cannot open ", path);
    if (!*buf) {
        *cap = 65536;
        *buf = malloc(*cap + 1);
    }
    sz = 0;
    for (;;) {
        if (sz == *cap) {
            q = malloc(*cap * 2 + 1);
            memcpy(q, *buf, sz);
            *buf = q;
            *cap = *cap * 2;
        }
        n = read(fd, *buf + sz, *cap - sz);
        if (n <= 0)
            break;
        sz = sz + n;
    }
    close(fd);
    (*buf)[sz] = 0;
    *len = sz;
    return *buf;
}

static char *slurp(const char *path, unsigned long *len)
{
    char *buf;
    unsigned long cap;

    buf = 0;
    return slurp_into(path, len, &buf, &cap);
}

static void put(const char *s, unsigned long n)
{
    char *q;

    if (olen + n > ocap) {
        while (olen + n > ocap)
            ocap = ocap ? ocap * 2 : 65536;
        q = malloc(ocap);
        if (olen)
            memcpy(q, out, olen);
        out = q;
    }
    memcpy(out + olen, s, n);
    olen = olen + n;
}

static void mkdir_p(const char *path)
{
    char buf[256];
    unsigned long i;

    i = 0;
    while (path[i] && i < 255) {
        buf[i] = path[i];
        if (path[i] == '/' && i > 0) {
            buf[i] = 0;
            ecall6(34, -100, (long)buf, 0755, 0, 0, 0); /* mkdirat */
            buf[i] = '/';
        }
        i = i + 1;
    }
}

static void spit(const char *path, const char *data, unsigned long n)
{
    int fd;

    mkdir_p(path);
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        die("cannot create ", path);
    if (n && write(fd, data, n) != (long)n)
        die("short write ", path);
    close(fd);
}

static int isspc(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static int isalnum_(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static int starts(const char *s, const char *e, const char *w)
{
    unsigned long n;

    n = strlen(w);
    return (unsigned long)(e - s) >= n && memcmp(s, w, n) == 0;
}

/* One line [s, e), without its '\n'. */
static void do_line(const char *s, const char *e)
{
    static const char *drop[3] = { "__user", "__force", "__iomem" };
    static const char *kw[3] = { "inline", "asm", "volatile" };
    const char *p;
    const char *w;
    unsigned long n;
    int k;
    int done;

    /* s@^#include <linux/compiler(|_types).h>@@ */
    if (starts(s, e, "#include <linux/compiler.h>"))
        s = s + strlen("#include <linux/compiler.h>");
    else if (starts(s, e, "#include <linux/compiler_types.h>"))
        s = s + strlen("#include <linux/compiler_types.h>");
    /* s@#(ifndef|define|endif[[:space:]]*\/[*])[[:space:]]*_UAPI@#\1 @ */
    p = s;
    while (p < e && *p != '#')
        p = p + 1;
    if (p < e) {
        w = p + 1;
        if (starts(w, e, "ifndef"))
            w = w + 6;
        else if (starts(w, e, "define"))
            w = w + 6;
        else if (starts(w, e, "endif")) {
            w = w + 5;
            while (w < e && isspc(*w))
                w = w + 1;
            if (starts(w, e, "/*"))
                w = w + 2;
            else
                w = 0;
        } else
            w = 0;
        if (w) {
            const char *u;
            u = w;
            while (u < e && isspc(*u))
                u = u + 1;
            if (starts(u, e, "_UAPI")) {
                put(s, w - s);
                put(" ", 1);
                s = u + 5;
            }
        }
    }
    p = s;
    while (p < e) {
        done = 0;
        /* s/([[:space:](])(__user|__force|__iomem)[[:space:]]/\1/g */
        if (isspc(*p) || *p == '(') {
            for (k = 0; k < 3 && !done; k++) {
                n = strlen(drop[k]);
                if (starts(p + 1, e, drop[k]) && p + 1 + n < e && isspc(p[1 + n])) {
                    put(p, 1);
                    p = p + 2 + n;
                    done = 1;
                }
            }
        }
        /* s/__attribute_const__([[:space:]]|$)/\1/g */
        if (!done && starts(p, e, "__attribute_const__")) {
            w = p + strlen("__attribute_const__");
            if (w == e || isspc(*w)) {
                p = w;
                done = 1;
            }
        }
        /* s/(^|[^a-zA-Z0-9])__packed([^a-zA-Z0-9_]|$)/\1__attribute__((packed))\2/g */
        if (!done && starts(p, e, "__packed") && (p == s || !(isalnum_(p[-1]) && p[-1] != '_'))) {
            w = p + 8;
            if (w == e || !isalnum_(*w)) {
                put("__attribute__((packed))", 23);
                p = w;
                done = 1;
            }
        }
        /* s/(^|[[:space:](])(inline|asm|volatile)([[:space:](]|$)/\1__\2__\3/g */
        if (!done && (p == s || isspc(p[-1]) || p[-1] == '(')) {
            for (k = 0; k < 3 && !done; k++) {
                n = strlen(kw[k]);
                w = p + n;
                if (starts(p, e, kw[k]) && (w == e || isspc(*w) || *w == '(')) {
                    put("__", 2);
                    put(p, n);
                    put("__", 2);
                    p = w;
                    done = 1;
                }
            }
        }
        if (!done) {
            put(p, 1);
            p = p + 1;
        }
    }
}

/* Directive word after '#' at the start of [s, e), or 0. */
static const char *directive(const char *s, const char *e)
{
    while (s < e && (*s == ' ' || *s == '\t'))
        s = s + 1;
    if (s == e || *s != '#')
        return 0;
    s = s + 1;
    while (s < e && (*s == ' ' || *s == '\t'))
        s = s + 1;
    return s;
}

static int word_is(const char *s, const char *e, const char *w)
{
    unsigned long n;

    n = strlen(w);
    return (unsigned long)(e - s) >= n && memcmp(s, w, n) == 0
        && (s + n == e || !isalnum_(s[n]));
}

/* Value of a plain #if/#ifdef/#ifndef on __KERNEL__ (0) or
   __EXPORTED_HEADERS__ (1): 0 or 1, or -1 if not one of those. */
static int known_if(const char *s, const char *e)
{
    const char *p;
    int neg;
    int def;
    int v;

    p = directive(s, e);
    if (!p)
        return -1;
    neg = 0;
    def = 0;
    if (word_is(p, e, "ifdef")) {
        p = p + 5;
    } else if (word_is(p, e, "ifndef")) {
        p = p + 6;
        neg = 1;
    } else if (word_is(p, e, "if")) {
        p = p + 2;
        while (p < e && isspc(*p))
            p = p + 1;
        if (p < e && *p == '!') {
            neg = 1;
            p = p + 1;
        }
        if (!word_is(p, e, "defined"))
            return -1;
        p = p + 7;
        while (p < e && isspc(*p))
            p = p + 1;
        if (p == e || *p != '(')
            return -1;
        p = p + 1;
        def = 1;
    } else
        return -1;
    while (p < e && isspc(*p))
        p = p + 1;
    if (word_is(p, e, "__KERNEL__")) {
        v = 0;
        p = p + 10;
    } else if (word_is(p, e, "__EXPORTED_HEADERS__")) {
        v = 1;
        p = p + 20;
    } else
        return -1;
    while (p < e && isspc(*p))
        p = p + 1;
    if (def) {
        if (p == e || *p != ')')
            return -1;
        p = p + 1;
        while (p < e && isspc(*p))
            p = p + 1;
    }
    /* only a comment may follow */
    if (p < e && !(e - p >= 2 && p[0] == '/' && (p[1] == '*' || p[1] == '/')))
        return -1;
    return neg ? !v : v;
}

/* unifdef over out[0, olen) into a new out */
static void unifdef(void)
{
    /* per #if level: 0 plain (kept as is), 1 known and keeping,
       2 known and dropping, 3 known, dropping to its #endif */
    static int st[64];
    char *in;
    unsigned long n;
    const char *s;
    const char *e;
    const char *p;
    int depth;
    int drop;
    int k;
    int i;

    /* swap the output buffer with the spare one */
    in = out;
    n = olen;
    out = spare;
    ocap = spare_cap;
    spare = in;
    spare_cap = in_cap;
    olen = 0;
    depth = 0;
    s = in;
    while (s < in + n) {
        e = s;
        while (e < in + n && *e != '\n')
            e = e + 1;
        drop = 0;
        for (i = 0; i < depth; i++)
            if (st[i] >= 2)
                drop = 1;
        p = directive(s, e);
        if (p && (word_is(p, e, "if") || word_is(p, e, "ifdef") || word_is(p, e, "ifndef"))) {
            if (depth == 64)
                die("#if nesting too deep", "");
            k = drop ? -1 : known_if(s, e);
            st[depth++] = k < 0 ? 0 : (k ? 1 : 2);
            if (k >= 0 || drop)
                goto next;
        } else if (p && word_is(p, e, "else") && depth > 0 && st[depth - 1]) {
            st[depth - 1] = st[depth - 1] == 2 ? 1 : 3;
            goto next;
        } else if (p && word_is(p, e, "elif") && depth > 0 && st[depth - 1]) {
            if (st[depth - 1] == 2) {
                /* the rest of the chain stays, as a plain #if */
                st[depth - 1] = 0;
                drop = 0;
                for (i = 0; i < depth - 1; i++)
                    if (st[i] >= 2)
                        drop = 1;
                if (!drop) {
                    put("#if", 3);
                    put(p + 4, e - (p + 4));
                    put("\n", 1);
                }
                goto next;
            }
            st[depth - 1] = 3;
            goto next;
        } else if (p && word_is(p, e, "endif") && depth > 0) {
            depth = depth - 1;
            if (st[depth] || drop)
                goto next;
        }
        if (!drop) {
            put(s, e - s);
            if (e < in + n)
                put("\n", 1);
        }
    next:
        s = e + 1;
    }
}

static void install(const char *src, const char *dst)
{
    char *d;
    char *s;
    char *e;
    unsigned long n;

    d = slurp_into(src, &n, &rbuf, &rcap);
    olen = 0;
    s = d;
    while (s < d + n) {
        e = s;
        while (e < d + n && *e != '\n')
            e = e + 1;
        do_line(s, e);
        if (e < d + n)
            put("\n", 1);
        s = e + 1;
    }
    in_cap = ocap;
    unifdef();
    spit(dst, out, olen);
}

static const char *below(const char *p, const char *pre)
{
    unsigned long n;

    n = strlen(pre);
    if (strncmp(p, pre, n) == 0)
        return p + n;
    return 0;
}

static int have_arch(const char *x)
{
    int i;
    const char *r;

    for (i = 0; i < nnames; i++) {
        r = below(names[i], "arch/riscv/include/uapi/asm/");
        if (r && strcmp(r, x) == 0)
            return 1;
    }
    return 0;
}

int main(void)
{
    char *list;
    unsigned long n;
    char *p;
    char *q;
    char src[512];
    char dst[512];
    const char *r;
    int pass;
    int i;
    int count;

    list = slurp("/src/uapi/FILES", &n);
    p = list;
    while (*p) {
        q = p;
        while (*q && *q != '\n')
            q = q + 1;
        if (q > p) {
            if (nnames == MAXF)
                die("too many files", "");
            names[nnames++] = p;
        }
        if (!*q)
            break;
        *q = 0;
        p = q + 1;
    }
    count = 0;
    /* generic headers first, arch headers on top */
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < nnames; i++) {
            r = below(names[i], pass ? "arch/riscv/include/uapi/" : "include/uapi/");
            if (!r)
                continue;
            n = strlen(r);
            if (n < 2 || strcmp(r + n - 2, ".h") != 0)
                continue;
            strcpy(src, "/src/uapi/");
            strcat(src, names[i]);
            strcpy(dst, "/usr/include/");
            strcat(dst, r);
            install(src, dst);
            count = count + 1;
        }
    }
    for (i = 0; i < nnames; i++) {
        r = below(names[i], "include/uapi/asm-generic/");
        if (!r || have_arch(r))
            continue;
        strcpy(dst, "/usr/include/asm/");
        strcat(dst, r);
        olen = 0;
        put("#include <asm-generic/", 22);
        put(r, strlen(r));
        put(">\n", 2);
        spit(dst, out, olen);
        count = count + 1;
    }
    if (count == 0)
        die("no headers", "");
    puts("headers-install done");
    return 0;
}
