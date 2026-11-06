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
void abort_print_ra(unsigned long ra);

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

void abort_print_ra(unsigned long ra)
{
    char b[27];
    int i;

    b[0] = 'a';
    b[1] = 'b';
    b[2] = 'o';
    b[3] = 'r';
    b[4] = 't';
    b[5] = '\n';
    b[6] = 'r';
    b[7] = 'a';
    b[8] = '=';
    for (i = 0; i < 16; i++) {
        unsigned int d;

        d = (unsigned int)((ra >> ((15 - i) * 4)) & 15UL);
        b[9 + i] = (char)(d < 10 ? '0' + d : 'a' + (d - 10));
    }
    b[25] = '\n';
    write(2, b, 26);
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
    if (p >= 0x80000000UL)
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
    /* A wrapped size_t (cpp string length sign-extended) used to spin here. */
    if (n > 0x10000000UL)
        return 0;
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
    int prec;
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
        prec = -1;
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
            /* precision: %.Ns / %.*s print at most N chars (tcc mob -DX=v) */
            p = p + 1;
            prec = 0;
            if (*p == '*') {
                prec = va_arg(ap, int);
                if (prec < 0)
                    prec = -1;
                p = p + 1;
            } else {
                while (*p >= '0' && *p <= '9') {
                    prec = prec * 10 + (*p - '0');
                    p = p + 1;
                }
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
            sl = 0;
            while (str[sl] && (prec < 0 || sl < (unsigned long)prec))
                sl = sl + 1;
            pad = width - (int)sl;
            if (!left) {
                while (pad > 0) {
                    if (pos + 1 < n)
                        s[pos] = ' ';
                    pos = pos + 1;
                    pad = pad - 1;
                }
            }
            while (sl > 0) {
                if (pos + 1 < n)
                    s[pos] = *str;
                pos = pos + 1;
                str = str + 1;
                sl = sl - 1;
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
    if (r > 0) {
        unsigned long n;
        n = (unsigned long)r;
        if (n > 1023)
            n = 1023;
        write(fd, buf, n);
    }
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
FILE *freopen(const char *path, const char *mode, FILE *f)
{
    if (f)
        fclose(f);
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

char *asctime(const void *tm)
{
    (void)tm;
    return "Thu Jan  1 00:00:00 1970\n";
}

char *strtok(char *s, const char *sep)
{
    static char *save;
    char *start;
    const char *p;
    int hit;

    if (!s)
        s = save;
    if (!s)
        return 0;
    while (*s) {
        hit = 0;
        p = sep;
        while (p && *p) {
            if (*s == *p)
                hit = 1;
            p = p + 1;
        }
        if (!hit)
            break;
        s = s + 1;
    }
    if (!*s) {
        save = 0;
        return 0;
    }
    start = s;
    while (*s) {
        hit = 0;
        p = sep;
        while (p && *p) {
            if (*s == *p)
                hit = 1;
            p = p + 1;
        }
        if (hit) {
            *s = 0;
            save = s + 1;
            return start;
        }
        s = s + 1;
    }
    save = 0;
    return start;
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
int lstat(const char *path, void *st)
{
    return stat(path, st);
}
long clock(void)
{
    return 0;
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

/* IEEE binary128 long double helpers, ported from tcc mob lib/lib-arm64.c
 * (tinycc e23dabed), with this notice:
 *
 *  TCC runtime library for arm64.
 *
 *  Copyright (c) 2015 Edmund Grimley Evans
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.  This file is offered as-is,
 * without any warranty.
 *
 * Kronos port: long double is passed as struct f128 (same a0/a1 pair on
 * riscv64), so tcc builds without long double still compile this.  Only
 * 64-bit unsigned arithmetic: no 32->64 zero-extension and no hex literal
 * >= 0x80000000, which pnut-era tcc gets wrong.  No struct copy through a
 * pointer either (return *p, *p = s): 0.9.26 riscv64 loads the second
 * register of such a pair from a stale temp. */
struct f128 {
    unsigned long lo;
    unsigned long hi;
};

static struct f128 f3_make(unsigned long lo, unsigned long hi)
{
    struct f128 x;
    x.lo = lo;
    x.hi = hi;
    return x;
}

static unsigned long f3_sbit(int sgn)
{
    unsigned long s;
    s = sgn;
    return s << 63;
}

static struct f128 f3_zero(int sgn)
{
    return f3_make(0, f3_sbit(sgn));
}

static struct f128 f3_infinity(int sgn)
{
    unsigned long e;
    e = 32767;
    return f3_make(0, f3_sbit(sgn) | e << 48);
}

static struct f128 f3_NaN(void)
{
    unsigned long m;
    m = 0;
    m = ~m;
    return f3_make(m, m >> 1);
}

/* Returns 1 and sets *f if a or b is a NaN (quiet result). */
static int f3_nans(struct f128 *f, int a_sgn, int a_exp, struct f128 *a,
                   int b_sgn, int b_exp, struct f128 *b)
{
    unsigned long q;
    q = 0x7fff8;
    q = q << 44;
    if (a_exp == 32767 && (a->lo | a->hi << 16)) {
        f->lo = a->lo;
        f->hi = a->hi | q | f3_sbit(a_sgn);
        return 1;
    }
    if (b_exp == 32767 && (b->lo | b->hi << 16)) {
        f->lo = b->lo;
        f->hi = b->hi | q | f3_sbit(b_sgn);
        return 1;
    }
    return 0;
}

static void f3_unpack(int *sgn, int *exp, struct f128 *mnt, struct f128 f)
{
    unsigned long one;
    one = 1;
    *sgn = (int)(f.hi >> 63);
    *exp = (int)(f.hi >> 48 & 32767);
    mnt->lo = f.lo;
    mnt->hi = f.hi << 16 >> 16;
    if (*exp)
        mnt->hi = mnt->hi | one << 48;
    else
        *exp = 1;
}

static void f3_normalise(int *exp, struct f128 *mnt)
{
    int sh;
    if (!(mnt->lo | mnt->hi))
        return;
    if (!mnt->hi) {
        mnt->hi = mnt->lo;
        mnt->lo = 0;
        *exp = *exp - 64;
    }
    for (sh = 32; sh; sh = sh >> 1) {
        if (!(mnt->hi >> (64 - sh))) {
            mnt->hi = mnt->hi << sh | mnt->lo >> (64 - sh);
            mnt->lo = mnt->lo << sh;
            *exp = *exp - sh;
        }
    }
}

static void f3_sticky_shift(int sh, struct f128 *x)
{
    unsigned long s;
    if (sh >= 128) {
        s = !!(x->lo | x->hi);
        x->lo = s;
        x->hi = 0;
        return;
    }
    if (sh >= 64) {
        s = !!x->lo;
        x->lo = x->hi | s;
        x->hi = 0;
        sh = sh - 64;
    }
    if (sh > 0) {
        s = !!(x->lo << (64 - sh));
        x->lo = x->lo >> sh | x->hi << (64 - sh) | s;
        x->hi = x->hi >> sh;
    }
}

static struct f128 f3_round(int sgn, int exp, struct f128 *x)
{
    int error;
    unsigned long one;
    unsigned long e;
    one = 1;

    if (exp > 0) {
        f3_sticky_shift(13, x);
    } else {
        f3_sticky_shift(14 - exp, x);
        exp = 0;
    }

    error = (int)(x->lo & 3);
    x->lo = x->lo >> 2 | x->hi << 62;
    x->hi = x->hi >> 2;

    if (error == 3 || ((error == 2) & (int)(x->lo & 1))) {
        x->lo = x->lo + 1;
        if (!x->lo) {
            x->hi = x->hi + 1;
            if (x->hi == one << 48)
                exp = 1;
            else if (x->hi == one << 49) {
                exp = exp + 1;
                x->lo = x->lo >> 1 | x->hi << 63;
                x->hi = x->hi >> 1;
            }
        }
    }

    if (exp >= 32767)
        return f3_infinity(sgn);

    e = exp;
    x->hi = x->hi << 16 >> 16 | e << 48 | f3_sbit(sgn);
    return f3_make(x->lo, x->hi);
}

static struct f128 f3_add(struct f128 fa, struct f128 fb, int neg)
{
    struct f128 a;
    struct f128 b;
    struct f128 x;
    struct f128 fx;
    int a_exp;
    int b_exp;
    int x_exp;
    int a_sgn;
    int b_sgn;
    int x_sgn;

    f3_unpack(&a_sgn, &a_exp, &a, fa);
    f3_unpack(&b_sgn, &b_exp, &b, fb);

    if (f3_nans(&fx, a_sgn, a_exp, &a, b_sgn, b_exp, &b))
        return fx;

    b_sgn = b_sgn ^ neg;

    if (a_exp == 32767 && b_exp == 32767 && a_sgn != b_sgn)
        return f3_NaN();
    if (a_exp == 32767)
        return f3_infinity(a_sgn);
    if (b_exp == 32767)
        return f3_infinity(b_sgn);
    if (!(a.lo | a.hi | b.lo | b.hi))
        return f3_zero(a_sgn & b_sgn);

    a.hi = a.hi << 3 | a.lo >> 61;
    a.lo = a.lo << 3;
    b.hi = b.hi << 3 | b.lo >> 61;
    b.lo = b.lo << 3;

    if (a_exp <= b_exp) {
        f3_sticky_shift(b_exp - a_exp, &a);
        a_exp = b_exp;
    } else {
        f3_sticky_shift(a_exp - b_exp, &b);
        b_exp = a_exp;
    }

    x_sgn = a_sgn;
    x_exp = a_exp;
    if (a_sgn == b_sgn) {
        x.lo = a.lo + b.lo;
        x.hi = a.hi + b.hi + (x.lo < a.lo);
    } else {
        x.lo = a.lo - b.lo;
        x.hi = a.hi - b.hi - (x.lo > a.lo);
        if (x.hi >> 63) {
            x_sgn = x_sgn ^ 1;
            x.lo = 0 - x.lo;
            x.hi = 0 - x.hi - !!x.lo;
        }
    }

    if (!(x.lo | x.hi))
        return f3_zero(0);

    f3_normalise(&x_exp, &x);

    return f3_round(x_sgn, x_exp + 12, &x);
}

struct f128 __addtf3(struct f128 a, struct f128 b)
{
    return f3_add(a, b, 0);
}

struct f128 __subtf3(struct f128 a, struct f128 b)
{
    return f3_add(a, b, 1);
}

struct f128 __multf3(struct f128 fa, struct f128 fb)
{
    struct f128 a;
    struct f128 b;
    struct f128 x;
    struct f128 fx;
    int a_exp;
    int b_exp;
    int x_exp;
    int a_sgn;
    int b_sgn;
    int x_sgn;
    unsigned long a0;
    unsigned long a1;
    unsigned long a2;
    unsigned long a3;
    unsigned long b0;
    unsigned long b1;
    unsigned long b2;
    unsigned long b3;
    unsigned long x0;
    unsigned long x1;
    unsigned long x2;
    unsigned long x3;
    unsigned long x4;
    unsigned long x5;
    unsigned long x6;
    unsigned long y0;
    unsigned long y1;

    f3_unpack(&a_sgn, &a_exp, &a, fa);
    f3_unpack(&b_sgn, &b_exp, &b, fb);

    if (f3_nans(&fx, a_sgn, a_exp, &a, b_sgn, b_exp, &b))
        return fx;

    if ((a_exp == 32767 && !(b.lo | b.hi)) ||
        (b_exp == 32767 && !(a.lo | a.hi)))
        return f3_NaN();
    if (a_exp == 32767 || b_exp == 32767)
        return f3_infinity(a_sgn ^ b_sgn);
    if (!(a.lo | a.hi) || !(b.lo | b.hi))
        return f3_zero(a_sgn ^ b_sgn);

    f3_normalise(&a_exp, &a);
    f3_normalise(&b_exp, &b);

    x_sgn = a_sgn ^ b_sgn;
    x_exp = a_exp + b_exp - 16352;

    /* base 1 << 30 digits, bottom 6 bits are zero: (32, 30, 30, 30) bits */
    a0 = a.lo << 28 >> 34;
    b0 = b.lo << 28 >> 34;
    a1 = a.lo >> 36 | a.hi << 62 >> 34;
    b1 = b.lo >> 36 | b.hi << 62 >> 34;
    a2 = a.hi << 32 >> 34;
    b2 = b.hi << 32 >> 34;
    a3 = a.hi >> 32;
    b3 = b.hi >> 32;
    x0 = a0 * b0;
    x1 = (x0 >> 30) + a0 * b1 + a1 * b0;
    x2 = (x1 >> 30) + a0 * b2 + a1 * b1 + a2 * b0;
    x3 = (x2 >> 30) + a0 * b3 + a1 * b2 + a2 * b1 + a3 * b0;
    x4 = (x3 >> 30) + a1 * b3 + a2 * b2 + a3 * b1;
    x5 = (x4 >> 30) + a2 * b3 + a3 * b2;
    x6 = (x5 >> 30) + a3 * b3;
    y0 = !!(x3 << 38 | (x2 | x1 | x0) << 34);
    y0 = x5 << 34 | x4 << 34 >> 30 | x3 << 34 >> 60 | y0;
    y1 = x6;
    if (!(y1 >> 63)) {
        y1 = y1 << 1 | y0 >> 63;
        y0 = y0 << 1;
        x_exp = x_exp - 1;
    }
    x.lo = y0;
    x.hi = y1;

    return f3_round(x_sgn, x_exp, &x);
}

struct f128 __divtf3(struct f128 fa, struct f128 fb)
{
    struct f128 a;
    struct f128 b;
    struct f128 x;
    struct f128 fx;
    int a_exp;
    int b_exp;
    int x_exp;
    int a_sgn;
    int b_sgn;
    int x_sgn;
    int i;

    f3_unpack(&a_sgn, &a_exp, &a, fa);
    f3_unpack(&b_sgn, &b_exp, &b, fb);

    if (f3_nans(&fx, a_sgn, a_exp, &a, b_sgn, b_exp, &b))
        return fx;

    if ((a_exp == 32767 && b_exp == 32767) ||
        (!(a.lo | a.hi) && !(b.lo | b.hi)))
        return f3_NaN();
    if (a_exp == 32767 || !(b.lo | b.hi))
        return f3_infinity(a_sgn ^ b_sgn);
    if (!(a.lo | a.hi) || b_exp == 32767)
        return f3_zero(a_sgn ^ b_sgn);

    f3_normalise(&a_exp, &a);
    f3_normalise(&b_exp, &b);

    x_sgn = a_sgn ^ b_sgn;
    x_exp = a_exp - b_exp + 16395;

    a.lo = a.lo >> 1 | a.hi << 63;
    a.hi = a.hi >> 1;
    b.lo = b.lo >> 1 | b.hi << 63;
    b.hi = b.hi >> 1;
    x.lo = 0;
    x.hi = 0;
    for (i = 0; i < 116; i = i + 1) {
        x.hi = x.hi << 1 | x.lo >> 63;
        x.lo = x.lo << 1;
        if (a.hi > b.hi || (a.hi == b.hi && a.lo >= b.lo)) {
            a.hi = a.hi - b.hi - (a.lo < b.lo);
            a.lo = a.lo - b.lo;
            x.lo = x.lo | 1;
        }
        a.hi = a.hi << 1 | a.lo >> 63;
        a.lo = a.lo << 1;
    }
    x.lo = x.lo | !!(a.lo | a.hi);

    f3_normalise(&x_exp, &x);

    return f3_round(x_sgn, x_exp, &x);
}

struct f128 __negtf2(struct f128 f)
{
    f.hi = f.hi ^ f3_sbit(1);
    return f;
}

struct f128 __extendsftf2(float f)
{
    unsigned long aa;
    unsigned long m;
    unsigned long adj;
    unsigned long e;
    struct f128 x;

    aa = 0;
    memcpy(&aa, &f, 4);
    m = aa << 41 >> 41;
    e = aa >> 23 & 255;
    x.lo = 0;
    if (!(aa << 33))
        x.hi = aa << 32;
    else if (e == 255) {
        e = 32767;
        x.hi = e << 48 | aa >> 31 << 63 | aa << 41 >> 16 | (unsigned long)!!m << 47;
    } else if (e == 0) {
        adj = 0;
        while (!(m >> (23 - adj)))
            adj = adj + 1;
        x.hi = aa >> 31 << 63 | (16256 - adj + 1) << 48 | aa << adj << 41 >> 16;
    } else
        x.hi = aa >> 31 << 63 | (e + 16256) << 48 | aa << 41 >> 16;
    return x;
}

struct f128 __extenddftf2(double f)
{
    unsigned long a;
    unsigned long adj;
    unsigned long e;
    struct f128 x;

    a = 0;
    memcpy(&a, &f, 8);
    x.lo = a << 60;
    if (!(a << 1))
        x.hi = a;
    else if (a << 1 >> 53 == 2047) {
        e = 32767;
        x.hi = e << 48 | a >> 63 << 63 | a << 12 >> 16 | (unsigned long)!!(a << 12) << 47;
    } else if (a << 1 >> 53 == 0) {
        adj = 0;
        while (!(a << 1 >> 1 >> (52 - adj)))
            adj = adj + 1;
        x.lo = x.lo << adj;
        x.hi = a >> 63 << 63 | (15360 - adj + 1) << 48 | a << adj << 12 >> 16;
    } else
        x.hi = a >> 63 << 63 | ((a >> 52 & 2047) + 15360) << 48 | a << 12 >> 16;
    return x;
}

float __trunctfsf2(struct f128 f)
{
    struct f128 mnt;
    int exp;
    int sgn;
    unsigned long x;
    unsigned long s;
    unsigned long m32;
    float r;

    m32 = 1;
    m32 = (m32 << 32) - 1;
    f3_unpack(&sgn, &exp, &mnt, f);
    s = sgn;
    s = s << 31;
    if (exp == 32767 && (mnt.lo | mnt.hi << 16))
        x = 0x7fc00000 | s | (mnt.hi >> 25 & 0x007fffff);
    else if (exp > 16510)
        x = 0x7f800000 | s;
    else if (exp < 16233)
        x = s;
    else {
        exp = exp - 16257;
        x = mnt.hi >> 23 | !!(mnt.lo | mnt.hi << 41);
        if (exp < 0) {
            x = x >> (0 - exp) | !!((x << (32 + exp)) & m32);
            exp = 0;
        }
        if ((x & 3) == 3 || (x & 7) == 6)
            x = x + 4;
        x = (((x >> 2) + ((unsigned long)exp << 23)) & m32) | s;
    }
    memcpy(&r, &x, 4);
    return r;
}

double __trunctfdf2(struct f128 f)
{
    struct f128 mnt;
    int exp;
    int sgn;
    unsigned long x;
    unsigned long e;
    double r;

    f3_unpack(&sgn, &exp, &mnt, f);
    if (exp == 32767 && (mnt.lo | mnt.hi << 16)) {
        e = 0x7ff8;
        x = e << 48 | f3_sbit(sgn) | mnt.hi << 16 >> 12 | mnt.lo >> 60;
    } else if (exp > 17406) {
        e = 0x7ff;
        x = e << 52 | f3_sbit(sgn);
    } else if (exp < 15308)
        x = f3_sbit(sgn);
    else {
        exp = exp - 15361;
        x = mnt.hi << 6 | mnt.lo >> 58 | !!(mnt.lo << 6);
        if (exp < 0) {
            x = x >> (0 - exp) | !!(x << (64 + exp));
            exp = 0;
        }
        if ((x & 3) == 3 || (x & 7) == 6)
            x = x + 4;
        e = exp;
        x = ((x >> 2) + (e << 52)) | f3_sbit(sgn);
    }
    memcpy(&r, &x, 8);
    return r;
}

long __fixtfdi(struct f128 fa)
{
    struct f128 a;
    int a_exp;
    int a_sgn;
    unsigned long x;
    unsigned long big;
    f3_unpack(&a_sgn, &a_exp, &a, fa);
    if (a_exp < 16383)
        return 0;
    if (a_exp > 16445) {
        big = f3_sbit(1);
        if (!a_sgn)
            big = big - 1;
        return (long)big;
    }
    x = (a.hi << 15 | a.lo >> 49) >> (16446 - a_exp);
    if (a_sgn)
        x = 0 - x;
    return (long)x;
}

int __fixtfsi(struct f128 fa)
{
    struct f128 a;
    int a_exp;
    int a_sgn;
    int x;
    f3_unpack(&a_sgn, &a_exp, &a, fa);
    if (a_exp < 16369)
        return 0;
    if (a_exp > 16413)
        return a_sgn ? -2147483647 - 1 : 2147483647;
    x = (int)(a.hi >> (16431 - a_exp));
    return a_sgn ? -x : x;
}

unsigned long __fixunstfdi(struct f128 fa)
{
    struct f128 a;
    int a_exp;
    int a_sgn;
    unsigned long x;
    f3_unpack(&a_sgn, &a_exp, &a, fa);
    if (a_sgn || a_exp < 16383)
        return 0;
    x = 0;
    if (a_exp > 16446)
        return ~x;
    return (a.hi << 15 | a.lo >> 49) >> (16446 - a_exp);
}

unsigned __fixunstfsi(struct f128 fa)
{
    struct f128 a;
    int a_exp;
    int a_sgn;
    unsigned x;
    f3_unpack(&a_sgn, &a_exp, &a, fa);
    if (a_sgn || a_exp < 16369)
        return 0;
    if (a_exp > 16414)
        return -1;
    x = (unsigned)(a.hi >> (16431 - a_exp));
    return x;
}

struct f128 __floatunditf(unsigned long a)
{
    int exp;
    int i;
    unsigned long mnt;
    unsigned long e;
    struct f128 x;
    exp = 16446;
    mnt = a;
    x.lo = 0;
    x.hi = 0;
    if (a) {
        for (i = 32; i; i = i >> 1)
            if (!(mnt >> (64 - i))) {
                mnt = mnt << i;
                exp = exp - i;
            }
        e = exp;
        x.lo = mnt << 49;
        x.hi = e << 48 | mnt << 1 >> 16;
    }
    return x;
}

struct f128 __floatditf(long a)
{
    struct f128 x;
    unsigned long m;
    if (a >= 0)
        return __floatunditf((unsigned long)a);
    m = (unsigned long)a;
    x = __floatunditf(0 - m);
    x.hi = x.hi | f3_sbit(1);
    return x;
}

struct f128 __floatsitf(int a)
{
    return __floatditf((long)a);
}

struct f128 __floatunsitf(unsigned a)
{
    unsigned long v;
    v = a;
    v = v << 32 >> 32;
    return __floatunditf(v);
}

/* <0 a<b, 0 equal, >0 a>b, 2 unordered (NaN) */
static int f3_cmp(struct f128 a, struct f128 b)
{
    int as;
    if (!(a.lo | a.hi << 1 | b.lo | b.hi << 1))
        return 0;
    if ((a.hi << 1 >> 49 == 32767 && (a.lo | a.hi << 16)) ||
        (b.hi << 1 >> 49 == 32767 && (b.lo | b.hi << 16)))
        return 2;
    as = (int)(a.hi >> 63);
    if (as != (int)(b.hi >> 63))
        return (int)(b.hi >> 63) - as;
    if (a.hi < b.hi)
        return (as << 1) - 1;
    if (a.hi > b.hi)
        return 1 - (as << 1);
    if (a.lo < b.lo)
        return (as << 1) - 1;
    if (b.lo < a.lo)
        return 1 - (as << 1);
    return 0;
}

int __eqtf2(struct f128 a, struct f128 b)
{
    return !!f3_cmp(a, b);
}

int __netf2(struct f128 a, struct f128 b)
{
    return !!f3_cmp(a, b);
}

int __lttf2(struct f128 a, struct f128 b)
{
    return f3_cmp(a, b);
}

int __letf2(struct f128 a, struct f128 b)
{
    return f3_cmp(a, b);
}

int __gttf2(struct f128 a, struct f128 b)
{
    return -f3_cmp(b, a);
}

int __getf2(struct f128 a, struct f128 b)
{
    return -f3_cmp(b, a);
}

/* strtod/strtof/strtold.  The decimal mantissa is kept exact in 128 bits;
 * digits past that only set a sticky bit.  It is rounded to binary128 and
 * scaled by 10^e with __multf3/__divtf3 (10^k is exact for k <= 48, so
 * that is one rounding); double and float are rounded from that.  Integer
 * arithmetic only, like the helpers above. */
static int f3_word(const char *p, const char *w)
{
    int c;
    while (*w) {
        c = *p;
        if (c >= 'A' && c <= 'Z')
            c = c + 32;
        if (c != *w)
            return 0;
        p = p + 1;
        w = w + 1;
    }
    return 1;
}

/* hi:lo = hi:lo * 10 + d */
static void f3_mul10(unsigned long *hi, unsigned long *lo, unsigned long d)
{
    unsigned long l8;
    unsigned long l;
    unsigned long h;
    unsigned long c;
    l8 = *lo << 3;
    l = l8 + (*lo << 1);
    h = (*hi << 3 | *lo >> 61) + (*hi << 1 | *lo >> 63) + (l < l8);
    c = l + d;
    h = h + (c < l);
    *hi = h;
    *lo = c;
}

static struct f128 f3_pow10(int k)
{
    struct f128 p;
    struct f128 t;
    unsigned long v;
    int i;
    v = 1;
    for (i = 0; i < 19; i++)
        v = v * 10;
    t = __floatunditf(v);
    p = __floatunditf(1);
    while (k >= 19) {
        p = __multf3(p, t);
        k = k - 19;
    }
    v = 1;
    while (k > 0) {
        v = v * 10;
        k = k - 1;
    }
    t = __floatunditf(v);
    return __multf3(p, t);
}

static struct f128 f3_strto(const char *s, char **end)
{
    const char *p;
    const char *q;
    unsigned long hi;
    unsigned long lo;
    unsigned long room;
    unsigned long sticky;
    unsigned long d;
    struct f128 m;
    struct f128 r;
    int neg;
    int dot;
    int any;
    int zeros;
    int e10;
    int ex;
    int eneg;
    int exp;

    p = s;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' ||
           *p == '\f' || *p == '\v')
        p = p + 1;
    neg = 0;
    if (*p == '+')
        p = p + 1;
    else if (*p == '-') {
        neg = 1;
        p = p + 1;
    }
    if (f3_word(p, "inf")) {
        p = p + 3;
        if (f3_word(p, "inity"))
            p = p + 5;
        if (end)
            *end = (char *)p;
        return f3_infinity(neg);
    }
    if (f3_word(p, "nan")) {
        if (end)
            *end = (char *)(p + 3);
        return f3_NaN();
    }

    room = 0;
    room = ~room / 10;
    hi = 0;
    lo = 0;
    sticky = 0;
    dot = 0;
    any = 0;
    zeros = 0;
    e10 = 0;
    for (;;) {
        if (*p >= '0' && *p <= '9') {
            any = 1;
            d = *p - '0';
            p = p + 1;
            if (dot && !d) {
                /* trailing fraction zeros change nothing */
                zeros = zeros + 1;
                continue;
            }
            for (; zeros; zeros = zeros - 1) {
                if (hi < room) {
                    f3_mul10(&hi, &lo, 0);
                    e10 = e10 - 1;
                }
            }
            if (hi < room) {
                f3_mul10(&hi, &lo, d);
                if (dot)
                    e10 = e10 - 1;
            } else {
                if (d)
                    sticky = 1;
                if (!dot)
                    e10 = e10 + 1;
            }
        } else if (*p == '.' && !dot) {
            dot = 1;
            p = p + 1;
        } else
            break;
    }
    if (!any) {
        if (end)
            *end = (char *)s;
        return f3_zero(0);
    }
    if (*p == 'e' || *p == 'E') {
        q = p + 1;
        eneg = 0;
        if (*q == '+')
            q = q + 1;
        else if (*q == '-') {
            eneg = 1;
            q = q + 1;
        }
        if (*q >= '0' && *q <= '9') {
            ex = 0;
            while (*q >= '0' && *q <= '9') {
                if (ex < 100000)
                    ex = ex * 10 + (*q - '0');
                q = q + 1;
            }
            if (eneg)
                ex = 0 - ex;
            e10 = e10 + ex;
            p = q;
        }
    }
    if (end)
        *end = (char *)p;
    if (!(hi | lo))
        return f3_zero(neg);

    exp = 16510;
    m.hi = hi;
    m.lo = lo;
    f3_normalise(&exp, &m);
    m.lo = m.lo | sticky;
    r = f3_round(0, exp, &m);
    if (e10 > 6000)
        e10 = 6000;
    if (e10 < -6000)
        e10 = -6000;
    if (e10 > 0) {
        m = f3_pow10(e10);
        r = __multf3(r, m);
    } else if (e10 < 0) {
        m = f3_pow10(0 - e10);
        r = __divtf3(r, m);
    }
    r.hi = r.hi | f3_sbit(neg);
    return r;
}

struct f128 strtold(const char *s, char **end)
{
    return f3_strto(s, end);
}

double strtod(const char *s, char **end)
{
    struct f128 f;
    f = f3_strto(s, end);
    return __trunctfdf2(f);
}

float strtof(const char *s, char **end)
{
    struct f128 f;
    f = f3_strto(s, end);
    return __trunctfsf2(f);
}

double atof(const char *s)
{
    return strtod(s, 0);
}

/* tccgen.c gen_cvt_itof1/ftoi1 (non-ARM64): ull <-> float/double/ld.
 * tcc6 calls these for unsigned long <-> float/double, so they must not
 * use such a cast themselves.  f3_ubits: x rounded to nearest even with
 * mb mantissa bits and exponent bias. */
static unsigned long f3_ubits(unsigned long x, int mb, int bias)
{
    unsigned long one;
    unsigned long m;
    unsigned long rem;
    unsigned long e;
    int p;
    int sh;
    if (!x)
        return 0;
    one = 1;
    p = 63;
    while (!(x >> p))
        p = p - 1;
    if (p <= mb)
        m = x << (mb - p);
    else {
        sh = p - mb;
        m = x >> sh;
        rem = x << (64 - sh);
        if (rem > one << 63 || (rem == one << 63 && (m & 1))) {
            m = m + 1;
            if (m >> (mb + 1)) {
                m = m >> 1;
                p = p + 1;
            }
        }
    }
    e = p + bias;
    return e << mb | (m ^ one << mb);
}

float __floatundisf(unsigned long x)
{
    unsigned long b;
    float r;
    b = f3_ubits(x, 23, 127);
    memcpy(&r, &b, 4);
    return r;
}

double __floatundidf(unsigned long x)
{
    unsigned long b;
    double r;
    b = f3_ubits(x, 52, 1023);
    memcpy(&r, &b, 8);
    return r;
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

/* 0.9.26 tccgen names ull <-> long double after x87 XF; it is binary128 here. */
struct f128 __floatundixf(unsigned long x)
{
    return __floatunditf(x);
}

unsigned long __fixunssfdi(float a)
{
    unsigned long b;
    unsigned long one;
    int e;
    one = 1;
    b = 0;
    memcpy(&b, &a, 4);
    if (b >> 31)
        return 0;
    e = (int)(b >> 23 & 255) - 127;
    if (e < 0)
        return 0;
    if (e > 63)
        return ~(b ^ b);
    b = (b << 41 >> 41) | one << 23;
    if (e >= 23)
        return b << (e - 23);
    return b >> (23 - e);
}

unsigned long __fixunsdfdi(double a)
{
    unsigned long b;
    unsigned long one;
    int e;
    one = 1;
    b = 0;
    memcpy(&b, &a, 8);
    if (b >> 63)
        return 0;
    e = (int)(b >> 52 & 2047) - 1023;
    if (e < 0)
        return 0;
    if (e > 63)
        return ~(b ^ b);
    b = (b << 12 >> 12) | one << 52;
    if (e >= 52)
        return b << (e - 52);
    return b >> (52 - e);
}

unsigned long __fixunsxfdi(struct f128 a)
{
    return __fixunstfdi(a);
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

static unsigned char gas_obuf[16];
static int gas_olen;

void gas_emit(int w)
{
    unsigned char *p;
    if (gas_olen > 12)
        return;
    p = gas_obuf + gas_olen;
    p[0] = (unsigned char)(w & 255);
    p[1] = (unsigned char)((w >> 8) & 255);
    p[2] = (unsigned char)((w >> 16) & 255);
    p[3] = (unsigned char)((w >> 24) & 255);
    gas_olen = gas_olen + 4;
}

void gas_flush(void)
{
    int fd;
    fd = open("/src/as/smoke.bin", O_WRONLY | O_CREAT | O_TRUNC, 420);
    if (fd < 0)
        return;
    write(fd, gas_obuf, (unsigned long)gas_olen);
    close(fd);
}

static unsigned char elf_words[4096];
static int elf_nins;

static void wr16(unsigned char *p, int v)
{
    p[0] = (unsigned char)(v & 255);
    p[1] = (unsigned char)((v >> 8) & 255);
}

static void wr32(unsigned char *p, int v)
{
    p[0] = (unsigned char)(v & 255);
    p[1] = (unsigned char)((v >> 8) & 255);
    p[2] = (unsigned char)((v >> 16) & 255);
    p[3] = (unsigned char)((v >> 24) & 255);
}

static void wr64(unsigned char *p, int v)
{
    wr32(p, v);
    wr32(p + 4, 0);
}

void elf_emit(int w)
{
    unsigned char *p;
    if (elf_nins > 4092)
        return;
    p = elf_words + elf_nins;
    p[0] = (unsigned char)(w & 255);
    p[1] = (unsigned char)((w >> 8) & 255);
    p[2] = (unsigned char)((w >> 16) & 255);
    p[3] = (unsigned char)((w >> 24) & 255);
    elf_nins = elf_nins + 4;
}

void elf_flush(void)
{
    unsigned char hdr[120];
    unsigned char z[256];
    unsigned char *p;
    int i;
    int left;
    int fd;

    i = 0;
    while (i < 120) {
        hdr[i] = 0;
        i = i + 1;
    }
    i = 0;
    while (i < 256) {
        z[i] = 0;
        i = i + 1;
    }
    hdr[0] = 0x7f;
    hdr[1] = 'E';
    hdr[2] = 'L';
    hdr[3] = 'F';
    hdr[4] = 2;
    hdr[5] = 1;
    hdr[6] = 1;
    wr16(hdr + 16, 2);
    wr16(hdr + 18, 243);
    wr32(hdr + 20, 1);
    wr64(hdr + 24, 0x10000);
    wr64(hdr + 32, 0x40);
    wr16(hdr + 52, 64);
    wr16(hdr + 54, 56);
    wr16(hdr + 56, 1);
    p = hdr + 64;
    wr32(p, 1);
    wr32(p + 4, 5);
    wr64(p + 8, 0x1000);
    wr64(p + 16, 0x10000);
    wr64(p + 24, 0x10000);
    wr64(p + 32, elf_nins);
    wr64(p + 40, elf_nins);
    wr64(p + 48, 0x1000);
    fd = open("/src/ld/a.out", O_WRONLY | O_CREAT | O_TRUNC, 420);
    if (fd < 0)
        return;
    write(fd, hdr, 120);
    left = 0x1000 - 120;
    while (left >= 256) {
        write(fd, z, 256);
        left = left - 256;
    }
    if (left)
        write(fd, z, (unsigned long)left);
    write(fd, elf_words, (unsigned long)elf_nins);
    close(fd);
}

void elf_reset(void)
{
    elf_nins = 0;
}
