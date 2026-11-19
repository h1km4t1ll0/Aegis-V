/* paxw: the test archive of paxx (20-gcc95 README, rung gmp9), written
   here instead of by a host tar.

   paxw make       a pax archive on stdout: a `g` header (comment, mtime),
                   `x` headers with path (a name of 165 bytes), linkpath
                   (a symlink target of 145 bytes), size (the ustar size
                   field says 0) and a fractional mtime; a name of 151
                   bytes in ustar prefix + name; directories, regular
                   files (0, 1, 513, 700, 1500 bytes), a symlink, a hard
                   link, and members that excludes.txt (below) leaves out
                   or keeps. Every name starts with `t/`.
   paxw bad-type   an archive with a character device `t/dev/null0`
   paxw bad-sum    an archive whose member `t/badsum.txt` has a wrong
                   header checksum
   paxw check DIR  checks DIR after `paxx -C DIR -s 1 -X EXCLUDES`, with
                   the four EXCLUDES lines of run.sh (skip/ less
                   skip/keep.txt, .gmo files, gone); prints
                   `paxw check ok`, or each failed check and exits 1. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char longdir[128], longpath[256], longlink[256];
static char pfx[160], pname[100], pfull[260];
static int fails;

static void put(const void *p, size_t n)
{
    const char *c = p;
    while (n) {
        ssize_t w = write(1, c, n);
        if (w <= 0) {
            perror("paxw: write");
            exit(1);
        }
        c += w;
        n -= (size_t)w;
    }
}

static void octal(char *f, int w, unsigned long n)
{
    int i;
    f[w - 1] = 0;
    for (i = w - 2; i >= 0; i--) {
        f[i] = (char)('0' + (n & 7));
        n >>= 3;
    }
}

/* one ustar header; BADSUM writes a checksum one too high */
static void header(const char *prefix, const char *name, char type, unsigned long mode,
                   unsigned long size, unsigned long mtime, const char *link, int badsum)
{
    char h[512];
    unsigned long sum = 0;
    int i;
    memset(h, 0, sizeof h);
    strncpy(h, name, 100);
    octal(h + 100, 8, mode);
    octal(h + 108, 8, 0);
    octal(h + 116, 8, 0);
    octal(h + 124, 12, size);
    octal(h + 136, 12, mtime);
    h[156] = type;
    if (link)
        strncpy(h + 157, link, 100);
    memcpy(h + 257, "ustar", 6);
    memcpy(h + 263, "00", 2);
    if (prefix)
        strncpy(h + 345, prefix, 155);
    memset(h + 148, ' ', 8);
    for (i = 0; i < 512; i++)
        sum += (unsigned char)h[i];
    octal(h + 148, 7, sum + (badsum ? 1 : 0));
    h[155] = ' ';
    put(h, 512);
}

/* the byte at offset I of test file SEED */
static unsigned char pat(int seed, unsigned long i)
{
    return (unsigned char)(i * 31u * (unsigned)seed + (unsigned)seed + i / 7);
}

static void data(int seed, unsigned long n)
{
    static char z[512];
    unsigned long i;
    for (i = 0; i < n; i++) {
        unsigned char c = pat(seed, i);
        put(&c, 1);
    }
    if (n % 512)
        put(z, 512 - n % 512);
}

/* a pax header of type TYPE named NAME with up to two KEY=VALUE records */
static void pax(char type, const char *name, const char *k1, const char *v1,
                const char *k2, const char *v2)
{
    char d[1024];
    size_t n = 0;
    const char *k[2], *v[2];
    int r;
    k[0] = k1; v[0] = v1; k[1] = k2; v[1] = v2;
    for (r = 0; r < 2 && k[r]; r++) {
        size_t body = 1 + strlen(k[r]) + 1 + strlen(v[r]) + 1; /* " k=v\n" */
        size_t len = body + 1, digits;
        char num[16];
        for (;;) { /* the length counts its own digits */
            digits = (size_t)sprintf(num, "%lu", (unsigned long)len);
            if (body + digits == len)
                break;
            len = body + digits;
        }
        n += (size_t)sprintf(d + n, "%lu %s=%s\n", (unsigned long)len, k[r], v[r]);
    }
    header(NULL, name, type, 0644, n, 0, NULL, 0);
    put(d, n);
    if (n % 512) {
        static char z[512];
        put(z, 512 - n % 512);
    }
}

static void names(void)
{
    int i;
    strcpy(longdir, "t/");
    for (i = 0; i < 6; i++)
        strcat(longdir, "long-directory-name-");
    sprintf(longpath, "%s/file-with-a-long-name.txt", longdir);    /* 2+120+26 */
    sprintf(longlink, "%s/file-with-a-long-name.txt", longdir + 2); /* relative to t/ */
    strcpy(pfx, "t/pfx/");
    for (i = 0; i < 80; i++)
        strcat(pfx, "p");
    for (i = 0; i < 60; i++)
        pname[i] = 'n';
    strcpy(pname + 60, ".txt");
    sprintf(pfull, "%s/%s", pfx, pname);
}

static void make(void)
{
    static char z[1024];
    header(NULL, "t/", '5', 0755, 0, 0, NULL, 0);
    pax('g', "pax_global_header", "comment", "paxx test archive", "mtime", "1000000000");
    header(NULL, "t/d/", '5', 0750, 0, 0, NULL, 0);
    header(NULL, "t/d/short.txt", '0', 0644, 11, 0, NULL, 0);
    put("short file\n", 11);
    put(z, 512 - 11);
    pax('x', "t/PaxHeaders.1/file-with-a-long-name.txt", "path", longpath,
        "mtime", "1234567890.123456789");
    header(NULL, longpath, '0', 0755, 1500, 0, NULL, 0); /* name cut at 100 */
    data(1, 1500);
    header(pfx, pname, '0', 0600, 513, 0, NULL, 0);
    data(2, 513);
    pax('x', "t/PaxHeaders.1/sized.bin", "size", "700", NULL, NULL);
    header(NULL, "t/sized.bin", '0', 0644, 0, 0, NULL, 0);
    data(3, 700);
    header(NULL, "t/link", '2', 0777, 0, 0, "d/short.txt", 0);
    pax('x', "t/PaxHeaders.1/longlink", "linkpath", longlink, NULL, NULL);
    header(NULL, "t/longlink", '2', 0777, 0, 0, "cut", 0);
    header(NULL, "t/hard.txt", '1', 0444, 0, 0, "t/d/short.txt", 0);
    header(NULL, "t/skip/", '5', 0755, 0, 0, NULL, 0);
    header(NULL, "t/skip/a.txt", '0', 0644, 4, 0, NULL, 0);
    data(4, 4);
    header(NULL, "t/skip/keep.txt", '0', 0644, 5, 0, NULL, 0);
    data(5, 5);
    header(NULL, "t/d/x.gmo", '0', 0644, 6, 0, NULL, 0);
    data(6, 6);
    header(NULL, "t/gone/", '5', 0755, 0, 0, NULL, 0);
    header(NULL, "t/gone/f.txt", '0', 0644, 7, 0, NULL, 0);
    data(7, 7);
    header(NULL, "t/empty", '0', 0644, 0, 0, NULL, 0);
    put(z, 1024);
}

static void fail(const char *what, const char *path)
{
    printf("paxw check FAIL %s %s\n", what, path);
    fails++;
}

/* PATH is a regular file of N bytes of pattern SEED (SEED 0: TEXT) */
static void file(const char *path, int seed, const char *text, unsigned long n,
                 unsigned long mode, long mtime)
{
    struct stat st;
    unsigned char buf[2048];
    unsigned long i;
    int fd;
    if (lstat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        fail("not a regular file", path);
        return;
    }
    if ((unsigned long)st.st_size != n)
        fail("size", path);
    if ((st.st_mode & 07777) != mode)
        fail("mode", path);
    if (mtime >= 0 && (long)st.st_mtime != mtime)
        fail("mtime", path);
    fd = open(path, O_RDONLY);
    if (fd < 0 || read(fd, buf, sizeof buf) != (ssize_t)n) {
        fail("read", path);
    } else {
        for (i = 0; i < n; i++)
            if (buf[i] != (seed ? pat(seed, i) : (unsigned char)text[i]))
                break;
        if (i < n)
            fail("content", path);
    }
    if (fd >= 0)
        close(fd);
}

static void absent(const char *path)
{
    struct stat st;
    if (lstat(path, &st) == 0 || errno != ENOENT)
        fail("should not exist", path);
}

static void dir(const char *path, unsigned long mode)
{
    struct stat st;
    if (lstat(path, &st) != 0 || !S_ISDIR(st.st_mode))
        fail("not a directory", path);
    else if ((st.st_mode & 07777) != mode)
        fail("mode", path);
}

static void symlnk(const char *path, const char *target)
{
    char buf[512];
    ssize_t n = readlink(path, buf, sizeof buf - 1);
    if (n < 0) {
        fail("not a symlink", path);
        return;
    }
    buf[n] = 0;
    if (strcmp(buf, target) != 0)
        fail("symlink target", path);
}

static void check(const char *top)
{
    char lp[256], lt[256];
    if (chdir(top) != 0) {
        fail("chdir", top);
        return;
    }
    sprintf(lp, "%s/file-with-a-long-name.txt", longdir + 2);
    sprintf(lt, "%s/%s", pfx + 2, pname);
    printf("paxw: long path %d bytes, prefix path %d bytes, long link %d bytes\n",
           (int)strlen(longpath), (int)strlen(pfull), (int)strlen(longlink));
    dir("d", 0750);
    dir("skip", 0755);
    file("d/short.txt", 0, "short file\n", 11, 0644, 1000000000);
    file(lp, 1, NULL, 1500, 0755, 1234567890);
    file(lt, 2, NULL, 513, 0600, 1000000000);
    file("sized.bin", 3, NULL, 700, 0644, -1);
    symlnk("link", "d/short.txt");
    symlnk("longlink", longlink);
    file("hard.txt", 0, "short file\n", 11, 0444, -1);
    file("skip/keep.txt", 5, NULL, 5, 0644, -1);
    file("empty", 0, "", 0, 0644, -1);
    absent("skip/a.txt");
    absent("d/x.gmo");
    absent("gone");
    absent("PaxHeaders.1");
    absent("pax_global_header");
    absent("t");
    /* through the symlinks */
    {
        int fd = open("longlink", O_RDONLY);
        unsigned char c;
        if (fd < 0 || read(fd, &c, 1) != 1 || c != pat(1, 0))
            fail("read through", "longlink");
        if (fd >= 0)
            close(fd);
        fd = open("link", O_RDONLY);
        if (fd < 0 || read(fd, &c, 1) != 1 || c != 's')
            fail("read through", "link");
        if (fd >= 0)
            close(fd);
    }
}

int main(int argc, char **argv)
{
    names();
    if (argc == 2 && strcmp(argv[1], "make") == 0) {
        make();
    } else if (argc == 2 && strcmp(argv[1], "bad-type") == 0) {
        static char z[1024];
        header(NULL, "t/", '5', 0755, 0, 0, NULL, 0);
        header(NULL, "t/dev/null0", '3', 0666, 0, 0, NULL, 0);
        put(z, 1024);
    } else if (argc == 2 && strcmp(argv[1], "bad-sum") == 0) {
        static char z[1024];
        header(NULL, "t/", '5', 0755, 0, 0, NULL, 0);
        header(NULL, "t/badsum.txt", '0', 0644, 3, 0, NULL, 1);
        data(8, 3);
        put(z, 1024);
    } else if (argc == 3 && strcmp(argv[1], "check") == 0) {
        check(argv[2]);
        if (fails) {
            printf("paxw check: %d failed\n", fails);
            return 1;
        }
        printf("paxw check ok\n");
    } else {
        fprintf(stderr, "usage: paxw make|bad-type|bad-sum|check DIR\n");
        return 2;
    }
    return 0;
}
