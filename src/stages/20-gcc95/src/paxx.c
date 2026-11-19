/* paxx [-C DIR] [-s N] [-X FILE]: extract a POSIX tar archive (ustar with
   pax extended headers) read from stdin, e.g. `gzip -dc A.tar.gz | paxx`
   (20-gcc95 README, rung gmp9). tar 1.12 knows neither pax headers nor the
   ustar prefix field: it writes `PaxHeaders.*` files and cuts long names.

   -C DIR   extract into DIR (made if missing; default the current one)
   -s N     drop the first N components of every name (and hard link
            target); a member left with no name is skipped
   -X FILE  exclude list, one pattern per line, matched against the name
            after -s by fnmatch() without FNM_PATHNAME (`*` also matches
            `/`) and with FNM_LEADING_DIR (a pattern also matches every
            path below it). A line `!PATTERN` keeps a member that an
            exclude pattern matched. Empty lines and `#` lines are ignored.

   Members: pax `x` (for the next member) and `g` (for all later members,
   unless an `x` header says otherwise) headers with the keys `path`,
   `linkpath`, `size` and `mtime` (whole seconds; other keys are ignored);
   the ustar `prefix`; regular files (`0`, NUL), directories (`5`),
   symbolic links (`2`) and hard links (`1`, written as a copy of the
   target, which must have been extracted before). Any other type, a bad
   header checksum, an absolute name or a `..` component is an error that
   names the member. Files and directories get the header's permission
   bits, files also its mtime. At the end:
   `paxx: N files, D dirs, L links, P pax headers` and `paxx: E excluded`
   (L counts symbolic and hard links, E excluded members of any type). */
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

#define PATHMAX 4096

static char inbuf[1 << 18];
static size_t inpos, inlen;
static char **pats;
static int npats;

static void die(const char *what, const char *name)
{
    if (errno)
        fprintf(stderr, "paxx: %s: %s: %s\n", name, what, strerror(errno));
    else
        fprintf(stderr, "paxx: %s: %s\n", name, what);
    exit(1);
}

/* N bytes of stdin into BUF; 0 at a clean end of input */
static int input(char *buf, size_t n)
{
    size_t got = 0;
    while (got < n) {
        size_t k;
        if (inpos == inlen) {
            ssize_t r = read(0, inbuf, sizeof inbuf);
            if (r < 0)
                die("read", "stdin");
            if (r == 0) {
                if (got == 0)
                    return 0;
                errno = 0;
                die("archive ends inside a block", "stdin");
            }
            inpos = 0;
            inlen = (size_t)r;
        }
        k = inlen - inpos;
        if (k > n - got)
            k = n - got;
        memcpy(buf + got, inbuf + inpos, k);
        inpos += k;
        got += k;
    }
    return 1;
}

static void writeall(int fd, const char *p, size_t n, const char *name)
{
    while (n) {
        ssize_t w = write(fd, p, n);
        if (w <= 0)
            die("write", name);
        p += w;
        n -= (size_t)w;
    }
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

/* the keys of one pax header; *_set says the key was given */
struct pax {
    char path[PATHMAX], linkpath[PATHMAX];
    unsigned long long size, mtime;
    int path_set, linkpath_set, size_set, mtime_set;
};

static void setval(char *dst, const char *v, size_t n, const char *name)
{
    if (n >= PATHMAX) {
        errno = 0;
        die("pax value too long", name);
    }
    memcpy(dst, v, n);
    dst[n] = 0;
}

/* records "LEN key=value\n" of a header's data D (N bytes) into X */
static void parse_pax(char *d, size_t n, struct pax *x, const char *name)
{
    size_t off = 0;
    errno = 0;
    while (off < n) {
        size_t len = 0, i = off, klen;
        char *key, *eq, *val;
        while (i < n && d[i] >= '0' && d[i] <= '9')
            len = len * 10 + (size_t)(d[i++] - '0');
        if (len == 0 && off < n && d[off] == 0)
            break; /* NUL padding */
        if (i >= n || d[i] != ' ' || len < 5 || off + len > n || d[off + len - 1] != '\n')
            die("bad pax record", name);
        key = d + i + 1;
        eq = memchr(key, '=', (size_t)(d + off + len - 1 - key));
        if (!eq)
            die("bad pax record", name);
        klen = (size_t)(eq - key);
        val = eq + 1;
        i = (size_t)(d + off + len - 1 - val); /* value length */
        if (klen == 4 && memcmp(key, "path", 4) == 0) {
            setval(x->path, val, i, name);
            x->path_set = 1;
        } else if (klen == 8 && memcmp(key, "linkpath", 8) == 0) {
            setval(x->linkpath, val, i, name);
            x->linkpath_set = 1;
        } else if (klen == 4 && memcmp(key, "size", 4) == 0) {
            x->size = strtoull(val, NULL, 10);
            x->size_set = 1;
        } else if (klen == 5 && memcmp(key, "mtime", 5) == 0) {
            x->mtime = strtoull(val, NULL, 10); /* stops at '.' */
            x->mtime_set = 1;
        }
        off += len;
    }
}

/* NAME without its first N components and leading "./"; NULL if none
   is left. Absolute names and ".." components are errors. */
static char *strip(char *name, int n)
{
    char *p = name, *q;
    if (*p == '/') {
        errno = 0;
        die("absolute name", name);
    }
    for (q = p; *q; ) {
        size_t k = strcspn(q, "/");
        if (k == 2 && q[0] == '.' && q[1] == '.') {
            errno = 0;
            die("name with ..", name);
        }
        q += k;
        if (*q)
            q++;
    }
    while (n-- > 0) {
        p = strchr(p, '/');
        if (!p)
            return NULL;
        p++;
    }
    while (p[0] == '.' && p[1] == '/')
        p += 2;
    q = p + strlen(p);
    while (q > p && q[-1] == '/')
        *--q = 0;
    return *p ? p : NULL;
}

static int excluded(const char *path)
{
    int i, ex = 0;
    for (i = 0; i < npats && !ex; i++)
        if (pats[i][0] != '!' && fnmatch(pats[i], path, FNM_LEADING_DIR) == 0)
            ex = 1;
    for (i = 0; i < npats && ex; i++)
        if (pats[i][0] == '!' && fnmatch(pats[i] + 1, path, FNM_LEADING_DIR) == 0)
            ex = 0;
    return ex;
}

static void read_excludes(const char *file)
{
    static char line[PATHMAX];
    FILE *f = fopen(file, "r");
    if (!f)
        die("open", file);
    while (fgets(line, sizeof line, f)) {
        size_t k = strcspn(line, "\r\n");
        line[k] = 0;
        if (k == 0 || line[0] == '#')
            continue;
        pats = realloc(pats, (size_t)(npats + 1) * sizeof *pats);
        if (!pats || !(pats[npats] = strdup(line)))
            die("out of memory", file);
        npats++;
    }
    fclose(f);
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

int main(int argc, char **argv)
{
    static char hdr[512], full[PATHMAX], lname[PATHMAX], buf[65536];
    static struct pax glob, loc;
    long files = 0, dirs = 0, links = 0, paxh = 0, excl = 0;
    int strip_n = 0, i;
    const char *dir = NULL;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-C") == 0 && i + 1 < argc)
            dir = argv[++i];
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
            strip_n = atoi(argv[++i]);
        else if (strcmp(argv[i], "-X") == 0 && i + 1 < argc)
            read_excludes(argv[++i]);
        else {
            fprintf(stderr, "usage: paxx [-C DIR] [-s N] [-X FILE] < ARCHIVE\n");
            return 2;
        }
    }
    if (dir) {
        strncpy(full, dir, sizeof full - 1);
        mkdirs(full, 1);
        if (chdir(dir) != 0)
            die("chdir", dir);
    }
    for (;;) {
        unsigned long long size, mode, mtime, sum, want = 0, swant = 0, left;
        char type, *path, *link = NULL;
        int out = -1;

        if (!input(hdr, 512))
            break; /* end of input without the zero blocks */
        for (i = 0; i < 512 && hdr[i] == 0; i++)
            ;
        if (i == 512)
            break; /* the first zero block ends the archive */
        if (hdr[345] && memcmp(hdr + 257, "ustar", 6) == 0)
            snprintf(full, sizeof full, "%.155s/%.100s", hdr + 345, hdr);
        else
            snprintf(full, sizeof full, "%.100s", hdr);
        sum = octal(hdr + 148, 8);
        for (i = 0; i < 512; i++) {
            want += (unsigned char)(i >= 148 && i < 156 ? ' ' : hdr[i]);
            swant += (unsigned long long)(long long)(signed char)(i >= 148 && i < 156 ? ' ' : hdr[i]);
        }
        errno = 0;
        if (sum != want && sum != swant)
            die("bad header checksum", full);
        type = hdr[156];
        size = octal(hdr + 124, 12);
        mode = octal(hdr + 100, 8) & 07777;
        mtime = octal(hdr + 136, 12);
        if (type == 'x' || type == 'g') {
            char *d;
            if (size > (1 << 24))
                die("pax header too big", full);
            d = malloc((size_t)size + 512);
            if (!d || !input(d, (size_t)((size + 511) / 512 * 512)))
                die("short pax header", full);
            parse_pax(d, (size_t)size, type == 'g' ? &glob : &loc, full);
            free(d);
            paxh++;
            continue;
        }
        /* the member's keys: its x header, then the g headers, then ustar */
        if (loc.path_set || glob.path_set)
            snprintf(full, sizeof full, "%s", loc.path_set ? loc.path : glob.path);
        if (loc.size_set || glob.size_set)
            size = loc.size_set ? loc.size : glob.size;
        if (loc.mtime_set || glob.mtime_set)
            mtime = loc.mtime_set ? loc.mtime : glob.mtime;
        if (loc.linkpath_set || glob.linkpath_set)
            snprintf(lname, sizeof lname, "%s", loc.linkpath_set ? loc.linkpath : glob.linkpath);
        else
            snprintf(lname, sizeof lname, "%.100s", hdr + 157);
        memset(&loc, 0, sizeof loc);
        if (type == '1' || type == '2' || type == '5')
            size = 0; /* no data, whatever the size field says */
        else if (type != '0' && type != 0) {
            fprintf(stderr, "paxx: %s: unsupported type '%c'\n", full, type);
            exit(1);
        }
        path = strip(full, strip_n);
        if (path && excluded(path)) {
            excl++;
            path = NULL;
        }
        if (path && type == '1') {
            link = strip(lname, strip_n);
            if (!link) {
                errno = 0;
                die("hard link target outside -s", full);
            }
        }
        if (!path) {
            ;
        } else if (type == '5') {
            mkdirs(path, 1);
            chmod(path, (mode_t)mode);
            dirs++;
        } else if (type == '2') {
            mkdirs(path, 0);
            unlink(path);
            if (symlink(lname, path) != 0)
                die("symlink", path);
            links++;
        } else {
            mkdirs(path, 0);
            unlink(path);
            out = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
            if (out < 0)
                die("create", path);
            if (type == '1') {
                int in = open(link, O_RDONLY);
                ssize_t r;
                if (in < 0)
                    die("open hard link target", path);
                while ((r = read(in, buf, sizeof buf)) > 0)
                    writeall(out, buf, (size_t)r, path);
                if (r < 0)
                    die("read hard link target", path);
                close(in);
                links++;
            } else {
                files++;
            }
        }
        for (left = size; left; ) {
            size_t blk = (size_t)((left + 511) / 512 * 512);
            size_t n = blk < sizeof buf ? blk : sizeof buf;
            size_t data = left < n ? (size_t)left : n;
            if (!input(buf, n)) {
                errno = 0;
                die("archive ends inside the data", full);
            }
            if (out >= 0)
                writeall(out, buf, data, path);
            left -= data;
        }
        if (out >= 0) {
            struct utimbuf ut;
            if (close(out) != 0)
                die("close", path);
            if (chmod(path, (mode_t)mode) != 0)
                die("chmod", path);
            ut.actime = ut.modtime = (time_t)mtime;
            if (utime(path, &ut) != 0)
                die("utime", path);
        }
    }
    /* read the rest (zero blocks), so a writer on a pipe gets no SIGPIPE */
    while (read(0, inbuf, sizeof inbuf) > 0)
        ;
    printf("paxx: %ld files, %ld dirs, %ld links, %ld pax headers\n", files, dirs, links, paxh);
    printf("paxx: %ld excluded\n", excl);
    return 0;
}
