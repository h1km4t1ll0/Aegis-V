/* /initrd/bin/init for Tilck (plan 2, phase 2). Built by tcc7b on Kronos
   with Kronos libc.c; Tilck gives init no open files, so fds 0-2 come from
   /dev/console. Prints "init ok" and execs the process tests; on failure
   it exits, and Tilck panics "Init exited with code: 9x". */

long write(int fd, const void *buf, unsigned long n);
int open(const char *path, int flags, int mode);
void exit(int n);

/* a0 = number, a1..a6 = arguments (as compat-crt.s ecall6) */
long init_sys(long n, long a, long b, long c, long d, long e, long f);
__asm__(
    ".global init_sys\n"
    "init_sys:\n"
    "\tmv a7, a0\n"
    "\tmv a0, a1\n"
    "\tmv a1, a2\n"
    "\tmv a2, a3\n"
    "\tmv a3, a4\n"
    "\tmv a4, a5\n"
    "\tmv a5, a6\n"
    "\tecall\n"
    "\tjalr zero, 0(ra)\n");

#define SYS_execve 221
#define O_RDONLY 0
#define O_WRONLY 1

static void say(const char *s)
{
    unsigned long n;

    n = 0;
    while (s[n])
        n = n + 1;
    write(1, s, n);
}

int main(int argc, char **argv)
{
    char *av[2];
    char *ev[1];

    if (open("/dev/console", O_RDONLY, 0) != 0
        || open("/dev/console", O_WRONLY, 0) != 1
        || open("/dev/console", O_WRONLY, 0) != 2) {
        say("init fail: console\n");
        exit(90); /* Tilck panics "Init exited with code: 90" */
    }
    say("init ok\n");
    av[0] = "/initrd/bin/proc";
    av[1] = 0;
    ev[0] = 0;
    init_sys(SYS_execve, (long)av[0], (long)av, (long)ev, 0, 0, 0);
    say("init fail: execve /initrd/bin/proc\n");
    exit(91);
    return 1;
}
