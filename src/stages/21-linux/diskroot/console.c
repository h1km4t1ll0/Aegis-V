#define _GNU_SOURCE
#include <sys/ioctl.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

int main(void)
{
    int fd;
    if (setsid() < 0) { perror("setsid"); return 1; }
    fd = open("/dev/ttyS0", O_RDWR);
    if (fd < 0 || ioctl(fd, TIOCSCTTY, 1) || tcsetpgrp(fd, getpgrp())) {
        perror("serial controlling terminal"); return 1;
    }
    for (int n = 0; n < 3; n++) if (dup2(fd, n) < 0) return 1;
    if (fd > 2) close(fd);
    if (chdir("/root")) return 1;
    puts("diskroot Bash console ready");
    fflush(stdout);
    execl("/bin/bash", "bash", "--login", "-i", (char *)0);
    perror("exec Bash");
    return 1;
}
