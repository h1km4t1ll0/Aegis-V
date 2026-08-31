/* Freestanding POSIX layer. No #include: Simon compile uses -nostdinc. */

typedef char *va_list;
#define va_arg(ap, type) (*(type *)((ap += 8) - 8))
#define va_end(ap) ((void)0)

#define NULL ((void *)0)
#define AT_FDCWD (-100)
#define SYS_getcwd 17
#define SYS_fcntl 25
#define SYS_openat 56
#define SYS_close 57
#define SYS_lseek 62
#define SYS_read 63
#define SYS_write 64
#define SYS_exit 93
#define SYS_brk 214
#define SYS_munmap 215
#define SYS_mmap 222
#define SYS_mprotect 226
#define SYS_gettimeofday 169
#define SYS_clock_gettime 113
#define SYS_uname 160
#define SYS_faccessat 48
#define SYS_mkdirat 34
#define SYS_newfstatat 79
#define SYS_fstat 80
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 64
#define O_TRUNC 512
#define O_APPEND 1024
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define MAP_ANONYMOUS 0x20
#define MAP_PRIVATE 0x02
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4

struct FILE {
    int fd;
};

typedef struct FILE FILE;

long ecall6(long n, long a, long b, long c, long d, long e, long f);

static FILE _stdin;
static FILE _stdout;
static FILE _stderr;
FILE *stdin;
FILE *stdout;
FILE *stderr;
int errno;
static unsigned long heap_cur;
static unsigned long heap_end;
static int libc_inited;
static struct FILE files[16];
static int files_used[16];
static char errbuf[32];
static int rand_state;

static void libc_init(void)
{
    if (libc_inited)
        return;
    libc_inited = 1;
    _stdin.fd = 0;
    _stdout.fd = 1;
    _stderr.fd = 2;
    stdin = &_stdin;
    stdout = &_stdout;
    stderr = &_stderr;
    heap_cur = 0;
    heap_end = 0;
}

static long sysret(long r)
{
    if (r < 0 && r > -4096) {
        errno = (int)(0 - r);
        return -1;
    }
    return r;
}

void *memcpy(void *d, const void *s, unsigned long n)
{
    unsigned char *dd = d;
    const unsigned char *ss = s;
    while (n) {
        *dd = *ss;
        dd = dd + 1;
        ss = ss + 1;
        n = n - 1;
    }
    return d;
}

void *memset(void *d, int c, unsigned long n)
{
    unsigned char *dd = d;
    while (n) {
        *dd = (unsigned char)c;
        dd = dd + 1;
        n = n - 1;
    }
    return d;
}

void *memmove(void *d, const void *s, unsigned long n)
{
    unsigned char *dd = d;
    const unsigned char *ss = s;
    if (dd < ss) {
        return memcpy(d, s, n);
    }
    dd = dd + n;
    ss = ss + n;
    while (n) {
        dd = dd - 1;
        ss = ss - 1;
        *dd = *ss;
        n = n - 1;
    }
    return d;
}

int memcmp(const void *a, const void *b, unsigned long n)
{
    const unsigned char *aa = a;
    const unsigned char *bb = b;
    while (n) {
        if (*aa != *bb)
            return (int)*aa - (int)*bb;
        aa = aa + 1;
        bb = bb + 1;
        n = n - 1;
    }
    return 0;
}

void *memchr(const void *s, int c, unsigned long n)
{
    const unsigned char *p = s;
    while (n) {
        if (*p == (unsigned char)c)
            return (void *)p;
        p = p + 1;
        n = n - 1;
    }
    return 0;
}

unsigned long strlen(const char *s)
{
    unsigned long n = 0;
    while (s[n])
        n = n + 1;
    return n;
}

char *strcpy(char *d, const char *s)
{
    char *o = d;
    while (*s) {
        *d = *s;
        d = d + 1;
        s = s + 1;
    }
    *d = 0;
    return o;
}

char *strncpy(char *d, const char *s, unsigned long n)
{
    char *o = d;
    while (n) {
        *d = *s;
        if (*s == 0)
            break;
        d = d + 1;
        s = s + 1;
        n = n - 1;
    }
    while (n) {
        *d = 0;
        d = d + 1;
        n = n - 1;
    }
    return o;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a = a + 1;
        b = b + 1;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char *a, const char *b, unsigned long n)
{
    while (n && *a && *a == *b) {
        a = a + 1;
        b = b + 1;
        n = n - 1;
    }
    if (n == 0)
        return 0;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

unsigned long strcspn(const char *s, const char *reject)
{
    unsigned long n = 0;
    while (s[n]) {
        const char *r = reject;
        while (*r) {
            if (s[n] == *r)
                return n;
            r = r + 1;
        }
        n = n + 1;
    }
    return n;
}

char *strcat(char *d, const char *s)
{
    char *o = d;
    while (*d)
        d = d + 1;
    strcpy(d, s);
    return o;
}

char *strncat(char *d, const char *s, unsigned long n)
{
    char *o = d;
    while (*d)
        d = d + 1;
    while (n && *s) {
        *d = *s;
        d = d + 1;
        s = s + 1;
        n = n - 1;
    }
    *d = 0;
    return o;
}

char *strchr(const char *s, int c)
{
    while (*s) {
        if (*s == (char)c)
            return (char *)s;
        s = s + 1;
    }
    if ((char)c == 0)
        return (char *)s;
    return 0;
}

char *strrchr(const char *s, int c)
{
    const char *r = 0;
    while (*s) {
        if (*s == (char)c)
            r = s;
        s = s + 1;
    }
    if ((char)c == 0)
        return (char *)s;
    return (char *)r;
}

char *strstr(const char *h, const char *n)
{
    unsigned long nl;
    if (!n[0])
        return (char *)h;
    nl = strlen(n);
    while (*h) {
        if (*h == *n && strncmp(h, n, nl) == 0)
            return (char *)h;
        h = h + 1;
    }
    return 0;
}

static int cfold(int c)
{
    if (c >= 'A' && c <= 'Z')
        return c + 32;
    return c;
}

int strcasecmp(const char *a, const char *b)
{
    while (*a && cfold(*a) == cfold(*b)) {
        a = a + 1;
        b = b + 1;
    }
    return cfold((int)(unsigned char)*a) - cfold((int)(unsigned char)*b);
}

int strncasecmp(const char *a, const char *b, unsigned long n)
{
    while (n && *a && cfold(*a) == cfold(*b)) {
        a = a + 1;
        b = b + 1;
        n = n - 1;
    }
    if (n == 0)
        return 0;
    return cfold((int)(unsigned char)*a) - cfold((int)(unsigned char)*b);
}

char *strerror(int e)
{
    errbuf[0] = 'e';
    errbuf[1] = 0;
    (void)e;
    return errbuf;
}

char *strsignal(int sig)
{
    (void)sig;
    return errbuf;
}

long write(int fd, const void *buf, unsigned long n)
{
    return sysret(ecall6(SYS_write, fd, (long)buf, (long)n, 0, 0, 0));
}

long read(int fd, void *buf, unsigned long n)
{
    return sysret(ecall6(SYS_read, fd, (long)buf, (long)n, 0, 0, 0));
}

int close(int fd)
{
    return (int)sysret(ecall6(SYS_close, fd, 0, 0, 0, 0, 0));
}

long lseek(int fd, long off, int whence)
{
    return sysret(ecall6(SYS_lseek, fd, off, whence, 0, 0, 0));
}

int open(const char *path, int flags, int mode)
{
    if ((flags & O_CREAT) == 0)
        mode = 0;
    return (int)sysret(ecall6(SYS_openat, AT_FDCWD, (long)path, flags, mode, 0, 0));
}

void *brk(void *addr)
{
    return (void *)ecall6(SYS_brk, (long)addr, 0, 0, 0, 0, 0);
}

void exit(int n)
{
    ecall6(SYS_exit, n, 0, 0, 0, 0, 0);
    for (;;)
        ;
}

void _exit(int n)
{
    exit(n);
}

void abort(void)
{
    write(2, "abort\n", 6);
    exit(1);
}

const char *libiberty_optr;
char *libiberty_nptr;
unsigned long libiberty_len;

void __main(void)
{
}

static int heap_ok(unsigned long p)
{
    if (p < 0x10000)
        return 0;
    if (p >= 0x1C000000UL)
        return 0;
    return 1;
}

static int heap_grow(unsigned long need)
{
    unsigned long n;
    unsigned long p;
    unsigned long b;
    libc_init();
    b = (unsigned long)brk(0);
    if (!heap_ok(heap_cur) || !heap_ok(heap_end)) {
        heap_cur = b;
        heap_end = b;
    }
    if (heap_cur + need <= heap_end)
        return 0;
    n = need + 65536;
    p = (unsigned long)brk((void *)(heap_end + n));
    if (p <= heap_end)
        return -1;
    heap_end = p;
    return 0;
}

void *malloc(unsigned long n)
{
    unsigned long p;
    if (n == 0)
        n = 1;
    n = n + 15;
    n = n >> 4;
    n = n << 4;
    if (heap_grow(n))
        return 0;
    p = heap_cur;
    heap_cur = heap_cur + n;
    return (void *)p;
}

void free(void *p)
{
    (void)p;
}

void *calloc(unsigned long a, unsigned long b)
{
    unsigned long n;
    void *p;
    n = a * b;
    p = malloc(n);
    if (p)
        memset(p, 0, n);
    return p;
}

void *realloc(void *p, unsigned long n)
{
    void *q;
    if (!p)
        return malloc(n);
    q = malloc(n);
    if (!q)
        return 0;
    memcpy(q, p, n);
    return q;
}

char *strdup(const char *s)
{
    unsigned long n;
    char *p;
    n = strlen(s) + 1;
    p = malloc(n);
    if (!p)
        return 0;
    memcpy(p, s, n);
    return p;
}

static int digit(int c, int base)
{
    int v;
    if (c >= '0' && c <= '9')
        v = c - '0';
    else if (c >= 'a' && c <= 'z')
        v = c - 'a' + 10;
    else if (c >= 'A' && c <= 'Z')
        v = c - 'A' + 10;
    else
        return -1;
    if (v >= base)
        return -1;
    return v;
}

unsigned long strtoul(const char *s, char **end, int base)
{
    unsigned long v;
    int d;
    int neg;
    const char *p;
    p = s;
    neg = 0;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p = p + 1;
    if (*p == '+')
        p = p + 1;
    else if (*p == '-') {
        neg = 1;
        p = p + 1;
    }
    if (base == 0) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            base = 16;
            p = p + 2;
        } else if (p[0] == '0')
            base = 8;
        else
            base = 10;
    } else if (base == 16 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
        p = p + 2;
    v = 0;
    d = digit((int)*p, base);
    if (d < 0) {
        if (end)
            *end = (char *)s;
        return 0;
    }
    while (d >= 0) {
        v = v * (unsigned long)base + (unsigned long)d;
        p = p + 1;
        d = digit((int)*p, base);
    }
    if (end)
        *end = (char *)p;
    if (neg)
        return 0 - v;
    return v;
}

long strtol(const char *s, char **end, int base)
{
    return (long)strtoul(s, end, base);
}

long long strtoll(const char *s, char **end, int base)
{
    return (long long)strtoul(s, end, base);
}

unsigned long long strtoull(const char *s, char **end, int base)
{
    return (unsigned long long)strtoul(s, end, base);
}

int atoi(const char *s)
{
    return (int)strtol(s, 0, 10);
}

int strtod(const char *s, char **end)
{
    return (int)strtol(s, end, 10);
}

int strtof(const char *s, char **end)
{
    return strtod(s, end);
}

int strtold(const char *s, char **end)
{
    return strtod(s, end);
}

int atof(const char *s)
{
    return strtod(s, 0);
}

static void put_num(char *dst, unsigned long *pos, unsigned long max,
                    unsigned long v, int base, int width, int zpad, int up)
{
    char tmp[32];
    int i;
    const char *dig;
    dig = up ? "0123456789ABCDEF" : "0123456789abcdef";
    i = 0;
    if (v == 0) {
        tmp[i] = '0';
        i = i + 1;
    }
    while (v) {
        tmp[i] = dig[v % (unsigned long)base];
        i = i + 1;
        v = v / (unsigned long)base;
    }
    while (width > i) {
        if (*pos + 1 < max)
            dst[*pos] = zpad ? '0' : ' ';
        *pos = *pos + 1;
        width = width - 1;
    }
    while (i > 0) {
        i = i - 1;
        if (*pos + 1 < max)
            dst[*pos] = tmp[i];
        *pos = *pos + 1;
    }
}

int vsnprintf(char *s, unsigned long n, const char *fmt, va_list ap)
{
    unsigned long pos;
    const char *p;
    int width;
    int zpad;
    int left;
    int lng;
    unsigned long u;
    long d;
    const char *str;
    pos = 0;
    if (n == 0)
        return 0;
    p = fmt;
    while (*p) {
        if (*p != '%') {
            if (pos + 1 < n)
                s[pos] = *p;
            pos = pos + 1;
            p = p + 1;
            continue;
        }
        p = p + 1;
        width = 0;
        zpad = 0;
        left = 0;
        lng = 0;
        while (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0') {
            if (*p == '-')
                left = 1;
            if (*p == '0')
                zpad = 1;
            p = p + 1;
        }
        if (*p == '*') {
            width = va_arg(ap, int);
            if (width < 0) {
                left = 1;
                width = 0 - width;
            }
            p = p + 1;
        }
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p = p + 1;
        }
        if (*p == '.') {
            p = p + 1;
            if (*p == '*') {
                (void)va_arg(ap, int);
                p = p + 1;
            } else {
                while (*p >= '0' && *p <= '9')
                    p = p + 1;
            }
        }
        while (*p == 'l' || *p == 'z' || *p == 't') {
            if (*p == 'l')
                lng = lng + 1;
            p = p + 1;
        }
        if (*p == 's') {
            unsigned long sl;
            int pad;
            str = va_arg(ap, char *);
            if (!str)
                str = "(null)";
            sl = strlen(str);
            pad = width - (int)sl;
            if (!left) {
                while (pad > 0) {
                    if (pos + 1 < n)
                        s[pos] = ' ';
                    pos = pos + 1;
                    pad = pad - 1;
                }
            }
            while (*str) {
                if (pos + 1 < n)
                    s[pos] = *str;
                pos = pos + 1;
                str = str + 1;
            }
            if (left) {
                while (pad > 0) {
                    if (pos + 1 < n)
                        s[pos] = ' ';
                    pos = pos + 1;
                    pad = pad - 1;
                }
            }
        } else if (*p == 'n') {
            int *np;
            np = va_arg(ap, int *);
            if (np)
                *np = (int)pos;
        } else if (*p == 'c') {
            if (pos + 1 < n)
                s[pos] = (char)va_arg(ap, int);
            pos = pos + 1;
        } else if (*p == '%') {
            if (pos + 1 < n)
                s[pos] = '%';
            pos = pos + 1;
        } else if (*p == 'd' || *p == 'i') {
            d = lng ? va_arg(ap, long) : (long)va_arg(ap, int);
            if (d < 0) {
                if (pos + 1 < n)
                    s[pos] = '-';
                pos = pos + 1;
                u = (unsigned long)(0 - d);
            } else
                u = (unsigned long)d;
            put_num(s, &pos, n, u, 10, width, zpad, 0);
        } else if (*p == 'u') {
            u = lng ? va_arg(ap, unsigned long) : (unsigned long)va_arg(ap, unsigned int);
            put_num(s, &pos, n, u, 10, width, zpad, 0);
        } else if (*p == 'x' || *p == 'X') {
            u = lng ? va_arg(ap, unsigned long) : (unsigned long)va_arg(ap, unsigned int);
            put_num(s, &pos, n, u, 16, width, zpad, *p == 'X');
        } else if (*p == 'o') {
            u = lng ? va_arg(ap, unsigned long) : (unsigned long)va_arg(ap, unsigned int);
            put_num(s, &pos, n, u, 8, width, zpad, 0);
        } else if (*p == 'p') {
            u = (unsigned long)va_arg(ap, void *);
            put_num(s, &pos, n, u, 16, width, zpad, 0);
        } else {
            if (pos + 1 < n)
                s[pos] = *p;
            pos = pos + 1;
        }
        if (*p)
            p = p + 1;
    }
    if (pos < n)
        s[pos] = 0;
    else
        s[n - 1] = 0;
    return (int)pos;
}

int snprintf(char *s, unsigned long n, const char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7)
{
    long args[7];
    args[0] = a1;
    args[1] = a2;
    args[2] = a3;
    args[3] = a4;
    args[4] = a5;
    args[5] = a6;
    args[6] = a7;
    return vsnprintf(s, n, fmt, (va_list)args);
}

int vsprintf(char *s, const char *fmt, va_list ap)
{
    return vsnprintf(s, 1UL << 20, fmt, ap);
}

int sprintf(char *s, const char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7)
{
    return snprintf(s, 1UL << 20, fmt, a1, a2, a3, a4, a5, a6, a7);
}

int vfprintf(FILE *f, const char *fmt, va_list ap)
{
    char buf[1024];
    int r;
    int fd;
    r = vsnprintf(buf, 1024, fmt, ap);
    fd = 1;
    if (f)
        fd = f->fd;
    if (r > 0)
        write(fd, buf, (unsigned long)r);
    return r;
}

int fprintf(FILE *f, const char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7)
{
    long args[7];
    args[0] = a1;
    args[1] = a2;
    args[2] = a3;
    args[3] = a4;
    args[4] = a5;
    args[5] = a6;
    args[6] = a7;
    return vfprintf(f, fmt, (va_list)args);
}

int printf(const char *fmt, long a1, long a2, long a3, long a4, long a5, long a6, long a7)
{
    libc_init();
    return fprintf(stdout, fmt, a1, a2, a3, a4, a5, a6, a7);
}

int puts(const char *s)
{
    int fd;
    libc_init();
    fd = stdout ? stdout->fd : 1;
    write(fd, s, strlen(s));
    write(fd, "\n", 1);
    return 1;
}

int putchar(int c)
{
    char ch;
    int fd;
    libc_init();
    fd = stdout ? stdout->fd : 1;
    ch = (char)c;
    write(fd, &ch, 1);
    return c;
}

int getchar(void)
{
    char ch;
    if (read(0, &ch, 1) != 1)
        return -1;
    return (unsigned char)ch;
}

int sscanf(const char *s, const char *fmt, void *p1, void *p2, void *p3, void *p4, void *p5, void *p6, void *p7)
{
    void *ps[7];
    va_list ap;
    int n;
    int *ip;
    const char *p;
    const char *q;
    n = 0;
    p = s;
    q = fmt;
    ps[0] = p1;
    ps[1] = p2;
    ps[2] = p3;
    ps[3] = p4;
    ps[4] = p5;
    ps[5] = p6;
    ps[6] = p7;
    ap = (char *)ps;
    while (*q) {
        if (*q == '%' && q[1] == 'd') {
            while (*p == ' ' || *p == '.')
                p = p + 1;
            ip = va_arg(ap, int *);
            *ip = (int)strtol(p, (char **)&p, 10);
            n = n + 1;
            q = q + 2;
            continue;
        }
        if (*q == *p) {
            q = q + 1;
            p = p + 1;
            continue;
        }
        break;
    }
    return n;
}

static FILE *alloc_file(int fd)
{
    int i;
    libc_init();
    i = 0;
    while (i < 16) {
        if (!files_used[i]) {
            files_used[i] = 1;
            files[i].fd = fd;
            return &files[i];
        }
        i = i + 1;
    }
    return 0;
}

FILE *fopen(const char *path, const char *mode)
{
    int flags;
    int fd;
    FILE *f;
    libc_init();
    flags = O_RDONLY;
    if (mode) {
        if (mode[0] == 'w')
            flags = O_WRONLY | O_CREAT | O_TRUNC;
        else if (mode[0] == 'a')
            flags = O_WRONLY | O_CREAT | O_APPEND;
        else if (mode[0] == 'r' && mode[1] == '+')
            flags = O_RDWR;
        else if (mode[0] == 'w' && mode[1] == '+')
            flags = O_RDWR | O_CREAT | O_TRUNC;
    }
    fd = open(path, flags, 0777);
    if (fd < 0)
        return 0;
    f = alloc_file(fd);
    if (!f) {
        close(fd);
        return 0;
    }
    return f;
}

FILE *fdopen(int fd, const char *mode)
{
    (void)mode;
    return alloc_file(fd);
}

FILE *fopen_unlocked(const char *path, const char *mode)
{
    return fopen(path, mode);
}

FILE *fdopen_unlocked(int fd, const char *mode)
{
    return fdopen(fd, mode);
}

FILE *freopen_unlocked(const char *path, const char *mode, FILE *f)
{
    (void)f;
    return fopen(path, mode);
}

int fclose(FILE *f)
{
    int i;
    if (!f)
        return -1;
    close(f->fd);
    i = 0;
    while (i < 16) {
        if (&files[i] == f) {
            files_used[i] = 0;
            return 0;
        }
        i = i + 1;
    }
    return 0;
}

int fflush(FILE *f)
{
    (void)f;
    return 0;
}

void setbuf(FILE *f, char *buf)
{
    (void)f;
    (void)buf;
}

unsigned long fread(void *p, unsigned long sz, unsigned long n, FILE *f)
{
    long r;
    unsigned long want;
    if (!f || sz == 0)
        return 0;
    want = sz * n;
    r = read(f->fd, p, want);
    if (r <= 0)
        return 0;
    return (unsigned long)r / sz;
}

unsigned long fwrite(const void *p, unsigned long sz, unsigned long n, FILE *f)
{
    long r;
    unsigned long want;
    if (!f || sz == 0)
        return 0;
    want = sz * n;
    r = write(f->fd, p, want);
    if (r <= 0)
        return 0;
    return (unsigned long)r / sz;
}

int fseek(FILE *f, long off, int whence)
{
    if (!f)
        return -1;
    if (lseek(f->fd, off, whence) < 0)
        return -1;
    return 0;
}

long ftell(FILE *f)
{
    if (!f)
        return -1;
    return lseek(f->fd, 0, SEEK_CUR);
}

int fgetc(FILE *f)
{
    unsigned char c;
    long r;
    if (!f)
        return -1;
    r = read(f->fd, &c, 1);
    if (r != 1)
        return -1;
    return c;
}

int getc(FILE *f)
{
    return fgetc(f);
}

int ungetc(int c, FILE *f)
{
    if (!f)
        return -1;
    lseek(f->fd, -1, SEEK_CUR);
    (void)c;
    return c;
}

int fputc(int c, FILE *f)
{
    unsigned char ch;
    if (!f)
        return -1;
    ch = (unsigned char)c;
    if (write(f->fd, &ch, 1) != 1)
        return -1;
    return c;
}

int putc(int c, FILE *f)
{
    return fputc(c, f);
}

int fputs(const char *s, FILE *f)
{
    if (!f)
        return -1;
    write(f->fd, s, strlen(s));
    return 0;
}

char *fgets(char *s, int n, FILE *f)
{
    int i;
    int c;
    if (!f || n <= 0)
        return 0;
    i = 0;
    while (i < n - 1) {
        c = fgetc(f);
        if (c < 0)
            break;
        s[i] = (char)c;
        i = i + 1;
        if (c == '\n')
            break;
    }
    if (i == 0)
        return 0;
    s[i] = 0;
    return s;
}

int fileno(FILE *f)
{
    return f ? f->fd : -1;
}

int ferror(FILE *f)
{
    (void)f;
    return 0;
}

int feof(FILE *f)
{
    (void)f;
    return 0;
}

void clearerr(FILE *f)
{
    (void)f;
}

void perror(const char *s)
{
    write(2, s ? s : "error", s ? strlen(s) : 5);
    write(2, "\n", 1);
}

int remove(const char *path)
{
    return unlink(path);
}

int rename(const char *a, const char *b)
{
    (void)a;
    (void)b;
    return -1;
}

int unlink(const char *path)
{
    (void)path;
    return 0;
}

char *getcwd(char *buf, unsigned long size)
{
    long r;
    r = ecall6(SYS_getcwd, (long)buf, (long)size, 0, 0, 0, 0);
    if (r < 0)
        return 0;
    return buf;
}

char *getpwd(void)
{
    static char pwd[256];
    if (!getcwd(pwd, 256))
        return 0;
    return pwd;
}

int isatty(int fd)
{
    (void)fd;
    return 0;
}

int getpid(void)
{
    return 1;
}

char *getenv(const char *name)
{
    (void)name;
    return 0;
}

int abs(int n)
{
    return n < 0 ? 0 - n : n;
}

long labs(long n)
{
    return n < 0 ? 0 - n : n;
}

void qsort(void *base, unsigned long nmemb, unsigned long size,
           int (*cmp)(const void *, const void *))
{
    unsigned char *p;
    unsigned char tmp[256];
    unsigned long i;
    unsigned long j;
    p = base;
    if (size > 256)
        return;
    i = 1;
    while (i < nmemb) {
        memcpy(tmp, p + i * size, size);
        j = i;
        while (j > 0 && cmp(p + (j - 1) * size, tmp) > 0) {
            memcpy(p + j * size, p + (j - 1) * size, size);
            j = j - 1;
        }
        memcpy(p + j * size, tmp, size);
        i = i + 1;
    }
}

int system(const char *cmd)
{
    (void)cmd;
    return -1;
}

int atexit(void (*fn)(void))
{
    (void)fn;
    return 0;
}

int rand(void)
{
    rand_state = rand_state * 1103515245 + 12345;
    return (rand_state >> 16) & 32767;
}

void srand(unsigned s)
{
    rand_state = (int)s;
}

int setjmp(int *buf)
{
    (void)buf;
    return 0;
}

void longjmp(int *buf, int val)
{
    (void)buf;
    (void)val;
    /* tcc error path: no real jmp_buf restore */
    exit(1);
}

int isspace(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

int isdigit(int c)
{
    return c >= '0' && c <= '9';
}

int isxdigit(int c)
{
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int isalpha(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int isalnum(int c)
{
    return isalpha(c) || isdigit(c);
}

int isprint(int c)
{
    return c >= 32 && c < 127;
}

int toupper(int c)
{
    if (c >= 'a' && c <= 'z')
        return c - 32;
    return c;
}

int tolower(int c)
{
    if (c >= 'A' && c <= 'Z')
        return c + 32;
    return c;
}

int fabs(int x)
{
    return x < 0 ? 0 - x : x;
}

int ldexp(int x, int exp)
{
    (void)exp;
    return x;
}

int frexp(int x, int *exp)
{
    if (exp)
        *exp = 0;
    return x;
}

int gettimeofday(void *tv, void *tz)
{
    return (int)sysret(ecall6(SYS_gettimeofday, (long)tv, (long)tz, 0, 0, 0, 0));
}

int clock_gettime(int clk, void *ts)
{
    return (int)sysret(ecall6(SYS_clock_gettime, clk, (long)ts, 0, 0, 0, 0));
}

long time(long *t)
{
    long tv[2];
    gettimeofday(tv, 0);
    if (t)
        *t = tv[0];
    return tv[0];
}

void *localtime(const long *t)
{
    static int tm[9];
    (void)t;
    memset(tm, 0, sizeof(tm));
    tm[4] = 0;
    tm[5] = 126;
    return tm;
}

void *gmtime(const long *t)
{
    return localtime(t);
}

int fcntl(int fd, int cmd, ...)
{
    return (int)sysret(ecall6(SYS_fcntl, fd, cmd, 0, 0, 0, 0));
}

int fstat(int fd, void *st)
{
    return (int)sysret(ecall6(SYS_fstat, fd, (long)st, 0, 0, 0, 0));
}

int stat(const char *path, void *st)
{
    return (int)sysret(ecall6(SYS_newfstatat, AT_FDCWD, (long)path, (long)st, 0, 0, 0));
}

int access(const char *path, int mode)
{
    return (int)sysret(ecall6(SYS_faccessat, AT_FDCWD, (long)path, mode, 0, 0, 0));
}

int getpagesize(void)
{
    return 4096;
}

void bcopy(const void *s, void *d, unsigned long n)
{
    memmove(d, s, n);
}

char *strpbrk(const char *s, const char *accept)
{
    unsigned long i;
    unsigned long j;
    if (!s || !accept)
        return 0;
    i = 0;
    while (s[i]) {
        j = 0;
        while (accept[j]) {
            if (s[i] == accept[j])
                return (char *)(s + i);
            j = j + 1;
        }
        i = i + 1;
    }
    return 0;
}

unsigned long strspn(const char *s, const char *accept)
{
    unsigned long i;
    unsigned long j;
    int ok;
    i = 0;
    if (!s)
        return 0;
    while (s[i]) {
        ok = 0;
        j = 0;
        if (accept) {
            while (accept[j]) {
                if (s[i] == accept[j])
                    ok = 1;
                j = j + 1;
            }
        }
        if (!ok)
            return i;
        i = i + 1;
    }
    return i;
}

void *bsearch(const void *key, const void *base, unsigned long n, unsigned long sz,
    int (*cmp)(const void *, const void *))
{
    unsigned long lo;
    unsigned long hi;
    unsigned long mid;
    int c;
    const unsigned char *p;
    if (!key || !base || !cmp || sz == 0)
        return 0;
    lo = 0;
    hi = n;
    p = (const unsigned char *)base;
    while (lo < hi) {
        mid = lo + (hi - lo) / 2;
        c = cmp(key, p + mid * sz);
        if (c == 0)
            return (void *)(p + mid * sz);
        if (c < 0)
            hi = mid;
        else
            lo = mid + 1;
    }
    return 0;
}

void *mmap(void *addr, unsigned long len, int prot, int flags, int fd, long off)
{
    long r;
    r = ecall6(SYS_mmap, (long)addr, (long)len, prot, flags, fd, off);
    if (r < 0 && r > -4096)
        return (void *)-1;
    return (void *)r;
}

int munmap(void *addr, unsigned long len)
{
    return (int)sysret(ecall6(SYS_munmap, (long)addr, (long)len, 0, 0, 0, 0));
}

int mprotect(void *addr, unsigned long len, int prot)
{
    return (int)sysret(ecall6(SYS_mprotect, (long)addr, (long)len, prot, 0, 0, 0));
}

void *sbrk(long inc)
{
    unsigned long cur;
    libc_init();
    cur = (unsigned long)brk(0);
    if (inc == 0)
        return (void *)cur;
    return brk((void *)(cur + (unsigned long)inc));
}

void *alloca(unsigned long n)
{
    return malloc(n);
}

void __arm64_clear_cache(void *b, void *e)
{
    (void)b;
    (void)e;
}

void __clear_cache(void *b, void *e)
{
    (void)b;
    (void)e;
}

void *__memmove(void *d, const void *s, unsigned long n)
{
    return memmove(d, s, n);
}

void *__memset(void *d, int c, unsigned long n)
{
    return memset(d, c, n);
}

struct f128 {
    unsigned long lo;
    unsigned long hi;
};

static struct f128 f128_zero(void)
{
    struct f128 z;
    z.lo = 0;
    z.hi = 0;
    return z;
}

struct f128 __addtf3(struct f128 a, struct f128 b)
{
    (void)b;
    return a;
}

struct f128 __subtf3(struct f128 a, struct f128 b)
{
    (void)b;
    return a;
}

struct f128 __multf3(struct f128 a, struct f128 b)
{
    (void)b;
    return a;
}

struct f128 __divtf3(struct f128 a, struct f128 b)
{
    (void)b;
    return a;
}

int __eqtf2(struct f128 a, struct f128 b)
{
    if (a.lo == b.lo && a.hi == b.hi)
        return 0;
    return 1;
}

int __netf2(struct f128 a, struct f128 b)
{
    return __eqtf2(a, b);
}

int __lttf2(struct f128 a, struct f128 b)
{
    (void)a;
    (void)b;
    return 1;
}

int __letf2(struct f128 a, struct f128 b)
{
    (void)a;
    (void)b;
    return 1;
}

int __gttf2(struct f128 a, struct f128 b)
{
    (void)a;
    (void)b;
    return 1;
}

int __getf2(struct f128 a, struct f128 b)
{
    (void)a;
    (void)b;
    return 1;
}

struct f128 __floatsitf(int x)
{
    (void)x;
    return f128_zero();
}

struct f128 __floatunsitf(unsigned x)
{
    (void)x;
    return f128_zero();
}

struct f128 __floatditf(long x)
{
    (void)x;
    return f128_zero();
}

struct f128 __floatunditf(unsigned long x)
{
    (void)x;
    return f128_zero();
}

int __fixtfsi(struct f128 a)
{
    (void)a;
    return 0;
}

unsigned __fixunstfsi(struct f128 a)
{
    (void)a;
    return 0;
}

long __fixtfdi(struct f128 a)
{
    (void)a;
    return 0;
}

unsigned long __fixunstfdi(struct f128 a)
{
    (void)a;
    return 0;
}

struct f128 __extendsftf2(int x)
{
    (void)x;
    return f128_zero();
}

struct f128 __extenddftf2(long x)
{
    (void)x;
    return f128_zero();
}

int __trunctfsf2(struct f128 a)
{
    (void)a;
    return 0;
}

long __trunctfdf2(struct f128 a)
{
    (void)a;
    return 0;
}

/* tccgen.c gen_cvt_itof1/ftoi1 (non-ARM64): ull <-> float/double/ld. */
int __floatundisf(unsigned long x)
{
    (void)x;
    return 0;
}

long __floatundidf(unsigned long x)
{
    return (long)(double)x;
}

double __adddf3(double a, double b)
{
    return a + b;
}

double __subdf3(double a, double b)
{
    return a - b;
}

double __muldf3(double a, double b)
{
    return a * b;
}

double __divdf3(double a, double b)
{
    return a / b;
}

double __floatsidf(int x)
{
    return (double)x;
}

double __floatdidf(long x)
{
    return (double)x;
}

int __fixdfsi(double x)
{
    return (int)x;
}

long __fixdfdi(double x)
{
    return (long)x;
}

int __eqdf2(double a, double b)
{
    return a != b;
}

int __nedf2(double a, double b)
{
    return a != b;
}

int __ltdf2(double a, double b)
{
    return a < b ? -1 : (a == b ? 0 : 1);
}

int __ledf2(double a, double b)
{
    return a < b ? -1 : (a == b ? 0 : 1);
}

int __gtdf2(double a, double b)
{
    return a > b ? 1 : (a == b ? 0 : -1);
}

int __gedf2(double a, double b)
{
    return a > b ? 1 : (a == b ? 0 : -1);
}

struct f128 __floatundixf(unsigned long x)
{
    (void)x;
    return f128_zero();
}

unsigned long __fixunssfdi(int a)
{
    (void)a;
    return 0;
}

unsigned long __fixunsdfdi(long a)
{
    (void)a;
    return 0;
}

unsigned long __fixunsxfdi(struct f128 a)
{
    (void)a;
    return 0;
}

void *dlopen(const char *filename, int flag)
{
    (void)filename;
    (void)flag;
    return 0;
}

void dlclose(void *p)
{
    (void)p;
}

void *dlsym(void *handle, const char *symbol)
{
    (void)handle;
    (void)symbol;
    return 0;
}

const char *dlerror(void)
{
    return "dl";
}

void (*signal(int sig, void (*handler)(int)))(int)
{
    (void)sig;
    (void)handler;
    return 0;
}

int raise(int sig)
{
    (void)sig;
    return 0;
}

int kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    return 0;
}

struct DIR {
    int unused;
};

void *opendir(const char *path)
{
    (void)path;
    return 0;
}

void *readdir(void *d)
{
    (void)d;
    return 0;
}

int closedir(void *d)
{
    (void)d;
    return -1;
}

/* HEAD tcc.c skips tcctools.c when CONFIG_TCCBOOT but still calls these. */
void tcc_tool_cross(void *s, char **argv, int option)
{
    (void)s;
    (void)argv;
    (void)option;
}

void gen_makedeps(void *s, const char *target, const char *filename)
{
    (void)s;
    (void)target;
    (void)filename;
}
