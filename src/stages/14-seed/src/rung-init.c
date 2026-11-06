/* rung-init: /initrd/bin/init of a Tilck rung (musl, static).
 *
 * Tilck starts init with no open files, so it opens /dev/console as fds
 * 0-2. It unpacks /initrd/rootfs.tar (POSIX ustar: directories and regular
 * files) into /, makes /bin/sh a symlink to /usr/bin/bash if that exists,
 * then runs
 *   /usr/bin/kaem --verbose --strict --file /initrd/steps/run.kaem
 * in a child with the rung environment, waits for it and prints
 * "rung exit N" (N = exit code, 128 + signal if killed, -1 if setup
 * failed). pid 1 must not exit: it then sleeps forever. As every init it
 * reaps the orphans Tilck gives it (waitpid(-1)), before and after kaem
 * exits: a zombie keeps its cwd and files referenced (rmdir: EBUSY).
 *
 * Modes: the header's permission bits, as mkustar (stage 13) took them
 * from stat() (from the content on Kronos: 0755 for an ELF executable). */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define TAR "/initrd/rootfs.tar"

static char hdr[512];
static char buf[65536];
static char path[512];

static char *kaem_argv[] = {
    "kaem", "--verbose", "--strict", "--file", "/initrd/steps/run.kaem", 0
};

static char *kaem_envp[] = {
    "PREFIX=/usr",
    "BINDIR=/usr/bin",
    "LIBDIR=/usr/lib",
    "INCDIR=/usr/include",
    "DISTFILES=/initrd/distfiles",
    "ARCH=riscv64",
    "PATH=/usr/bin",
    0
};

static void spin(void)
{
    for (;;)
        sleep(1000);
}

static void fail(const char *what, const char *arg)
{
    printf("rung-init fail: %s %s (errno %d)\n", what, arg, errno);
    printf("rung exit -1\n");
    fflush(stdout);
    spin();
}

/* read exactly n bytes; 0 at a clean end of file before the first byte */
static int read_full(int fd, char *p, unsigned long n)
{
    unsigned long got;
    long r;

    got = 0;
    while (got < n) {
        r = read(fd, p + got, n - got);
        if (r < 0)
            fail("read", TAR);
        if (r == 0)
            break;
        got = got + r;
    }
    if (got != 0 && got != n)
        fail("short read", TAR);
    return got != 0;
}

/* write all n bytes: a write may be short (Tilck: at most 12 KB per call) */
static void write_full(int fd, const char *p, unsigned long n)
{
    long r;

    while (n > 0) {
        r = write(fd, p, n);
        if (r <= 0)
            fail("write", path);
        p = p + r;
        n = n - r;
    }
}

static unsigned long octal(const char *f, int w)
{
    unsigned long n;
    int i;

    n = 0;
    for (i = 0; i < w && f[i] >= '0' && f[i] <= '7'; i = i + 1)
        n = n * 8 + (unsigned long)(f[i] - '0');
    return n;
}

/* mkdir -p of every parent of path (path itself excluded) */
static void parents(char *p)
{
    char *s;

    for (s = p + 1; *s; s = s + 1) {
        if (*s != '/')
            continue;
        *s = 0;
        if (mkdir(p, 0755) != 0 && errno != EEXIST)
            fail("mkdir", p);
        *s = '/';
    }
}

/* "/" + ustar prefix + "/" + name, trailing slashes removed */
static void member_path(void)
{
    unsigned long n;

    strcpy(path, "/");
    if (hdr[345]) {
        strncat(path, hdr + 345, 155);
        strcat(path, "/");
    }
    strncat(path, hdr, 100);
    n = strlen(path);
    while (n > 1 && path[n - 1] == '/') {
        n = n - 1;
        path[n] = 0;
    }
}

static void unpack(void)
{
    int in;
    int out;
    unsigned long size;
    unsigned long left;
    unsigned long n;
    unsigned long mode;
    unsigned long files;
    unsigned long dirs;
    char type;

    in = open(TAR, O_RDONLY);
    if (in < 0)
        fail("open", TAR);
    files = 0;
    dirs = 0;
    for (;;) {
        if (!read_full(in, hdr, 512) || hdr[0] == 0)
            break;
        if (memcmp(hdr + 257, "ustar", 5) != 0)
            fail("not ustar", TAR);
        member_path();
        size = octal(hdr + 124, 12);
        mode = octal(hdr + 100, 8) & 07777;
        type = hdr[156];
        parents(path);
        if (type == '5') {
            if (mkdir(path, mode) != 0 && errno != EEXIST)
                fail("mkdir", path);
            dirs = dirs + 1;
            continue;
        }
        if (type != '0' && type != 0)
            fail("unsupported member type in", path);
        out = -1;
        left = size;
        while (left > 0 || out < 0) {
            n = left < sizeof(buf) ? left : sizeof(buf);
            /* whole 512-byte blocks: the last one carries the padding */
            if (n > 0 && !read_full(in, buf, (n + 511) & ~511UL))
                fail("truncated member", path);
            if (out < 0) {
                out = open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
                if (out < 0)
                    fail("create", path);
            }
            write_full(out, buf, n);
            left = left - n;
        }
        close(out);
        /* open() applies the umask; the header mode is meant as is */
        if (chmod(path, mode) != 0)
            fail("chmod", path);
        files = files + 1;
    }
    close(in);
    printf("rung-init: %lu files, %lu dirs from %s\n", files, dirs, TAR);
}

/* /bin/sh -> /usr/bin/bash when the rootfs has bash (rung tools2 on) */
static void bin_sh(void)
{
    struct stat st;

    if (stat("/usr/bin/bash", &st) != 0 || !S_ISREG(st.st_mode))
        return;
    if (mkdir("/bin", 0755) != 0 && errno != EEXIST)
        fail("mkdir", "/bin");
    if (symlink("/usr/bin/bash", "/bin/sh") != 0)
        fail("symlink", "/bin/sh");
    printf("rung-init: /bin/sh -> /usr/bin/bash\n");
}

int main(void)
{
    int pid;
    int st;
    int code;

    open("/dev/console", O_RDONLY);
    open("/dev/console", O_WRONLY);
    open("/dev/console", O_WRONLY);
    setvbuf(stdout, 0, _IONBF, 0);
    printf("rung-init\n");
    unpack();
    bin_sh();
    if (chdir("/") != 0)
        fail("chdir", "/");
    pid = fork();
    if (pid < 0)
        fail("fork", "");
    if (pid == 0) {
        execve("/usr/bin/kaem", kaem_argv, kaem_envp);
        printf("rung-init fail: execve /usr/bin/kaem (errno %d)\n", errno);
        _exit(127);
    }
    for (;;) {
        pid_t w = waitpid(-1, &st, 0);
        if (w == pid)
            break;
        if (w < 0)
            fail("waitpid", "");
    }
    if (WIFEXITED(st))
        code = WEXITSTATUS(st);
    else if (WIFSIGNALED(st))
        code = 128 + WTERMSIG(st);
    else
        code = -1;
    printf("rung exit %d\n", code);
    for (;;)
        if (waitpid(-1, &st, 0) < 0)
            sleep(1);
    return 0;
}
