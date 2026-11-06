/* Tilck for long builds (plan 4, phase 1), a musl program run by proc.
   Each check prints its mark or "hard fail: <what> <value>":
   fdmax ok     200 files open at once (and a fork with them open);
   chown ok     chown/fchown with -1 (unchanged) and 0 succeed;
   rootperm ok  root writes a 0444 file and creates a file in a 0555 dir;
   xbit ok      access() checks modes as Linux does for root (X_OK needs an
                x bit, except for a directory), execve() of a 0001 file
                works and of a 0644 file fails with EACCES;
   fatseek ok   lseek(SEEK_END) on a file of the FAT32 initrd;
   deepstack ok 4 MB of recursion on the user stack with a fork() in the
                middle of it, inside 50 nested directories (getcwd, stat);
   brkzero ok   pages brk() hands out again are zero (plan 4 phase 2);
   prio ok      getpriority()/setpriority()/nice() work (nice level 0);
   scriptlock ok a "#!" script that ran can be opened for writing;
   sharedoff ok a forked child and a dup'ed fd share the file offset of
                the open file (plan 4 phase 3), FD_CLOEXEC stays per fd;
   abort ok     abort() kills with SIGABRT, also from a signal handler;
   killcwd ok   a process killed by SIGKILL while sleeping releases its
                cwd (rmdir of it works after waitpid);
   pipewrite ok a blocking write() of 1 MB to a pipe returns 1 MB (plan 5
                phase 2), writes of 4096 bytes (PIPE_BUF) stay whole;
   ttywrite ok  a blocking write() of 16 KB to the console returns 16 KB;
   rlimit ok    getrlimit()/setrlimit() work: 256 files, 8 MB stack;
   munmapspan ok a munmap() over the end of one mmap() and the start of
                the next unmaps both parts and keeps the rest (plan 5
                phase 3);
   munmappart ok munmap() of the tail or the middle of an mmap() frees just
                those pages: later mmap()s never overlap live pages, under
                random mmap()/munmap() of up to 48 MB too;
   mremap ok    mremap() shrinks a mapping in place, and musl's realloc()
                of a big (mmap()ed) block to a small size keeps the heap
                around the new block intact;
   scriptargs ok execve() of a "#!" script with 400 arguments runs its
                interpreter with all of them (was E2BIG from 30 on).
   Exits 1 if any check failed. "hard xchild" is the exec'd side of the
   xbit check: it exits 42. */
#define _GNU_SOURCE 1 /* mremap(), MREMAP_MAYMOVE */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define SELF "/initrd/bin/hard"
#define TOP "/tmp/hard"
#define NFD 200
#define NDIR 50
#define DEPTH 4096 /* frames of a bit over 1 KB: over 4 MB of stack */
#define FORK_AT 2048

static int fails;

static void fail(const char *what, long v)
{
    printf("hard fail: %s %ld\n", what, v);
    fflush(stdout);
    fails = fails + 1;
}

static void mark(int before, const char *m)
{
    if (fails == before)
        printf("%s\n", m);
    fflush(stdout);
}

/* exit code of pid, or -1000 - status if it did not exit normally */
static long wait_code(pid_t pid)
{
    int st;
    pid_t r;

    st = -1;
    r = waitpid(pid, &st, 0);
    if (r != pid)
        return r < 0 ? -errno : -999;
    if (!WIFEXITED(st))
        return -1000 - st;
    return WEXITSTATUS(st);
}

/* 0, or -errno */
static long make_file(const char *path, mode_t mode)
{
    int fd;

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        return -errno;
    if (write(fd, "x", 1) != 1) {
        close(fd);
        return -1000;
    }
    close(fd);
    if (chmod(path, mode) != 0)
        return -errno;
    return 0;
}

static void test_fdmax(void)
{
    int before;
    int fds[NFD];
    char path[64];
    int n;
    int i;
    pid_t pid;
    long code;

    before = fails;
    mkdir(TOP "/fd", 0755);
    for (n = 0; n < NFD; n++) {
        snprintf(path, sizeof(path), TOP "/fd/f%d", n);
        fds[n] = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
        if (fds[n] < 0) {
            fail("fdmax open", n);
            break;
        }
    }
    if (n == NFD) {
        for (i = 0; i < NFD; i++)
            if (write(fds[i], "y", 1) != 1) {
                fail("fdmax write", i);
                break;
            }
        pid = fork();
        if (pid == 0)
            _exit(write(fds[NFD - 1], "z", 1) == 1 ? 0 : 1);
        code = pid < 0 ? pid : wait_code(pid);
        if (code != 0)
            fail("fdmax fork", code);
    }
    for (i = 0; i < n; i++)
        if (close(fds[i]) != 0)
            fail("fdmax close", i);
    mark(before, "fdmax ok");
}

static void test_chown(void)
{
    int before;
    int fd;
    long rc;

    before = fails;
    if ((rc = make_file(TOP "/own", 0644)) != 0)
        fail("chown create", rc);
    if (chown(TOP "/own", -1, -1) != 0)
        fail("chown -1 -1", errno);
    if (chown(TOP "/own", 0, 0) != 0)
        fail("chown 0 0", errno);
    if (lchown(TOP "/own", -1, 0) != 0)
        fail("lchown -1 0", errno);
    fd = open(TOP "/own", O_RDONLY);
    if (fd < 0 || fchown(fd, -1, -1) != 0)
        fail("fchown -1 -1", errno);
    if (fd >= 0)
        close(fd);
    if (chown(TOP "/missing", -1, -1) == 0 || errno != ENOENT)
        fail("chown missing", errno);
    mark(before, "chown ok");
}

static void test_rootperm(void)
{
    int before;
    int fd;
    long rc;

    before = fails;
    if ((rc = make_file(TOP "/ro", 0444)) != 0)
        fail("rootperm create", rc);
    fd = open(TOP "/ro", O_WRONLY | O_TRUNC);
    if (fd < 0)
        fail("rootperm open 0444 for write", errno);
    else {
        if (write(fd, "w", 1) != 1)
            fail("rootperm write 0444", errno);
        close(fd);
    }
    if ((rc = make_file(TOP "/none", 0)) != 0)
        fail("rootperm create 0000", rc);
    fd = open(TOP "/none", O_RDONLY);
    if (fd < 0)
        fail("rootperm open 0000 for read", errno);
    else
        close(fd);
    if (mkdir(TOP "/rodir", 0755) != 0 || chmod(TOP "/rodir", 0555) != 0)
        fail("rootperm mkdir", errno);
    if ((rc = make_file(TOP "/rodir/f", 0644)) != 0)
        fail("rootperm create in 0555 dir", rc);
    if (mkdir(TOP "/rodir/d", 0755) != 0)
        fail("rootperm mkdir in 0555 dir", errno);
    if (unlink(TOP "/rodir/f") != 0)
        fail("rootperm unlink in 0555 dir", errno);
    mark(before, "rootperm ok");
}

/* 0 if access(path, mode) gives want (0 or an errno) */
static void check_access(const char *path, int mode, int want, const char *what)
{
    int r;

    errno = 0;
    r = access(path, mode);
    if (want == 0 ? r != 0 : r != -1 || errno != want)
        fail(what, r == 0 ? 0 : errno);
}

/* fork + execve(path, {"hard", "xchild"}); the child's exit code, or
   100 + errno if execve failed */
static long run_x(const char *path)
{
    char *av[3];
    pid_t pid;

    pid = fork();
    if (pid == 0) {
        av[0] = "hard";
        av[1] = "xchild";
        av[2] = 0;
        execv(path, av);
        _exit(100 + errno);
    }
    if (pid < 0)
        return pid;
    return wait_code(pid);
}

static long copy_file(const char *from, const char *to)
{
    char buf[8192];
    int in;
    int out;
    long n;

    in = open(from, O_RDONLY);
    if (in < 0)
        return -errno;
    out = open(to, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) {
        close(in);
        return -errno;
    }
    while ((n = read(in, buf, sizeof(buf))) > 0)
        if (write(out, buf, n) != n) {
            n = -1;
            break;
        }
    close(in);
    close(out);
    return n < 0 ? -1000 : 0;
}

static void test_xbit(void)
{
    int before;
    long rc;
    long code;

    before = fails;
    if ((rc = make_file(TOP "/r", 0444)) != 0)
        fail("xbit create", rc);
    check_access(TOP "/r", F_OK, 0, "access 0444 F_OK");
    check_access(TOP "/r", R_OK | W_OK, 0, "access 0444 R_OK|W_OK");
    check_access(TOP "/r", X_OK, EACCES, "access 0444 X_OK");
    if ((rc = make_file(TOP "/o", 0001)) != 0)
        fail("xbit create 0001", rc);
    check_access(TOP "/o", X_OK, 0, "access 0001 X_OK");
    check_access(TOP "/o", R_OK, 0, "access 0001 R_OK");
    if (mkdir(TOP "/nox", 0755) != 0 || chmod(TOP "/nox", 0) != 0)
        fail("xbit mkdir", errno);
    check_access(TOP "/nox", X_OK, 0, "access dir 0000 X_OK");
    check_access(TOP "/nothere", F_OK, ENOENT, "access missing");
    if ((rc = copy_file(SELF, TOP "/xbin")) != 0)
        fail("xbit copy", rc);
    if (chmod(TOP "/xbin", 0001) != 0)
        fail("xbit chmod 0001", errno);
    if ((code = run_x(TOP "/xbin")) != 42)
        fail("xbit exec 0001", code);
    if (chmod(TOP "/xbin", 0644) != 0)
        fail("xbit chmod 0644", errno);
    if ((code = run_x(TOP "/xbin")) != 100 + EACCES)
        fail("xbit exec 0644", code);
    mark(before, "xbit ok");
}

static void test_fatseek(void)
{
    int before;
    struct stat st;
    char buf[100];
    int fd;
    off_t r;

    before = fails;
    fd = open(SELF, O_RDONLY);
    if (fd < 0 || fstat(fd, &st) != 0 || st.st_size < 4096) {
        fail("fatseek open", errno);
        if (fd >= 0)
            close(fd);
        return;
    }
    if (read(fd, buf, sizeof(buf)) != sizeof(buf))
        fail("fatseek read", errno);
    if ((r = lseek(fd, 0, SEEK_END)) != st.st_size)
        fail("fatseek SEEK_END 0", (long)r);
    if ((r = lseek(fd, -16, SEEK_END)) != st.st_size - 16)
        fail("fatseek SEEK_END -16", (long)r);
    if (read(fd, buf, sizeof(buf)) != 16)
        fail("fatseek read tail", errno);
    if ((r = lseek(fd, 0, SEEK_CUR)) != st.st_size)
        fail("fatseek SEEK_CUR", (long)r);
    if ((r = lseek(fd, 8, SEEK_END)) != st.st_size + 8)
        fail("fatseek SEEK_END +8", (long)r);
    if (read(fd, buf, sizeof(buf)) != 0)
        fail("fatseek read past end", errno);
    if ((r = lseek(fd, 4, SEEK_SET)) != 4 || read(fd, buf, 4) != 4
        || memcmp(buf, "\002\001\001", 3) != 0)
        fail("fatseek SEEK_SET", (long)r);
    close(fd);
    mark(before, "fatseek ok");
}

static pid_t deep_pid = -1;

/* n frames of a bit over 1 KB; each adds its own two bytes back */
static unsigned long deep(unsigned long n)
{
    volatile unsigned char buf[1024];
    unsigned long r;

    buf[0] = (unsigned char)n;
    buf[sizeof(buf) - 1] = (unsigned char)(n >> 3);
    if (n == FORK_AT)
        deep_pid = fork();
    r = n ? deep(n - 1) : 0;
    return r + buf[0] + buf[sizeof(buf) - 1];
}

static void test_deepstack(void)
{
    int before;
    char cwd[512];
    char want[512];
    char *p;
    struct stat st;
    unsigned long sum;
    unsigned long got;
    unsigned long i;
    long code;
    int k;

    before = fails;
    if (mkdir(TOP "/deep", 0755) != 0 || chdir(TOP "/deep") != 0) {
        fail("deepstack mkdir", errno);
        return;
    }
    strcpy(want, TOP "/deep");
    for (k = 0; k < NDIR; k++) {
        if (mkdir("d", 0755) != 0 || chdir("d") != 0) {
            fail("deepstack nested dir", k);
            return;
        }
        strcat(want, "/d");
    }
    p = getcwd(cwd, sizeof(cwd));
    if (!p || strcmp(cwd, want) != 0)
        fail("deepstack getcwd", p ? (long)strlen(cwd) : -errno);
    strcat(want, "/f");
    if (make_file("f", 0644) != 0 || stat(want, &st) != 0 || st.st_size != 1)
        fail("deepstack stat", errno);
    sum = 0;
    for (i = 0; i <= DEPTH; i++)
        sum = sum + (unsigned char)i + (unsigned char)(i >> 3);
    got = deep(DEPTH);
    if (deep_pid == 0)
        _exit(got == sum && getcwd(cwd, sizeof(cwd)) != 0 ? 0 : 1);
    if (got != sum)
        fail("deepstack sum", (long)got);
    code = deep_pid < 0 ? deep_pid : wait_code(deep_pid);
    if (code != 0)
        fail("deepstack fork", code);
    chdir("/");
    mark(before, "deepstack ok");
}

/* Linux gives zeroed pages for a growing brk, and malloc'd memory taken
   from it starts zeroed (gawk 3.0.4 reads a node field it never set).
   Grow brk by 64 pages, dirty them, give them back, grow again: every
   byte must be 0. musl's brk()/sbrk() refuse, so the raw syscall. */
static void test_brkzero(void)
{
    int before = fails;
    unsigned long b0, a, n = 64 * 4096, i;
    long bad = 0;
    char *p;

    b0 = syscall(SYS_brk, 0);
    a = (b0 + 4095) & ~4095UL;
    if ((unsigned long)syscall(SYS_brk, a + n) != a + n) {
        fail("brkzero grow", (long)b0);
        return;
    }
    memset((char *)a, 0xAA, n);
    if ((unsigned long)syscall(SYS_brk, a) != a) {
        fail("brkzero shrink", (long)a);
        return;
    }
    if ((unsigned long)syscall(SYS_brk, a + n) != a + n) {
        fail("brkzero regrow", (long)a);
        return;
    }
    p = (char *)a;
    for (i = 0; i < n; i++)
        if (p[i])
            bad = bad + 1;
    if (bad)
        fail("brkzero nonzero bytes", bad);
    syscall(SYS_brk, a);
    mark(before, "brkzero ok");
}

/* coreutils' nice asks getpriority() first; Tilck has one nice level */
static void test_prio(void)
{
    int before = fails;
    int r;

    errno = 0;
    r = getpriority(PRIO_PROCESS, 0);
    if (r != 0 || errno != 0)
        fail("prio getpriority", errno ? -errno : r);
    if (setpriority(PRIO_PROCESS, 0, 0) != 0)
        fail("prio setpriority", -errno);
    errno = 0;
    r = nice(0);
    if (r != 0 || errno != 0)
        fail("prio nice", errno ? -errno : r);
    if (getpriority(5, 0) != -1 || errno != EINVAL)
        fail("prio which 5", errno);
    mark(before, "prio ok");
}

/* execve() of a "#!" script first tries it as an ELF file; that attempt
   took the exec lock of the script's inode and never gave it back, so
   the script stayed "busy" for writers (ETXTBSY), and after unlink a new
   file whose inode reused the address was busy too */
static void test_scriptlock(void)
{
    int before = fails;
    long code;
    int fd;

    fd = open(TOP "/s.sh", O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (fd < 0 || write(fd, "#!" SELF " xchild\n", strlen("#!" SELF " xchild\n")) < 0) {
        fail("scriptlock create", errno);
        return;
    }
    close(fd);
    if ((code = run_x(TOP "/s.sh")) != 42)
        fail("scriptlock run", code);
    fd = open(TOP "/s.sh", O_WRONLY);
    if (fd < 0)
        fail("scriptlock open for write", errno);
    else
        close(fd);
    mark(before, "scriptlock ok");
}

static long write_str(int fd, const char *s)
{
    return write(fd, s, strlen(s)) == (long)strlen(s) ? 0 : -1;
}

/* Linux: the fds of a fork() child and of dup()/dup2() refer to the same
   open file description, so they share its offset (and the O_ flags);
   only FD_CLOEXEC belongs to the fd. `(echo aaaa; echo bb) > f` and every
   config.log rely on it. */
static void test_sharedoff(void)
{
    int before = fails;
    const char *want = "p1\nc1\np2\nd\np3\n";
    char buf[64];
    pid_t pid;
    long code;
    long n;
    int fd;
    int fd2;

    fd = open(TOP "/shared", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0 || write_str(fd, "p1\n") != 0) {
        fail("sharedoff open", errno);
        return;
    }
    pid = fork();
    if (pid == 0)
        _exit(write_str(fd, "c1\n") == 0 ? 0 : 1);
    if ((code = pid < 0 ? pid : wait_code(pid)) != 0)
        fail("sharedoff child", code);
    if ((n = lseek(fd, 0, SEEK_CUR)) != 6)
        fail("sharedoff offset after child", n);
    write_str(fd, "p2\n");
    fd2 = dup2(fd, 20);
    if (fd2 != 20 || write_str(fd2, "d\n") != 0)
        fail("sharedoff dup2", errno);
    write_str(fd, "p3\n");
    if (fcntl(fd2, F_SETFD, FD_CLOEXEC) != 0 || fcntl(fd2, F_GETFD) != FD_CLOEXEC
        || fcntl(fd, F_GETFD) != 0)
        fail("sharedoff cloexec per fd", fcntl(fd, F_GETFD));
    pid = fork();
    if (pid == 0)
        _exit(lseek(fd, 3, SEEK_SET) == 3 ? 0 : 1);
    if ((code = pid < 0 ? pid : wait_code(pid)) != 0)
        fail("sharedoff child lseek", code);
    if ((n = lseek(fd2, 0, SEEK_CUR)) != 3)
        fail("sharedoff offset after child lseek", n);
    close(fd2);
    close(fd);
    fd = open(TOP "/shared", O_RDONLY);
    n = fd < 0 ? -errno : read(fd, buf, sizeof(buf) - 1);
    if (fd >= 0)
        close(fd);
    if (n != (long)strlen(want) || memcmp(buf, want, n) != 0) {
        if (n > 0) {
            buf[n] = 0;
            printf("hard: sharedoff file [%s]\n", buf);
        }
        fail("sharedoff content", n);
    }
    mark(before, "sharedoff ok");
}

/* waitpid() for up to secs seconds: the signal that killed pid, 1000 +
   its exit code, or -1 if it is still alive (then it gets SIGKILL) */
static long wait_signal(pid_t pid, int secs)
{
    struct timespec ts = {0, 100000000};
    int st = 0;
    int i;
    pid_t r;

    for (i = 0; i < secs * 10; i++) {
        r = waitpid(pid, &st, WNOHANG);
        if (r == pid)
            return WIFSIGNALED(st) ? WTERMSIG(st) : 1000 + WEXITSTATUS(st);
        if (r < 0)
            return -1000 - errno;
        nanosleep(&ts, 0);
    }
    kill(pid, SIGKILL);
    waitpid(pid, &st, 0);
    return -1;
}

/* gawk 3.0.4's SIGSEGV handler prints "fatal error: internal error" and
   calls abort() */
static void segv_abort(int sig)
{
    (void)sig;
    abort();
}

static void test_abort(void)
{
    int before = fails;
    long code;
    pid_t pid;

    pid = fork();
    if (pid == 0)
        abort();
    if ((code = pid < 0 ? pid : wait_signal(pid, 10)) != SIGABRT)
        fail("abort plain", code);
    pid = fork();
    if (pid == 0) {
        signal(SIGSEGV, segv_abort);
        *(volatile int *)0 = 1;
        _exit(3);
    }
    if ((code = pid < 0 ? pid : wait_signal(pid, 10)) != SIGABRT)
        fail("abort in SIGSEGV handler", code);
    mark(before, "abort ok");
}

/* a child chdir()s into dir, tells the parent through the pipe, then
   sleeps in pause() (how 0), read() of an empty pipe (1) or nanosleep()
   (2); SIGKILL, waitpid(), then rmdir(dir) must work */
static void killcwd_one(const char *dir, int how)
{
    struct timespec ts = {1000, 0};
    int sync[2];
    int idle[2];
    long code;
    char c;
    pid_t pid;

    if (mkdir(dir, 0755) != 0 || pipe(sync) != 0 || pipe(idle) != 0) {
        fail("killcwd setup", errno);
        return;
    }
    pid = fork();
    if (pid == 0) {
        if (chdir(dir) != 0)
            _exit(1);
        write(sync[1], "x", 1);
        if (how == 0)
            pause();
        else if (how == 1)
            read(idle[0], &c, 1);
        else
            nanosleep(&ts, 0);
        _exit(2);
    }
    if (pid < 0 || read(sync[0], &c, 1) != 1) {
        fail("killcwd fork", pid);
        return;
    }
    nanosleep(&(struct timespec){0, 200000000}, 0);
    kill(pid, SIGKILL);
    if ((code = wait_signal(pid, 10)) != SIGKILL)
        fail("killcwd wait", code);
    if (rmdir(dir) != 0)
        fail("killcwd rmdir", 100 * how + errno);
    close(sync[0]);
    close(sync[1]);
    close(idle[0]);
    close(idle[1]);
}

static void test_killcwd(void)
{
    int before = fails;

    killcwd_one(TOP "/kc0", 0);
    killcwd_one(TOP "/kc1", 1);
    killcwd_one(TOP "/kc2", 2);
    mark(before, "killcwd ok");
}

#define PW_SIZE (1L << 20)
#define PW_BLOCKS 64

static char pw_byte(long i)
{
    return (char)(i * 7 + (i >> 12));
}

/* POSIX: a blocking write() to a pipe transfers all its bytes, waiting for
   the reader as needed, and one of at most PIPE_BUF (4096) bytes is not
   interleaved with other writers' data. tar 1.12 takes a short write to a
   pipe as fatal ("Only wrote 4096 of 10240 bytes to -"). */
static void test_pipewrite(void)
{
    int before = fails;
    struct timespec ts = {0, 10000000};
    char rb[1000];
    char *buf;
    pid_t pid;
    pid_t w[2];
    long code;
    long got;
    long n;
    long i;
    int fd[2];
    int k;

    buf = malloc(PW_SIZE);
    if (!buf || pipe(fd) != 0) {
        fail("pipewrite pipe", errno);
        return;
    }
    for (i = 0; i < PW_SIZE; i++)
        buf[i] = pw_byte(i);
    /* 1 MB in one write(), a slow reader child checks every byte */
    pid = fork();
    if (pid == 0) {
        close(fd[1]);
        got = 0;
        k = 0;
        while ((n = read(fd[0], rb, sizeof(rb))) > 0) {
            for (i = 0; i < n; i++)
                if (rb[i] != pw_byte(got + i))
                    k = 1;
            if ((got + n) / 65536 != got / 65536)
                nanosleep(&ts, 0);
            got += n;
        }
        _exit(k ? 3 : got == PW_SIZE ? 0 : 4);
    }
    close(fd[0]);
    n = pid < 0 ? -1 : write(fd[1], buf, PW_SIZE);
    if (n != PW_SIZE)
        fail("pipewrite write", n < 0 ? -errno : n);
    close(fd[1]);
    if ((code = pid < 0 ? pid : wait_code(pid)) != 0)
        fail("pipewrite reader", code);
    /* two writers of 4096-byte blocks ('A' and 'B'), a reader taking odd
       sizes: every aligned 4096-byte block of the stream is one writer's */
    if (pipe(fd) != 0) {
        fail("pipewrite pipe2", errno);
        return;
    }
    for (k = 0; k < 2; k++) {
        w[k] = fork();
        if (w[k] == 0) {
            close(fd[0]);
            memset(buf, 'A' + k, 4096);
            for (i = 0; i < PW_BLOCKS; i++)
                if (write(fd[1], buf, 4096) != 4096)
                    _exit(5);
            _exit(0);
        }
    }
    close(fd[1]);
    got = 0;
    while (got < 2 * PW_BLOCKS * 4096
           && (n = read(fd[0], buf + got, sizeof(rb))) > 0)
        got += n;
    close(fd[0]);
    for (k = 0; k < 2; k++)
        if ((code = w[k] < 0 ? w[k] : wait_code(w[k])) != 0)
            fail("pipewrite writer", code);
    if (got != 2 * PW_BLOCKS * 4096)
        fail("pipewrite atomic total", got);
    for (i = 0; i + 4096 <= got; i += 4096)
        for (n = 1; n < 4096; n++)
            if (buf[i + n] != buf[i]) {
                fail("pipewrite atomic block", i / 4096);
                i = got;
                break;
            }
    free(buf);
    mark(before, "pipewrite ok");
}

/* a blocking write() to the console (a tty) takes the whole count too: one
   line of 16383 spaces */
static void test_ttywrite(void)
{
    int before = fails;
    static char sp[16384];
    long n;

    memset(sp, ' ', sizeof(sp) - 1);
    sp[sizeof(sp) - 1] = '\n';
    n = write(1, sp, sizeof(sp));
    if (n != (long)sizeof(sp))
        fail("ttywrite", n < 0 ? -errno : n);
    mark(before, "ttywrite ok");
}

/* getrlimit() tells the real limits: 256 open files, 8 MB of stack, the
   rest unlimited (binutils' BFD keeps RLIMIT_NOFILE / 8 archive members
   open; with getrlimit() failing, musl's sysconf(_SC_OPEN_MAX) returned an
   uninitialized value and `ar rc` of musl's 1324 objects ran out of fds);
   setrlimit() accepts values it does not keep */
static void test_rlimit(void)
{
    int before = fails;
    struct rlimit r;
    struct rlimit big;

    if (getrlimit(RLIMIT_NOFILE, &r) != 0)
        fail("rlimit getrlimit NOFILE", errno);
    else if (r.rlim_cur != 256 || r.rlim_max != 256)
        fail("rlimit NOFILE", (long)r.rlim_cur);
    if (sysconf(_SC_OPEN_MAX) != 256)
        fail("rlimit sysconf OPEN_MAX", sysconf(_SC_OPEN_MAX));
    if (getrlimit(RLIMIT_STACK, &r) != 0)
        fail("rlimit getrlimit STACK", errno);
    else if (r.rlim_cur != 8L << 20)
        fail("rlimit STACK", (long)r.rlim_cur);
    if (getrlimit(RLIMIT_AS, &r) != 0)
        fail("rlimit getrlimit AS", errno);
    else if (r.rlim_cur != RLIM_INFINITY || r.rlim_max != RLIM_INFINITY)
        fail("rlimit AS", (long)r.rlim_cur);
    if (getrlimit(99, &r) != -1 || errno != EINVAL)
        fail("rlimit getrlimit 99", errno);
    r.rlim_cur = 256;
    r.rlim_max = 256;
    if (setrlimit(RLIMIT_NOFILE, &r) != 0)
        fail("rlimit setrlimit NOFILE", errno);
    big.rlim_cur = 512;
    big.rlim_max = 256;
    if (setrlimit(RLIMIT_NOFILE, &big) != -1 || errno != EINVAL)
        fail("rlimit setrlimit cur > max", errno);
    mark(before, "rlimit ok");
}

/* munmap() of a range that covers the end of one mmap() and the start of
   the next one, as GCC's ggc-page does (release_pages() unmaps adjacent free
   pages together, whichever mmap() they came from): both parts go, the rest
   of both mappings stays, and new mmap()s do not hand out the pages that are
   still mapped (Tilck shrank the first mapping only and freed the range in
   its mmap heap, so the next mmap() returned the first mapping's live head:
   cc1 died on large functions, its GC pages written over) */
static void test_munmapspan(void)
{
    int before = fails;
    long pg = 4096, i, k;
    char *a, *b, *c[4];

    a = mmap(0, 4 * pg, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    b = mmap(0, 4 * pg, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (a == MAP_FAILED || b == MAP_FAILED) {
        fail("munmapspan mmap", errno);
        return;
    }
    if (b != a + 4 * pg) {
        fail("munmapspan not adjacent", (long)(b - a));
        return;
    }
    memset(a, 0xa1, 4 * pg);
    memset(b, 0xb2, 4 * pg);
    if (munmap(a + 2 * pg, 4 * pg) != 0)
        fail("munmapspan munmap", errno);
    for (k = 0; k < 4; k++) {
        c[k] = mmap(0, pg, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (c[k] == MAP_FAILED) {
            fail("munmapspan mmap after", errno);
            return;
        }
        memset(c[k], 0xc3, pg);
    }
    for (i = 0; i < 2 * pg; i++)
        if ((unsigned char)a[i] != 0xa1) {
            fail("munmapspan first head", i);
            break;
        }
    for (i = 2 * pg; i < 4 * pg; i++)
        if ((unsigned char)b[i] != 0xb2) {
            fail("munmapspan second tail", i);
            break;
        }
    munmap(a, 2 * pg);
    munmap(b + 2 * pg, 2 * pg);
    for (k = 0; k < 4; k++)
        munmap(c[k], pg);
    mark(before, "munmapspan ok");
}

static char *mmap_pages(long n)
{
    char *p = mmap(0, n * 4096, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    return p == MAP_FAILED ? 0 : p;
}

/* the first byte of [p, p + n) that is not v, or -1 */
static long not_all(const char *p, long n, int v)
{
    long i;

    for (i = 0; i < n; i++)
        if ((unsigned char)p[i] != v)
            return i;
    return -1;
}

static int overlaps(const char *p, long n, const char *q, long m)
{
    return p < q + m && q < p + n;
}

/* munmap() of the tail or of pages in the middle of one mmap() gives back
   those pages only. Tilck freed a part of a multi-step allocation by the
   bits of its size: 3 pages at page 3 of a 6-page mmap() (4 + 2 pages)
   freed the aligned 2-page block of pages 2-3, page 2 still mapped, and a
   later 2-page mmap() returned it (GCC's ggc-page frees such parts) */
static unsigned munmap_rnd = 12345;

static unsigned munmap_r(void)
{
    munmap_rnd = munmap_rnd * 1103515245u + 12345u;
    return munmap_rnd >> 8;
}

static void test_munmappart(void)
{
    enum { SLOTS = 200 };
    static char *p[SLOTS];
    static long np[SLOTS];
    int before = fails;
    long pg = 4096, k, total = 0, round, bad = 0;
    char *a, *m, *c[6];
    int i, j;

    a = mmap_pages(6);
    m = mmap_pages(7);
    if (!a || !m) {
        fail("munmappart mmap", errno);
        return;
    }
    memset(a, 0xa1, 6 * pg);
    memset(m, 0xd4, 7 * pg);
    if (munmap(a + 3 * pg, 3 * pg) != 0)
        fail("munmappart tail", errno);
    if (munmap(m + 3 * pg, 2 * pg) != 0)
        fail("munmappart middle", errno);
    for (k = 0; k < 6; k++) {
        long n = k % 3 + 1;

        if (!(c[k] = mmap_pages(n))) {
            fail("munmappart mmap after", errno);
            return;
        }
        if (overlaps(c[k], n * pg, a, 3 * pg) ||
            overlaps(c[k], n * pg, m, 3 * pg) ||
            overlaps(c[k], n * pg, m + 5 * pg, 2 * pg))
            fail("munmappart overlap", (long)(c[k] - a) / pg);
        memset(c[k], 0xc3, n * pg);
    }
    if ((k = not_all(a, 3 * pg, 0xa1)) >= 0)
        fail("munmappart tail kept", k);
    if ((k = not_all(m, 3 * pg, 0xd4)) >= 0)
        fail("munmappart middle head", k);
    if ((k = not_all(m + 5 * pg, 2 * pg, 0xd4)) >= 0)
        fail("munmappart middle tail", k);
    munmap(a, 3 * pg);
    munmap(m, 3 * pg);
    munmap(m + 5 * pg, 2 * pg);
    for (k = 0; k < 6; k++)
        munmap(c[k], (k % 3 + 1) * pg);

    /* random mmap()s, whole and tail munmap()s, past the mmap heap's
       first growths (16 and 32 MB) */
    for (round = 0; round < 3000 && !bad; round++) {
        i = (int)(munmap_r() % SLOTS);
        if (p[i]) {
            if (not_all(p[i], np[i] * pg, i + 1) >= 0) {
                fail("munmappart random clobbered", round);
                bad = 1;
            }
            if (np[i] > 2 && (munmap_r() & 1)) {
                long h = np[i] / 2;

                munmap(p[i] + (np[i] - h) * pg, h * pg);
                np[i] -= h;
                total -= h * pg;
            } else {
                munmap(p[i], np[i] * pg);
                total -= np[i] * pg;
                p[i] = 0;
            }
        } else if (total < (48L << 20)) {
            long n = 1 + munmap_r() % ((munmap_r() & 7) ? 8 : 300);
            char *q = mmap_pages(n);

            if (!q) {
                fail("munmappart random mmap", errno);
                bad = 1;
                break;
            }
            for (j = 0; j < SLOTS; j++)
                if (p[j] && overlaps(q, n * pg, p[j], np[j] * pg)) {
                    fail("munmappart random overlap", round);
                    bad = 1;
                    break;
                }
            p[i] = q;
            np[i] = n;
            total += n * pg;
            memset(q, i + 1, n * pg);
        }
    }
    for (i = 0; i < SLOTS; i++)
        if (p[i])
            munmap(p[i], np[i] * pg);
    mark(before, "munmappart ok");
}

/* execve() of a "#!" script with many arguments: GCC's build runs its
   linker through the script gcc/collect-ld (`exec ld "$@"`) with the ~350
   arguments of cc1's link line; Tilck took at most 30 for a script (fixed
   argv arrays in the execve context) and returned E2BIG. The interpreter
   ("hard xargs") gets interp, its argument, the script path, then the
   script's argv[1..] and exits 42 if they are all there. */
#define SCRIPT_ARGS 400

static int xargs_main(int argc, char **argv)
{
    char want[16];
    int i;

    if (argc != SCRIPT_ARGS + 2 || strcmp(argv[2], TOP "/args.sh") != 0)
        return 1;
    for (i = 3; i < argc; i++) {
        snprintf(want, sizeof want, "a%d", i - 2);
        if (strcmp(argv[i], want) != 0)
            return 2;
    }
    return 42;
}

static void test_scriptargs(void)
{
    static char *av[SCRIPT_ARGS + 1];
    static char names[SCRIPT_ARGS][16];
    int before = fails;
    long code;
    pid_t pid;
    int fd;
    int i;

    fd = open(TOP "/args.sh", O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (fd < 0 || write_str(fd, "#!" SELF " xargs\n") != 0) {
        fail("scriptargs create", errno);
        return;
    }
    close(fd);
    av[0] = "args.sh";
    for (i = 1; i < SCRIPT_ARGS; i++) {
        snprintf(names[i], sizeof names[i], "a%d", i);
        av[i] = names[i];
    }
    av[SCRIPT_ARGS] = 0;
    pid = fork();
    if (pid == 0) {
        execv(TOP "/args.sh", av);
        _exit(100 + errno);
    }
    if (pid < 0) {
        fail("scriptargs fork", errno);
        return;
    }
    if ((code = wait_code(pid)) != 42)
        fail("scriptargs run", code);
    mark(before, "scriptargs ok");
}

/* mremap() shrinks in place (Linux never fails that); musl 1.1.24's realloc()
   of a block it got from mmap() calls mremap() and, if that fails, copies
   the OLD size into a new malloc() of the new size, also when shrinking. With
   Tilck's mremap() returning ENOSYS that wrote past the new block over the
   rest of the heap (GCC's cc1 shrinks big tables with xrealloc). */
static void test_mremap(void)
{
    enum { SMALL = 64 };
    int before = fails;
    long pg = 4096, k;
    char *a, *r, *big, *small[SMALL];
    int i;

    a = mmap_pages(64);
    if (!a) {
        fail("mremap mmap", errno);
        return;
    }
    memset(a, 0x5a, 64 * pg);
    r = mremap(a, 64 * pg, 16 * pg, MREMAP_MAYMOVE);
    if (r == MAP_FAILED) {
        fail("mremap shrink", errno);
        munmap(a, 64 * pg);
    } else {
        if (r != a)
            fail("mremap shrink moved", (long)(r - a));
        if ((k = not_all(r, 16 * pg, 0x5a)) >= 0)
            fail("mremap shrink content", k);
        munmap(r, 16 * pg);
    }

    /* realloc() 2 MB -> 64 KB with the heap around it in use */
    big = malloc(2 << 20);
    if (!big) {
        fail("mremap malloc", errno);
        return;
    }
    memset(big, 0x6b, 2 << 20);
    for (i = 0; i < SMALL; i++) {
        small[i] = malloc(4000);
        if (small[i])
            memset(small[i], i + 1, 4000);
    }
    big = realloc(big, 64 << 10);
    for (i = 0; i < SMALL; i++) {
        char *q = malloc(4000);

        if (q)
            memset(q, 0x77, 4000);
        if (small[i] && not_all(small[i], 4000, i + 1) >= 0) {
            fail("mremap realloc clobbered", i);
            break;
        }
    }
    if (!big || not_all(big, 64 << 10, 0x6b) >= 0)
        fail("mremap realloc content", 0);
    mark(before, "mremap ok");
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "xchild") == 0)
        return 42;
    if (argc > 1 && strcmp(argv[1], "xargs") == 0)
        return xargs_main(argc, argv);
    setvbuf(stdout, 0, _IONBF, 0);
    if (mkdir(TOP, 0755) != 0)
        fail("mkdir " TOP, errno);
    test_fdmax();
    test_chown();
    test_rootperm();
    test_xbit();
    test_fatseek();
    test_deepstack();
    test_brkzero();
    test_prio();
    test_scriptlock();
    test_sharedoff();
    test_abort();
    test_killcwd();
    test_pipewrite();
    test_ttywrite();
    test_rlimit();
    test_munmapspan();
    test_munmappart();
    test_mremap();
    test_scriptargs();
    return fails ? 1 : 0;
}
