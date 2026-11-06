/* mkustar [-e] OUT ROOT LIST: a POSIX ustar archive OUT of the files named
 * in LIST (one path relative to ROOT per line), each preceded by entries for
 * its parent directories. mtime 0, uid/gid 0, directories 0755, a file
 * with the permission bits of its stat() mode; with -e (for Kronos, whose
 * stat() says 0777 for every file) 0755 for an ELF executable (ET_EXEC),
 * 0644 for the rest. The same input gives the same bytes. Names must fit
 * the 100-byte name field. Prints "mkustar ok N files". */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int ofd;
static char hdr[512];
static char zero[512];
static char dirs[64][100];
static int ndirs;

static void die(const char *a, const char *b)
{
    printf("mkustar: %s%s\n", a, b);
    exit(1);
}

/* a write may be short (Tilck: at most 12 KB per call) */
static void put(const char *p, unsigned long n)
{
    long r;

    while (n > 0) {
        r = write(ofd, p, n);
        if (r <= 0)
            die("short write", "");
        p = p + r;
        n = n - r;
    }
}

/* n as zero-padded octal in w-1 digits and a NUL */
static void octal(char *f, int w, unsigned long n)
{
    int i;

    f[w - 1] = 0;
    for (i = w - 2; i >= 0; i = i - 1) {
        f[i] = (char)('0' + (n & 7));
        n = n >> 3;
    }
}

static void header(const char *name, unsigned long size, char type,
                   unsigned long mode)
{
    unsigned long sum;
    int i;

    if (strlen(name) >= 100)
        die("name too long ", name);
    memset(hdr, 0, sizeof(hdr));
    strcpy(hdr, name);
    octal(hdr + 100, 8, mode);
    octal(hdr + 108, 8, 0);
    octal(hdr + 116, 8, 0);
    octal(hdr + 124, 12, size);
    octal(hdr + 136, 12, 0);
    memset(hdr + 148, ' ', 8);
    hdr[156] = type;
    memcpy(hdr + 257, "ustar", 6);
    memcpy(hdr + 263, "00", 2);
    sum = 0;
    for (i = 0; i < 512; i = i + 1)
        sum = sum + (unsigned char)hdr[i];
    octal(hdr + 148, 7, sum);
    hdr[155] = ' ';
    put(hdr, 512);
}

/* entries for the parent directories of name not written yet */
static void parents(const char *name)
{
    char d[100];
    unsigned long i;
    int k;

    for (i = 0; name[i] && i < 98; i = i + 1) {
        if (name[i] != '/')
            continue;
        memcpy(d, name, i + 1);
        d[i + 1] = 0;
        for (k = 0; k < ndirs; k = k + 1)
            if (strcmp(dirs[k], d) == 0)
                break;
        if (k < ndirs)
            continue;
        if (ndirs == 64)
            die("too many directories", "");
        strcpy(dirs[ndirs], d);
        ndirs = ndirs + 1;
        header(d, 0, '5', 0755);
    }
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

static int is_exec(const char *p, unsigned long n)
{
    return n >= 18 && memcmp(p, "\177ELF", 4) == 0 && p[16] == 2 && p[17] == 0;
}

int main(int argc, char **argv)
{
    char path[512];
    char name[256];
    struct stat st;
    char *list;
    char *data;
    unsigned long nl;
    unsigned long n;
    unsigned long i;
    unsigned long j;
    unsigned long mode;
    int files;
    int bycontent;

    bycontent = argc == 5 && strcmp(argv[1], "-e") == 0;
    if (bycontent) {
        argc = argc - 1;
        argv = argv + 1;
    }
    if (argc != 4)
        die("usage: mkustar [-e] OUT ROOT LIST", "");
    list = slurp(argv[3], &nl);
    ofd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (ofd < 0)
        die("cannot create ", argv[1]);
    files = 0;
    i = 0;
    while (i < nl) {
        j = i;
        while (j < nl && list[j] != '\n')
            j = j + 1;
        if (j > i) {
            if (j - i >= sizeof(name) || strlen(argv[2]) + (j - i) + 2 > sizeof(path))
                die("path too long in ", argv[3]);
            memcpy(name, list + i, j - i);
            name[j - i] = 0;
            strcpy(path, argv[2]);
            strcat(path, "/");
            strcat(path, name);
            data = slurp(path, &n);
            if (bycontent)
                mode = is_exec(data, n) ? 0755 : 0644;
            else if (stat(path, &st) == 0)
                mode = st.st_mode & 07777;
            else
                die("cannot stat ", path);
            parents(name);
            header(name, n, '0', mode);
            put(data, n);
            if (n & 511)
                put(zero, 512 - (n & 511));
            free(data);
            files = files + 1;
        }
        i = j + 1;
    }
    put(zero, 512);
    put(zero, 512);
    close(ofd);
    printf("mkustar ok %d files\n", files);
    return 0;
}
