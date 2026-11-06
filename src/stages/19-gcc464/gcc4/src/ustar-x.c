/* ustar-x TAR [PATH...]: extract a POSIX ustar archive into the current
   directory, taking the full name as `prefix/name` (tar 1.12 ignores the
   ustar prefix field; 19-gcc464 README, rung gcc4). Only regular files
   (typeflag '0' or NUL) and directories ('5') are written; pax headers
   ('g', 'x') and anything else are skipped with their data. With PATH
   arguments only members whose full name starts with one of them are
   extracted. Parent directories are made as needed, the header's mode and
   mtime are applied. Prints `ustar-x: N files, M dirs, P with prefix`
   (N and M extracted, P members of the whole archive with a prefix). */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

static const char *tarname;

static void die(const char *what, const char *arg)
{
    fprintf(stderr, "ustar-x: %s %s: %s\n", what, arg ? arg : "",
            errno ? strerror(errno) : "");
    exit(1);
}

static unsigned long long octal(const char *p, int n)
{
    unsigned long long v = 0;
    int i;
    for (i = 0; i < n && p[i] == ' '; i++)
        ;
    for (; i < n && p[i] >= '0' && p[i] <= '7'; i++)
        v = v * 8 + (unsigned)(p[i] - '0');
    return v;
}

static void readall(int fd, char *buf, size_t n)
{
    while (n) {
        ssize_t r = read(fd, buf, n);
        if (r <= 0) {
            if (r == 0)
                errno = 0;
            die("short read", tarname);
        }
        buf += r;
        n -= (size_t)r;
    }
}

static void writeall(int fd, const char *buf, size_t n, const char *path)
{
    while (n) {
        ssize_t w = write(fd, buf, n);
        if (w <= 0)
            die("write", path);
        buf += w;
        n -= (size_t)w;
    }
}

/* mkdir -p of the directories above PATH (and PATH itself if ALL) */
static void mkdirs(char *path, int all)
{
    char *s;
    for (s = path + 1; *s; s++) {
        if (*s != '/')
            continue;
        *s = 0;
        if (mkdir(path, 0755) != 0 && errno != EEXIST)
            die("mkdir", path);
        *s = '/';
    }
    if (all && mkdir(path, 0755) != 0 && errno != EEXIST)
        die("mkdir", path);
}

static int wanted(const char *path, int argc, char **argv)
{
    int i;
    if (argc <= 2)
        return 1;
    for (i = 2; i < argc; i++)
        if (strncmp(path, argv[i], strlen(argv[i])) == 0)
            return 1;
    return 0;
}

int main(int argc, char **argv)
{
    static char hdr[512], buf[65536];
    char path[512];
    long files = 0, dirs = 0, prefixed = 0;
    int fd;

    if (argc < 2) {
        fprintf(stderr, "usage: ustar-x TAR [PATH...]\n");
        return 2;
    }
    tarname = argv[1];
    fd = open(tarname, O_RDONLY);
    if (fd < 0)
        die("open", tarname);
    for (;;) {
        unsigned long long size, left, mode, mtime, sum, want = 0;
        char type;
        int i, take, out = -1;

        readall(fd, hdr, 512);
        for (i = 0; i < 512 && hdr[i] == 0; i++)
            ;
        if (i == 512)
            break; /* the first zero block ends the archive */
        sum = octal(hdr + 148, 8);
        for (i = 0; i < 512; i++)
            want += (unsigned char)(i >= 148 && i < 156 ? ' ' : hdr[i]);
        if (sum != want) {
            errno = 0;
            die("bad header checksum in", tarname);
        }
        size = octal(hdr + 124, 12);
        mode = octal(hdr + 100, 8) & 07777;
        mtime = octal(hdr + 136, 12);
        type = hdr[156];
        if (hdr[345]) {
            snprintf(path, sizeof path, "%.155s/%.100s", hdr + 345, hdr);
            prefixed++;
        } else {
            snprintf(path, sizeof path, "%.100s", hdr);
        }
        take = wanted(path, argc, argv) && (type == '0' || type == 0 || type == '5');
        if (take && type == '5') {
            i = (int)strlen(path);
            if (i > 1 && path[i - 1] == '/')
                path[i - 1] = 0;
            mkdirs(path, 1);
            chmod(path, (mode_t)mode);
            dirs++;
        } else if (take) {
            mkdirs(path, 0);
            out = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out < 0)
                die("create", path);
            files++;
        }
        for (left = size; left; ) {
            size_t blk = (size_t)((left + 511) / 512 * 512);
            size_t n = blk < sizeof buf ? blk : sizeof buf;
            size_t data = left < n ? (size_t)left : n;
            readall(fd, buf, n);
            if (out >= 0)
                writeall(out, buf, data, path);
            left -= data;
        }
        if (out >= 0) {
            struct utimbuf ut;
            if (close(out) != 0)
                die("close", path);
            chmod(path, (mode_t)mode);
            ut.actime = ut.modtime = (time_t)mtime;
            utime(path, &ut); /* best effort */
        }
    }
    close(fd);
    printf("ustar-x: %ld files, %ld dirs, %ld with prefix\n", files, dirs, prefixed);
    return 0;
}
