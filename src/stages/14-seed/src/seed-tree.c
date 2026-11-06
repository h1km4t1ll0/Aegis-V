/* seed-tree OUTLIST ROOT [SRCDIR PREFIX LIST]...: the file tree of the
 * seed under ROOT and its mkustar listing OUTLIST (a Kronos program).
 *
 * For every line L of each LIST: copy SRCDIR/L to ROOT/PREFIX/L (SRCDIR
 * "-": the file is already there, built in place; it must exist) and add
 * PREFIX/L to OUTLIST. Prints "seed-tree ok N files". */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *out;
static unsigned long olen;
static unsigned long ocap;
static char src[512];
static char dst[512];
static char name[512];

static void die(const char *a, const char *b)
{
    printf("seed-tree: %s%s\n", a, b);
    exit(1);
}

/* whole file into a malloc'd buffer */
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
    buf = malloc(cap);
    sz = 0;
    for (;;) {
        if (sz == cap) {
            q = malloc(cap * 2);
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
    *len = sz;
    return buf;
}

static void spill(const char *p, const char *data, unsigned long n)
{
    int fd;

    fd = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        die("cannot create ", p);
    if (write(fd, data, n) != (long)n)
        die("short write ", p);
    close(fd);
}

static void emit(const char *s)
{
    unsigned long n;
    char *q;

    n = strlen(s);
    if (olen + n + 1 > ocap) {
        ocap = (olen + n + 1) * 2;
        q = malloc(ocap);
        if (olen)
            memcpy(q, out, olen);
        out = q;
    }
    memcpy(out + olen, s, n);
    out[olen + n] = '\n';
    olen = olen + n + 1;
}

int main(int argc, char **argv)
{
    char *list;
    char *data;
    unsigned long nl;
    unsigned long n;
    unsigned long i;
    unsigned long j;
    int a;
    int fd;
    int files;

    if (argc < 3 || (argc - 3) % 3 != 0)
        die("usage: seed-tree OUTLIST ROOT [SRCDIR PREFIX LIST]...", "");
    files = 0;
    for (a = 3; a < argc; a = a + 3) {
        list = slurp(argv[a + 2], &nl);
        i = 0;
        while (i < nl) {
            j = i;
            while (j < nl && list[j] != '\n')
                j = j + 1;
            if (j > i) {
                if (j - i + strlen(argv[a + 1]) + 2 > 200)
                    die("path too long in ", argv[a + 2]);
                strcpy(name, argv[a + 1]);
                strcat(name, "/");
                n = strlen(name);
                memcpy(name + n, list + i, j - i);
                name[n + j - i] = 0;
                strcpy(dst, argv[2]);
                strcat(dst, "/");
                strcat(dst, name);
                if (strcmp(argv[a], "-") != 0) {
                    strcpy(src, argv[a]);
                    strcat(src, "/");
                    n = strlen(src);
                    memcpy(src + n, list + i, j - i);
                    src[n + j - i] = 0;
                    data = slurp(src, &n);
                    spill(dst, data, n);
                    free(data);
                } else {
                    fd = open(dst, O_RDONLY, 0);
                    if (fd < 0)
                        die("missing ", dst);
                    close(fd);
                }
                emit(name);
                files = files + 1;
            }
            i = j + 1;
        }
        free(list);
    }
    spill(argv[1], out, olen);
    printf("seed-tree ok %d files\n", files);
    return 0;
}
