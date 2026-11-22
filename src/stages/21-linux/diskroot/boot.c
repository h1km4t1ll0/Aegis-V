#define _GNU_SOURCE
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/reboot.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void die(const char *what)
{
    perror(what);
    puts("linux init fail: diskroot bootstrap");
    fflush(stdout);
    sync();
    reboot(RB_POWER_OFF);
    for (;;) pause();
}

static void require_line(const char *path, const char *expected)
{
    char line[128];
    FILE *f = fopen(path, "r");
    if (!f || !fgets(line, sizeof(line), f)) die(path);
    fclose(f);
    line[strcspn(line, "\r\n")] = 0;
    if (strcmp(line, expected)) die("disk identity mismatch");
}

int main(void)
{
    const char *token = getenv("AEGIS_ROOT");
    char owner[96];
    if (getpid() != 1 || !token || strlen(token) != 20 ||
        strncmp(token, "aegis-", 6) || strspn(token + 6, "0123456789abcdef") != 14)
        die("root ownership token");
    if (mount("devtmpfs", "/dev", "devtmpfs", 0, 0) ||
        mount("proc", "/proc", "proc", 0, 0) ||
        mount("sysfs", "/sys", "sysfs", 0, 0)) die("bootstrap mounts");
    require_line("/sys/class/block/vda/serial", token);
    require_line("/sys/class/block/vda/size", "8388608");
    if (mount("/dev/vda", "/newroot", "ext4", MS_RELATIME, 0)) die("mount ext4 root");
    snprintf(owner, sizeof(owner), "aegis-disk-owner-v1 %s", token);
    require_line("/newroot/.aegis-disk-owner", owner);
    if (access("/newroot/sbin/init", X_OK)) die("disk init");
    if (mount("/dev", "/newroot/dev", 0, MS_MOVE, 0) ||
        mount("/proc", "/newroot/proc", 0, MS_MOVE, 0) ||
        mount("/sys", "/newroot/sys", 0, MS_MOVE, 0)) die("move runtime mounts");
    if (chdir("/newroot") || mount(".", "/", 0, MS_MOVE, 0) ||
        chroot(".") || chdir("/")) die("switch to ext4 root");
    if (sethostname("aegis", 5)) die("hostname");
    setenv("PATH", "/usr/local/bin:/usr/bin:/usr/sbin:/bin:/sbin", 1);
    setenv("HOME", "/root", 1);
    setenv("USER", "root", 1);
    setenv("LOGNAME", "root", 1);
    setenv("SHELL", "/bin/bash", 1);
    puts("diskroot switched to ext4; exec disk /sbin/init");
    fflush(stdout);
    execl("/sbin/init", "init", (char *)0);
    die("exec disk init");
}
