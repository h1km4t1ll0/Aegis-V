/* Process tests under Tilck (plan 2, phase 2): init execs this as pid 1.
   Kronos libc.c gives write/read/open/close/exit; the other Linux riscv64
   syscalls go through proc_sys (ecall). Each test prints "<name> ok" or
   "proc fail: ..."; the musl programs (stage 13, and mem) print their own
   marks. The outbox test copies /initrd/bin/child to /dev/outbox and
   prints its SHA-256, "outbox sha256 <hex> <size>" (tilck.py compares).
   Then "tests done", and pid 1 must not exit (Tilck panics when init
   exits), so it spins. */

long write(int fd, const void *buf, unsigned long n);
long read(int fd, void *buf, unsigned long n);
int open(const char *path, int flags, int mode);
int close(int fd);
void exit(int n);

/* a0 = number, a1..a6 = arguments (as compat-crt.s ecall6) */
long proc_sys(long n, long a, long b, long c, long d, long e, long f);
__asm__(
    ".global proc_sys\n"
    "proc_sys:\n"
    "\tmv a7, a0\n"
    "\tmv a0, a1\n"
    "\tmv a1, a2\n"
    "\tmv a2, a3\n"
    "\tmv a3, a4\n"
    "\tmv a4, a5\n"
    "\tmv a5, a6\n"
    "\tecall\n"
    "\tjalr zero, 0(ra)\n");

#define SYS_dup3 24
#define SYS_mkdirat 34
#define SYS_unlinkat 35
#define SYS_chdir 49
#define SYS_pipe2 59
#define SYS_getdents64 61
#define SYS_writev 66
#define SYS_clone 220
#define SYS_execve 221
#define SYS_wait4 260
#define SIGCHLD 17
#define AT_FDCWD (-100)
#define AT_REMOVEDIR 0x200
#define O_RDONLY 0
#define O_WRONLY 1
#define O_CREAT 64
#define O_TRUNC 512

static unsigned long slen(const char *s)
{
    unsigned long n;

    n = 0;
    while (s[n])
        n = n + 1;
    return n;
}

static void say(const char *s)
{
    write(1, s, slen(s));
}

static void say_num(long v)
{
    char b[24];
    int i;
    unsigned long u;

    i = 23;
    b[i] = 0;
    u = v < 0 ? (unsigned long)(0 - v) : (unsigned long)v;
    do {
        i = i - 1;
        b[i] = (char)('0' + u % 10);
        u = u / 10;
    } while (u);
    if (v < 0) {
        i = i - 1;
        b[i] = '-';
    }
    say(b + i);
}

static int fails;

static void fail(const char *what, long v)
{
    say("proc fail: ");
    say(what);
    say(" ");
    say_num(v);
    say("\n");
    fails = fails + 1;
}

static long fork1(void)
{
    return proc_sys(SYS_clone, SIGCHLD, 0, 0, 0, 0, 0);
}

/* wait for pid; returns the exit code, or -1000 - raw status if it did
   not exit normally, or the negative errno */
static long wait_exit(long pid)
{
    int st;
    long r;

    st = -1;
    r = proc_sys(SYS_wait4, pid, (long)&st, 0, 0, 0, 0);
    if (r != pid)
        return r < 0 ? r : -999;
    if ((st & 0x7f) != 0)
        return -1000 - st;
    return (st >> 8) & 0xff;
}

static void test_fork(void)
{
    long pid;
    long code;

    pid = fork1();
    if (pid == 0)
        exit(7);
    if (pid < 0) {
        fail("clone", pid);
        return;
    }
    code = wait_exit(pid);
    if (code != 7) {
        fail("fork wait4", code);
        return;
    }
    say("fork ok\n");
}

static void test_pipe(void)
{
    int fds[2];
    char buf[32];
    const char *msg;
    long pid;
    long r;
    long got;
    long code;

    msg = "through the pipe";
    fds[0] = -1;
    fds[1] = -1;
    r = proc_sys(SYS_pipe2, (long)fds, 0, 0, 0, 0, 0);
    if (r != 0 || fds[0] < 0 || fds[1] < 0) {
        fail("pipe2", r);
        return;
    }
    pid = fork1();
    if (pid == 0) {
        /* the child writes through stdout = the write end (dup3) */
        close(fds[0]);
        if (proc_sys(SYS_dup3, fds[1], 1, 0, 0, 0, 0) != 1)
            exit(3);
        close(fds[1]);
        if (write(1, msg, slen(msg)) != (long)slen(msg))
            exit(4);
        exit(0);
    }
    if (pid < 0) {
        fail("pipe clone", pid);
        return;
    }
    close(fds[1]);
    got = 0;
    while (got < (long)sizeof(buf) - 1) {
        r = read(fds[0], buf + got, sizeof(buf) - 1 - got);
        if (r <= 0)
            break;
        got = got + r;
    }
    buf[got] = 0;
    close(fds[0]);
    code = wait_exit(pid);
    if (code != 0) {
        fail("pipe child", code);
        return;
    }
    if (got != (long)slen(msg) || buf[0] != 't' || buf[got - 1] != 'e') {
        fail("pipe read", got);
        return;
    }
    say("pipe ok\n");
}

static void test_exec(void)
{
    char *av[3];
    char *ev[1];
    long pid;
    long code;

    pid = fork1();
    if (pid == 0) {
        av[0] = "/initrd/bin/child";
        av[1] = "arg1";
        av[2] = 0;
        ev[0] = 0;
        proc_sys(SYS_execve, (long)av[0], (long)av, (long)ev, 0, 0, 0);
        exit(5);
    }
    if (pid < 0) {
        fail("exec clone", pid);
        return;
    }
    code = wait_exit(pid);
    if (code != 0) {
        fail("exec child", code);
        return;
    }
    say("exec ok\n");
}

static void test_dirs(void)
{
    char buf[512];
    long r;
    long n;
    long off;
    int fd;
    int seen;
    unsigned short reclen;
    char *name;

    r = proc_sys(SYS_mkdirat, AT_FDCWD, (long)"/tmp/t", 0755, 0, 0, 0);
    if (r != 0) {
        fail("mkdirat", r);
        return;
    }
    fd = open("/tmp/t/a", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        fail("open /tmp/t/a", fd);
        return;
    }
    if (write(fd, "data\n", 5) != 5)
        fail("write /tmp/t/a", 0);
    close(fd);
    r = proc_sys(SYS_chdir, (long)"/tmp/t", 0, 0, 0, 0, 0);
    if (r != 0) {
        fail("chdir", r);
        return;
    }
    fd = open(".", O_RDONLY, 0);
    if (fd < 0) {
        fail("open .", fd);
        return;
    }
    seen = 0;
    for (;;) {
        n = proc_sys(SYS_getdents64, fd, (long)buf, sizeof(buf), 0, 0, 0);
        if (n <= 0)
            break;
        off = 0;
        while (off < n) {
            /* linux_dirent64: u64 ino, s64 off, u16 reclen, u8 type, name */
            reclen = (unsigned short)((unsigned char)buf[off + 16]
                     | ((unsigned char)buf[off + 17] << 8));
            name = buf + off + 19;
            if (name[0] == 'a' && name[1] == 0)
                seen = seen + 1;
            if (reclen == 0)
                break;
            off = off + reclen;
        }
    }
    close(fd);
    if (n < 0) {
        fail("getdents64", n);
        return;
    }
    if (seen != 1) {
        fail("getdents64 a seen", seen);
        return;
    }
    proc_sys(SYS_chdir, (long)"/", 0, 0, 0, 0, 0);
    r = proc_sys(SYS_unlinkat, AT_FDCWD, (long)"/tmp/t/a", 0, 0, 0, 0);
    if (r != 0) {
        fail("unlinkat a", r);
        return;
    }
    r = proc_sys(SYS_unlinkat, AT_FDCWD, (long)"/tmp/t", AT_REMOVEDIR, 0, 0, 0);
    if (r != 0) {
        fail("unlinkat dir", r);
        return;
    }
    fd = open("/tmp/t/a", O_RDONLY, 0);
    if (fd >= 0) {
        fail("still there", fd);
        close(fd);
        return;
    }
    say("dirs ok\n");
}

/* writev with an empty {NULL, 0} iovec in the middle, as musl's fflush()
   passes one: Tilck gave -EFAULT after writing the data */
static void test_writev(void)
{
    struct { const char *base; unsigned long len; } iov[3];
    char buf[8];
    long r;
    int fd;

    iov[0].base = "ab";
    iov[0].len = 2;
    iov[1].base = 0;
    iov[1].len = 0;
    iov[2].base = "c";
    iov[2].len = 1;
    fd = open("/tmp/w", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        fail("open /tmp/w", fd);
        return;
    }
    r = proc_sys(SYS_writev, fd, (long)iov, 3, 0, 0, 0);
    close(fd);
    if (r != 3) {
        fail("writev", r);
        return;
    }
    fd = open("/tmp/w", O_RDONLY, 0);
    r = read(fd, buf, sizeof(buf));
    close(fd);
    proc_sys(SYS_unlinkat, AT_FDCWD, (long)"/tmp/w", 0, 0, 0, 0);
    if (r != 3 || buf[0] != 'a' || buf[1] != 'b' || buf[2] != 'c') {
        fail("writev read back", r);
        return;
    }
    say("writev ok\n");
}

/* SHA-256 (FIPS 180-4), for the outbox test */
static const unsigned int sha_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

struct sha256 {
    unsigned int h[8];
    unsigned char blk[64];
    unsigned long len;
};

static unsigned int ror(unsigned int x, int n)
{
    return (x >> n) | (x << (32 - n));
}

static void sha_block(struct sha256 *c)
{
    unsigned int w[64];
    unsigned int a, b, d, e, f, g, h, k, t1, t2;
    int i;

    for (i = 0; i < 16; i++)
        w[i] = ((unsigned int)c->blk[4 * i] << 24) | ((unsigned int)c->blk[4 * i + 1] << 16)
               | ((unsigned int)c->blk[4 * i + 2] << 8) | (unsigned int)c->blk[4 * i + 3];
    for (i = 16; i < 64; i++)
        w[i] = (ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10)) + w[i - 7]
               + (ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3)) + w[i - 16];
    a = c->h[0];
    b = c->h[1];
    k = c->h[2];
    d = c->h[3];
    e = c->h[4];
    f = c->h[5];
    g = c->h[6];
    h = c->h[7];
    for (i = 0; i < 64; i++) {
        t1 = h + (ror(e, 6) ^ ror(e, 11) ^ ror(e, 25)) + ((e & f) ^ (~e & g)) + sha_k[i] + w[i];
        t2 = (ror(a, 2) ^ ror(a, 13) ^ ror(a, 22)) + ((a & b) ^ (a & k) ^ (b & k));
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = k;
        k = b;
        b = a;
        a = t1 + t2;
    }
    c->h[0] += a;
    c->h[1] += b;
    c->h[2] += k;
    c->h[3] += d;
    c->h[4] += e;
    c->h[5] += f;
    c->h[6] += g;
    c->h[7] += h;
}

static void sha_init(struct sha256 *c)
{
    c->h[0] = 0x6a09e667;
    c->h[1] = 0xbb67ae85;
    c->h[2] = 0x3c6ef372;
    c->h[3] = 0xa54ff53a;
    c->h[4] = 0x510e527f;
    c->h[5] = 0x9b05688c;
    c->h[6] = 0x1f83d9ab;
    c->h[7] = 0x5be0cd19;
    c->len = 0;
}

static void sha_update(struct sha256 *c, const unsigned char *p, unsigned long n)
{
    while (n) {
        c->blk[c->len % 64] = *p;
        c->len = c->len + 1;
        if (c->len % 64 == 0)
            sha_block(c);
        p = p + 1;
        n = n - 1;
    }
}

/* out: 64 hex digits and a NUL */
static void sha_final(struct sha256 *c, char *out)
{
    unsigned long bits;
    unsigned char pad;
    int i;

    bits = c->len * 8;
    pad = 0x80;
    sha_update(c, &pad, 1);
    pad = 0;
    while (c->len % 64 != 56)
        sha_update(c, &pad, 1);
    for (i = 7; i >= 0; i--) {
        pad = (unsigned char)(bits >> (8 * i));
        sha_update(c, &pad, 1);
    }
    for (i = 0; i < 64; i++)
        out[i] = "0123456789abcdef"[(c->h[i / 8] >> (28 - 4 * (i % 8))) & 15];
    out[64] = 0;
}

/* /initrd/bin/child -> /dev/outbox, then its SHA-256 */
static void test_outbox(void)
{
    static struct sha256 c;
    char buf[4096];
    char hex[65];
    unsigned long total;
    long n;
    long w;
    long off;
    int in;
    int out;

    in = open("/initrd/bin/child", O_RDONLY, 0);
    if (in < 0) {
        fail("outbox open child", in);
        return;
    }
    out = open("/dev/outbox", O_WRONLY, 0);
    if (out < 0) {
        fail("outbox open", out);
        close(in);
        return;
    }
    sha_init(&c);
    total = 0;
    for (;;) {
        n = read(in, buf, sizeof(buf));
        if (n <= 0)
            break;
        sha_update(&c, (unsigned char *)buf, n);
        off = 0;
        while (off < n) {
            w = write(out, buf + off, n - off);
            if (w <= 0) {
                fail("outbox write", w);
                close(out);
                close(in);
                return;
            }
            off = off + w;
        }
        total = total + n;
    }
    close(in);
    close(out);
    if (n < 0 || total == 0) {
        fail("outbox read child", n);
        return;
    }
    sha_final(&c, hex);
    say("outbox sha256 ");
    say(hex);
    say(" ");
    say_num(total);
    say("\n");
}

/* stage 13: a musl program, fork + execve + wait4, exit code 0 */
static void run_musl(char *path)
{
    char *av[2];
    char *ev[1];
    long pid;
    long code;

    pid = fork1();
    if (pid == 0) {
        av[0] = path;
        av[1] = 0;
        ev[0] = 0;
        proc_sys(SYS_execve, (long)path, (long)av, (long)ev, 0, 0, 0);
        exit(5);
    }
    if (pid < 0) {
        fail("musl clone", pid);
        return;
    }
    code = wait_exit(pid);
    if (code != 0)
        fail(path, code);
}

int main(int argc, char **argv)
{
    test_fork();
    test_pipe();
    test_exec();
    test_dirs();
    test_writev();
    run_musl("/initrd/bin/hello-musl");
    run_musl("/initrd/bin/musl-proc");
    run_musl("/initrd/bin/mem");
    run_musl("/initrd/bin/hard");
    test_outbox();
    if (fails)
        fail("total", fails);
    say("tests done\n");
    for (;;)
        ;
    return 0;
}
