/* gen-tilck-config: the CMake steps that write Tilck's tilck_gen_headers/,
 * for Kronos (no CMake, no shell, no getdents).
 *
 * In:  /src/tilck/tilck-config.txt   NAME=value lines; ON=/OFF= name lists
 *      /src/tilck/CONFIG_FILES       template paths (config/...), one per line
 *      /src/tilck/MODULES            module directory names, one per line
 *      /src/tilck/<template>, /src/tilck/modules/sysfs/ro_config_vars.h
 * Out: config/X.h        -> BUILD_DIR/tilck_gen_headers/X.h
 *      config/kernel/X.h -> BUILD_DIR/kbin/tilck_gen_headers/X.h
 *      BUILD_DIR/tilck_gen_headers/modules_config.h   (root CMakeLists.txt)
 *      BUILD_DIR/tilck_gen_headers/generated_config.h (scripts/build_apps/
 *                                                      gen_config.cpp)
 * configure_file(@ONLY): @NAME@ is replaced, "#cmakedefine01 X" becomes
 * "#define X 1" or "#define X 0", "#cmakedefine X rest" becomes
 * "#define X rest" or "/* #undef X * /".  A name that tilck-config.txt does
 * not list is an error, not a silent 0. */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

long ecall6(long n, long a, long b, long c, long d, long e, long f);

#define SRC "/src/tilck/"
#define MAXV 256
#define MAXL 256

static char *vname[MAXV];
static char *vval[MAXV];
static int nv;
static char *onv[MAXV];
static int non;
static char *offv[MAXV];
static int noff;
static char *mods[64];
static int nmods;
static char *cfgs[MAXL];
static int ncfgs;
static char *out;
static unsigned long olen;
static unsigned long ocap;
static const char *build_dir;

static void die(const char *a, const char *b)
{
    write(1, "gen-tilck-config: ", 18);
    write(1, a, strlen(a));
    write(1, b, strlen(b));
    write(1, "\n", 1);
    exit(1);
}

static char *slurp(const char *path)
{
    int fd;
    long n;
    unsigned long cap;
    unsigned long sz;
    char *p;
    char *q;

    fd = open(path, O_RDONLY, 0);
    if (fd < 0)
        die("cannot open ", path);
    cap = 65536;
    sz = 0;
    p = malloc(cap + 1);
    for (;;) {
        if (sz == cap) {
            q = malloc(cap * 2 + 1);
            memcpy(q, p, sz);
            p = q;
            cap = cap * 2;
        }
        n = read(fd, p + sz, cap - sz);
        if (n <= 0)
            break;
        sz = sz + n;
    }
    close(fd);
    p[sz] = 0;
    return p;
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

static void puts_(const char *s)
{
    put(s, strlen(s));
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

static void spit(const char *path)
{
    int fd;

    mkdir_p(path);
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        die("cannot create ", path);
    if (olen && write(fd, out, olen) != (long)olen)
        die("short write ", path);
    close(fd);
    olen = 0;
}

/* Split text into lines in place; empty lines are dropped. */
static int lines(char *p, char **v, int max)
{
    int n;
    char *q;

    n = 0;
    while (*p) {
        q = p;
        while (*q && *q != '\n')
            q = q + 1;
        if (q > p && q[-1] == '\r')
            q[-1] = 0;
        if (q > p) {
            if (n == max)
                die("list too long", "");
            v[n++] = p;
        }
        if (!*q)
            break;
        *q = 0;
        p = q + 1;
    }
    return n;
}

static int isid(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

/* Space separated names of s go to v. */
static int words(char *s, char **v, int n)
{
    while (*s) {
        while (*s == ' ' || *s == '\t')
            s = s + 1;
        if (!*s)
            break;
        if (n == MAXV)
            die("too many names", "");
        v[n++] = s;
        while (*s && *s != ' ' && *s != '\t')
            s = s + 1;
        if (*s)
            *s++ = 0;
    }
    return n;
}

static int in(char **v, int n, const char *s, unsigned long len)
{
    int i;

    for (i = 0; i < n; i++)
        if (strlen(v[i]) == len && memcmp(v[i], s, len) == 0)
            return 1;
    return 0;
}

static const char *lookup(const char *s, unsigned long len)
{
    int i;

    for (i = 0; i < nv; i++)
        if (strlen(vname[i]) == len && memcmp(vname[i], s, len) == 0)
            return vval[i];
    return 0;
}

static void read_config(void)
{
    char *v[512];
    char *eq;
    int n;
    int i;

    n = lines(slurp(SRC "tilck-config.txt"), v, 512);
    for (i = 0; i < n; i++) {
        if (v[i][0] == '#')
            continue;
        eq = strchr(v[i], '=');
        if (!eq)
            die("bad config line ", v[i]);
        *eq = 0;
        if (strcmp(v[i], "ON") == 0)
            non = words(eq + 1, onv, non);
        else if (strcmp(v[i], "OFF") == 0)
            noff = words(eq + 1, offv, noff);
        else {
            if (nv == MAXV)
                die("too many values", "");
            vname[nv] = v[i];
            vval[nv] = eq + 1;
            nv = nv + 1;
        }
    }
    build_dir = lookup("BUILD_DIR", 9);
    if (!build_dir)
        die("no BUILD_DIR", "");
}

/* 1/0 of a boolean; unknown is an error. */
static int onoff(const char *s, unsigned long len)
{
    char name[128];

    if (in(onv, non, s, len))
        return 1;
    if (in(offv, noff, s, len))
        return 0;
    if (len > 127)
        len = 127;
    memcpy(name, s, len);
    name[len] = 0;
    die("unknown boolean ", name);
    return 0;
}

/* @NAME@ substitution of [s, e) into out. */
static void subst(const char *s, const char *e)
{
    const char *p;
    const char *q;
    const char *v;
    char name[128];

    p = s;
    while (p < e) {
        if (*p == '@') {
            q = p + 1;
            while (q < e && isid(*q))
                q = q + 1;
            if (q < e && *q == '@' && q > p + 1) {
                v = lookup(p + 1, q - p - 1);
                if (!v) {
                    memcpy(name, p, q - p + 1);
                    name[q - p + 1] = 0;
                    die("unknown ", name);
                }
                puts_(v);
                p = q + 1;
                continue;
            }
        }
        put(p, 1);
        p = p + 1;
    }
}

static void template(const char *rel, const char *dst)
{
    char path[256];
    char *d;
    char *s;
    char *e;
    char *w;
    char *n;

    strcpy(path, SRC);
    strcat(path, rel);
    d = slurp(path);
    s = d;
    while (*s) {
        e = s;
        while (*e && *e != '\n')
            e = e + 1;
        w = s;
        if (*w == '#') {
            w = w + 1;
            while (*w == ' ' || *w == '\t')
                w = w + 1;
        }
        if (*s == '#' && strncmp(w, "cmakedefine01", 13) == 0
            && (w[13] == ' ' || w[13] == '\t')) {
            w = w + 13;
            while (*w == ' ' || *w == '\t')
                w = w + 1;
            n = w;
            while (isid(*w))
                w = w + 1;
            puts_("#define ");
            put(n, w - n);
            puts_(onoff(n, w - n) ? " 1" : " 0");
        } else if (*s == '#' && strncmp(w, "cmakedefine", 11) == 0
                   && (w[11] == ' ' || w[11] == '\t')) {
            w = w + 11;
            while (*w == ' ' || *w == '\t')
                w = w + 1;
            n = w;
            while (isid(*w))
                w = w + 1;
            if (onoff(n, w - n)) {
                puts_("#define ");
                subst(n, e);
            } else {
                puts_("/* #undef ");
                put(n, w - n);
                puts_(" */");
            }
        } else
            subst(s, e);
        if (*e)
            put("\n", 1);
        s = *e ? e + 1 : e;
    }
    spit(dst);
}

static void modules_config(void)
{
    static const char *bools[5] = {
        "SERIAL_CON_IN_VIDEO_MODE", "KERNEL_SERCON", "TERM_BIG_SCROLL_BUF",
        "PS2_DO_SELFTEST", "PS2_VERBOSE_DEBUG_LOG"
    };
    char *en[64];
    char *list;
    const char *v;
    char dst[256];
    int nen;
    int i;

    v = lookup("ENABLED_MODULES_LIST", 20);
    if (!v)
        die("no ENABLED_MODULES_LIST", "");
    list = malloc(strlen(v) + 1);
    strcpy(list, v);
    nen = words(list, en, 0);
    puts_("/* Auto-generated by root CMake. Do not edit. */\n\n#pragma once\n\n");
    puts_("/* Module enable/disable flags */\n");
    for (i = 0; i < nmods; i++) {
        puts_("#define MOD_");
        puts_(mods[i]);
        puts_(in(en, nen, mods[i], strlen(mods[i])) ? " 1\n" : " 0\n");
    }
    puts_("\n/* Shared module config variables */\n");
    v = lookup("TTY_COUNT", 9);
    if (!v)
        die("no TTY_COUNT", "");
    puts_("#define TTY_COUNT              ");
    puts_(v);
    puts_("\n");
    for (i = 0; i < 5; i++) {
        puts_("#define ");
        puts_(bools[i]);
        puts_(onoff(bools[i], strlen(bools[i])) ? " 1\n" : " 0\n");
    }
    strcpy(dst, build_dir);
    strcat(dst, "/tilck_gen_headers/modules_config.h");
    spit(dst);
}

/* scripts/build_apps/gen_config.cpp: sysfs objects for every module and
 * every DEF_STATIC_CONF_RO of ro_config_vars.h, by category (std::map
 * order, i.e. sorted). */
static char *cat_name[32];
static char *opt_name[32][64];
static int nopt[32];
static int ncat;

static int cat_of(const char *s)
{
    int i;
    int j;

    for (i = 0; i < ncat; i++)
        if (strcmp(cat_name[i], s) == 0)
            return i;
    if (ncat == 32)
        die("too many categories", "");
    i = ncat;
    while (i > 0 && strcmp(cat_name[i - 1], s) > 0) {
        cat_name[i] = cat_name[i - 1];
        nopt[i] = nopt[i - 1];
        for (j = 0; j < nopt[i]; j++)
            opt_name[i][j] = opt_name[i - 1][j];
        i = i - 1;
    }
    cat_name[i] = (char *)s;
    nopt[i] = 0;
    ncat = ncat + 1;
    return i;
}

static void add_opt(const char *cat, char *opt)
{
    int c;

    c = cat_of(cat);
    if (nopt[c] == 64)
        die("too many options in ", cat);
    opt_name[c][nopt[c]++] = opt;
}

static void generated_config(void)
{
    static const char *rov = SRC "modules/sysfs/ro_config_vars.h";
    char *v[256];
    char *cat;
    char *s;
    char *p;
    char dst[256];
    int n;
    int i;
    int j;
    unsigned long len;

    for (i = 0; i < nmods; i++)
        add_opt("modules", mods[i]);
    cat = 0;
    n = lines(slurp(rov), v, 256);
    for (i = 0; i < n; i++) {
        s = v[i];
        len = strlen(s);
        /* /\* (\w+) \*\/ */
        if (len > 6 && strncmp(s, "/* ", 3) == 0 && strcmp(s + len - 3, " */") == 0) {
            p = s + 3;
            while (isid(*p))
                p = p + 1;
            if (p == s + len - 3 && p > s + 3) {
                *p = 0;
                cat = s + 3;
                continue;
            }
        }
        /* DEF_STATIC_CONF_RO\((\w+),\s+(\w+),\s+(.+)\); */
        if (strncmp(s, "DEF_STATIC_CONF_RO(", 19) == 0 && len > 2 && strcmp(s + len - 2, ");") == 0) {
            p = s + 19;
            while (isid(*p))
                p = p + 1;
            if (*p != ',' || p == s + 19)
                continue;
            p = p + 1;
            if (*p != ' ' && *p != '\t')
                continue;
            while (*p == ' ' || *p == '\t')
                p = p + 1;
            s = p;
            while (isid(*p))
                p = p + 1;
            if (*p != ',' || p == s)
                continue;
            if (!cat)
                die("No category found before ", v[i]);
            *p = 0;
            add_opt(cat, s);
        }
    }
    puts_("/* This file is AUTO-GENERATED by the gen_config tool */\n\n");
    puts_("#include \"");
    puts_(rov);
    puts_("\"\n\n");
    for (i = 0; i < nmods; i++) {
        puts_("#include <tilck_gen_headers/mod_");
        puts_(mods[i]);
        puts_(".h>\n");
    }
    puts_("\nstatic void sysfs_fail_to_register_obj(const char *name);\n\n");
    puts_("/* modules */\n");
    for (i = 0; i < nmods; i++) {
        puts_("DEF_STATIC_CONF_RO(BOOL, ");
        puts_(mods[i]);
        puts_(", MOD_");
        puts_(mods[i]);
        puts_(");\n");
    }
    puts_("\n");
    for (i = 0; i < ncat; i++) {
        puts_("DEF_STATIC_SYSOBJ_TYPE(\n   type_");
        puts_(cat_name[i]);
        puts_(",\n");
        for (j = 0; j < nopt[i]; j++) {
            puts_("   &prop_");
            puts_(opt_name[i][j]);
            puts_(",\n");
        }
        puts_("   NULL\n);\n\n");
        puts_("DEF_STATIC_SYSOBJ(\n   obj_");
        puts_(cat_name[i]);
        puts_(",\n   &type_");
        puts_(cat_name[i]);
        puts_(",\n   NULL /* hooks */,\n");
        for (j = 0; j < nopt[i]; j++) {
            puts_("   &conf_");
            puts_(opt_name[i][j]);
            puts_(",\n");
        }
        puts_(");\n\n");
    }
    puts_("\nvoid\nsysfs_create_config_obj(void)\n{\n");
    puts_("   struct sysobj *root = &sysfs_root_obj;\n   const char *name;\n\n");
    for (i = 0; i < ncat; i++) {
        puts_("   name = \"");
        puts_(cat_name[i]);
        puts_("\";\n   if (sysfs_register_obj(NULL, root, name, &obj_");
        puts_(cat_name[i]);
        puts_("))\n      goto fail;\n\n");
    }
    puts_("\n   return; /* success */\n\nfail:\n   sysfs_fail_to_register_obj(name);\n}\n");
    strcpy(dst, build_dir);
    strcat(dst, "/tilck_gen_headers/generated_config.h");
    spit(dst);
}

int main(void)
{
    char dst[256];
    const char *base;
    int i;

    read_config();
    ncfgs = lines(slurp(SRC "CONFIG_FILES"), cfgs, MAXL);
    nmods = lines(slurp(SRC "MODULES"), mods, 64);
    for (i = 0; i < ncfgs; i++) {
        strcpy(dst, build_dir);
        if (strncmp(cfgs[i], "config/kernel/", 14) == 0) {
            base = cfgs[i] + 14;
            strcat(dst, "/kbin/tilck_gen_headers/");
        } else if (strncmp(cfgs[i], "config/", 7) == 0) {
            base = cfgs[i] + 7;
            strcat(dst, "/tilck_gen_headers/");
        } else
            die("not a config template: ", cfgs[i]);
        strcat(dst, base);
        template(cfgs[i], dst);
    }
    modules_config();
    generated_config();
    puts("tilck config ok");
    return 0;
}
