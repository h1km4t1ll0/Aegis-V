#include "k.h"

static unsigned long tick;

static void print_dec(long n)
{
    char buf[24];
    int i;
    unsigned long v;
    i = 0;
    if (n < 0) {
        uart_putc('-');
        v = (unsigned long)(0 - n);
    } else {
        v = (unsigned long)n;
    }
    if (v == 0) {
        uart_putc('0');
        return;
    }
    while (v != 0) {
        buf[i] = (char)('0' + (v % 10));
        i = i + 1;
        v = v / 10;
    }
    while (i > 0) {
        i = i - 1;
        uart_putc(buf[i]);
    }
}

static void *uptr(unsigned long va)
{
    unsigned long pa;
    if (va == 0)
        return 0;
    pa = uva2pa(va);
    if (!pa)
        return 0;
    return (void *)pa;
}

static long sys_write(long fd, unsigned long buf, unsigned long n)
{
    unsigned char *p;
    unsigned long i;
    p = (unsigned char *)uptr(buf);
    if (!p)
        return -EFAULT;
    if (fd == 1 || fd == 2) {
        i = 0;
        while (i < n) {
            uart_putc(p[i]);
            i = i + 1;
        }
        return (long)n;
    }
    return ramfs_write((int)fd, p, n);
}

static long sys_read(long fd, unsigned long buf, unsigned long n)
{
    void *p;
    if (fd == 0)
        return 0;
    if (fd == 1 || fd == 2)
        return -EBADF;
    p = uptr(buf);
    if (!p)
        return -EFAULT;
    return ramfs_read((int)fd, p, n);
}

static long sys_getcwd(unsigned long buf, unsigned long size)
{
    const char *cwd;
    unsigned long n;
    char *p;
    cwd = proc_cwd();
    n = strlen(cwd) + 1;
    p = (char *)uptr(buf);
    if (!p || size < n)
        return -EINVAL;
    memcpy(p, cwd, n);
    return (long)n;
}

static void sys_exit(long code)
{
    uart_puts("exit ");
    print_dec(code);
    uart_puts("\n");
    jump_k();
}

static long sys_uname(unsigned long buf)
{
    char *p;
    p = (char *)uptr(buf);
    if (!p)
        return -EFAULT;
    memset(p, 0, 390);
    memcpy(p, "Linux", 6);
    memcpy(p + 65, "aegis", 6);
    memcpy(p + 130, "0", 2);
    memcpy(p + 195, "Kronos", 7);
    memcpy(p + 260, "riscv64", 8);
    return 0;
}

static void fill_stat(unsigned char *st, unsigned long sz, int dir)
{
    memset(st, 0, 128);
    /* st_mode at offset 16 */
    *(unsigned int *)(st + 16) = (unsigned int)(dir ? (S_IFDIR | 0755) : (S_IFREG | 0777));
    *(unsigned int *)(st + 20) = 1;
    *(unsigned long *)(st + 48) = sz;
    *(int *)(st + 56) = 4096;
}

static long sys_newfstatat(long dirfd, unsigned long path, unsigned long st, long flags)
{
    const char *np;
    unsigned long sz;
    unsigned char *out;
    int dir;
    (void)dirfd;
    (void)flags;
    np = (const char *)uptr(path);
    out = (unsigned char *)uptr(st);
    if (!np || !out)
        return -EFAULT;
    if (ramfs_stat_size(np, &sz) == 0) {
        fill_stat(out, sz, 0);
        return 0;
    }
    dir = ramfs_isdir(np);
    if (dir) {
        fill_stat(out, 0, 1);
        return 0;
    }
    return -ENOENT;
}

static long sys_fstat(long fd, unsigned long st)
{
    unsigned char *out;
    unsigned long sz;
    out = (unsigned char *)uptr(st);
    if (!out)
        return -EFAULT;
    if (fd == 0 || fd == 1 || fd == 2) {
        fill_stat(out, 0, 0);
        return 0;
    }
    if (ramfs_fd_size((int)fd, &sz) < 0)
        return -EBADF;
    fill_stat(out, sz, 0);
    return 0;
}

static long sys_gettimeofday(unsigned long tv, unsigned long tz)
{
    long *p;
    (void)tz;
    p = (long *)uptr(tv);
    if (!p)
        return -EFAULT;
    tick = tick + 1;
    p[0] = (long)tick;
    p[1] = 0;
    return 0;
}

static long sys_clock_gettime(long clk, unsigned long ts)
{
    long *p;
    (void)clk;
    p = (long *)uptr(ts);
    if (!p)
        return -EFAULT;
    tick = tick + 1;
    p[0] = (long)tick;
    p[1] = 0;
    return 0;
}

static long sys_fcntl(long fd, long cmd, long arg)
{
    (void)fd;
    (void)arg;
    if (cmd == F_GETFL)
        return 0;
    return 0;
}

static long sys_faccessat(long dirfd, unsigned long path, long mode, long flags)
{
    const char *np;
    unsigned long sz;
    (void)dirfd;
    (void)mode;
    (void)flags;
    np = (const char *)uptr(path);
    if (!np)
        return -EFAULT;
    if (ramfs_stat_size(np, &sz) == 0)
        return 0;
    if (ramfs_isdir(np))
        return 0;
    return -ENOENT;
}

static long sys_mkdirat(long dirfd, unsigned long path, long mode)
{
    const char *np;
    (void)dirfd;
    (void)mode;
    np = (const char *)uptr(path);
    if (!np)
        return -EFAULT;
    return ramfs_mkdir(np);
}

long syscall(long a7, long a0, long a1, long a2, long a3, long a4, long a5)
{
    if (a7 == SYS_write)
        return sys_write(a0, (unsigned long)a1, (unsigned long)a2);
    if (a7 == SYS_read)
        return sys_read(a0, (unsigned long)a1, (unsigned long)a2);
    if (a7 == SYS_openat)
        return ramfs_open((const char *)uptr((unsigned long)a1), (int)a2);
    if (a7 == SYS_close)
        return ramfs_close((int)a0);
    if (a7 == SYS_lseek)
        return ramfs_lseek((int)a0, a1, (int)a2);
    if (a7 == SYS_getcwd)
        return sys_getcwd((unsigned long)a0, (unsigned long)a1);
    if (a7 == SYS_exit)
        sys_exit(a0);
    if (a7 == SYS_ioctl)
        return -ENOTTY;
    if (a7 == SYS_brk)
        return kmem_brk((unsigned long)a0);
    if (a7 == SYS_mmap)
        return kmem_mmap((unsigned long)a1, (unsigned long)a3);
    if (a7 == SYS_munmap)
        return 0;
    if (a7 == SYS_mprotect)
        return 0;
    if (a7 == SYS_uname)
        return sys_uname((unsigned long)a0);
    if (a7 == SYS_gettimeofday)
        return sys_gettimeofday((unsigned long)a0, (unsigned long)a1);
    if (a7 == SYS_clock_gettime)
        return sys_clock_gettime(a0, (unsigned long)a1);
    if (a7 == SYS_fcntl)
        return sys_fcntl(a0, a1, a2);
    if (a7 == SYS_newfstatat)
        return sys_newfstatat(a0, (unsigned long)a1, (unsigned long)a2, a3);
    if (a7 == SYS_fstat)
        return sys_fstat(a0, (unsigned long)a1);
    if (a7 == SYS_faccessat)
        return sys_faccessat(a0, (unsigned long)a1, a2, a3);
    if (a7 == SYS_mkdirat)
        return sys_mkdirat(a0, (unsigned long)a1, a2);
    return -ENOSYS;
}
