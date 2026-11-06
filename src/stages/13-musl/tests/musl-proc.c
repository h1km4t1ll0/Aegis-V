/* musl's process, file and memory APIs under Tilck. Each failure prints
   "musl proc fail: <what> <value>"; "musl proc ok" only when all passed.
   "musl-proc child ARG" is the exec'd side of the pipe test: it prints
   "child ARG" to stdout (the pipe) and exits 3. */
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define SELF "/initrd/bin/musl-proc"

static int fails;

static void fail(const char *what, long v)
{
    printf("musl proc fail: %s %ld\n", what, v);
    fflush(stdout);
    fails = fails + 1;
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

static void test_fork(void)
{
    pid_t pid;
    long code;

    pid = fork();
    if (pid == 0)
        _exit(7);
    if (pid < 0) {
        fail("fork", pid);
        return;
    }
    code = wait_code(pid);
    if (code != 7)
        fail("fork waitpid", code);
}

/* fork + pipe + dup2 + execv: the child is this program in child mode */
static void test_pipe_exec(void)
{
    int fds[2];
    char buf[64];
    char *av[4];
    pid_t pid;
    long got;
    long r;
    long code;

    if (pipe(fds) != 0) {
        fail("pipe", -errno);
        return;
    }
    pid = fork();
    if (pid == 0) {
        close(fds[0]);
        if (dup2(fds[1], 1) != 1)
            _exit(120);
        close(fds[1]);
        av[0] = "musl-proc";
        av[1] = "child";
        av[2] = "xyz";
        av[3] = 0;
        execv(SELF, av);
        _exit(121);
    }
    if (pid < 0) {
        fail("pipe fork", pid);
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
    code = wait_code(pid);
    if (code != 3)
        fail("exec child exit", code);
    if (strcmp(buf, "child xyz\n") != 0)
        fail("pipe read", got);
}

static void test_dirs(void)
{
    DIR *d;
    struct dirent *de;
    struct stat sb;
    FILE *f;
    char line[32];
    int seen;

    d = opendir("/initrd/bin");
    if (!d) {
        fail("opendir", -errno);
        return;
    }
    seen = 0;
    while ((de = readdir(d)) != 0) {
        if (strcmp(de->d_name, "hello-musl") == 0 || strcmp(de->d_name, "musl-proc") == 0)
            seen = seen + 1;
    }
    closedir(d);
    if (seen != 2)
        fail("readdir seen", seen);
    if (mkdir("/tmp/m", 0755) != 0) {
        fail("mkdir", -errno);
        return;
    }
    f = fopen("/tmp/m/f", "w");
    if (!f) {
        fail("fopen w", -errno);
        return;
    }
    fprintf(f, "%s %d\n", "line", 42);
    fclose(f);
    if (stat("/tmp/m/f", &sb) != 0 || sb.st_size != 8)
        fail("stat size", (long)sb.st_size);
    f = fopen("/tmp/m/f", "r");
    if (!f || !fgets(line, sizeof(line), f) || strcmp(line, "line 42\n") != 0)
        fail("fopen r", 0);
    if (f)
        fclose(f);
    if (unlink("/tmp/m/f") != 0)
        fail("unlink", -errno);
    if (rmdir("/tmp/m") != 0)
        fail("rmdir", -errno);
    /* stat, not access: Tilck's faccessat is a stub that returns 0 */
    if (stat("/tmp/m", &sb) == 0)
        fail("still there", 0);
}

static void test_malloc(void)
{
    char *p;
    char *q;
    char *big;
    long i;

    p = malloc(100);
    q = malloc(1 << 20);
    big = malloc(16 << 20);
    if (!p || !q || !big) {
        fail("malloc", 0);
        return;
    }
    strcpy(p, "small");
    memset(q, 0x5a, 1 << 20);
    big[0] = 1;
    big[(16 << 20) - 1] = 2;
    p = realloc(p, 5000);
    if (!p || strcmp(p, "small") != 0)
        fail("realloc", 0);
    for (i = 0; i < (1 << 20); i = i + 4096) {
        if (q[i] != 0x5a) {
            fail("malloc fill", i);
            break;
        }
    }
    if (big[0] != 1 || big[(16 << 20) - 1] != 2)
        fail("malloc big", 0);
    free(big);
    free(q);
    free(p);
}

static void test_printf(void)
{
    char buf[80];

    snprintf(buf, sizeof(buf), "%d %x %s %5.2f %lu", -42, 255, "s", 3.14159, 123456789012UL);
    if (strcmp(buf, "-42 ff s  3.14 123456789012") != 0)
        fail("snprintf", (long)strlen(buf));
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "child") == 0) {
        printf("child %s\n", argv[2]);
        return 3;
    }
    test_fork();
    test_pipe_exec();
    test_dirs();
    test_malloc();
    test_printf();
    if (fails) {
        printf("musl proc fail: total %d\n", fails);
        return 1;
    }
    printf("musl proc ok\n");
    return 0;
}
