/* First Linux PID 1, compiled by the guest GCC and statically linked with musl. */
#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void fail(const char *s)
{
    perror(s);
    puts("linux init fail");
    fflush(stdout);
    reboot(RB_POWER_OFF);
    for (;;) pause();
}
int main(void)
{
    int fd, p[2], status;
    char buf[16];
    struct utsname u;
    pid_t child;
    if (getpid() != 1 || uname(&u)) fail("pid/uname");
    printf("linux uname %s %s %s\n", u.sysname, u.release, u.machine);
    if (strcmp(u.sysname, "Linux") || strncmp(u.release, "6.6", 3)) fail("kernel");
    if (pipe(p)) fail("pipe");
    child = fork();
    if (child < 0) fail("fork");
    if (!child) {
        close(p[0]);
        _exit(write(p[1], "guest gcc9", 10) == 10 ? 0 : 1);
    }
    close(p[1]);
    if (read(p[0], buf, sizeof(buf)) != 10 || memcmp(buf, "guest gcc9", 10)) fail("pipe read");
    close(p[0]);
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status)) fail("wait");
    puts("linux fork pipe ok");
    mkdir("/tmp", 0755);
    fd = open("/tmp/probe", O_CREAT | O_TRUNC | O_RDWR, 0600);
    if (fd < 0 || write(fd, "linux", 5) != 5 || lseek(fd, 0, SEEK_SET) != 0 ||
        read(fd, buf, sizeof(buf)) != 5 || memcmp(buf, "linux", 5)) fail("file IO");
    close(fd);
    puts("linux file io ok");
    child = fork();
    if (child < 0) fail("fork shell");
    if (!child) {
        execl("/bin/sh", "sh", "/smoke.sh", (char *)0);
        _exit(127);
    }
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status)) fail("shell smoke");
    puts("linux init ok");
    fflush(stdout);
    sync();
    if (reboot(RB_POWER_OFF)) fail("poweroff");
    for (;;) pause();
}
