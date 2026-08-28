#include "k.h"

static unsigned char *fs_base;
static unsigned int fs_nent;

struct fdent {
    unsigned char *data;
    unsigned long size;
    unsigned long cap;
    unsigned long pos;
    int used;
    int writable;
};

struct extra {
    char path[256];
    unsigned char *data;
    unsigned long size;
    unsigned long cap;
    int used;
    int isdir;
};

static struct fdent fds[32];
static struct extra extras[16];
static char pathbuf[256];

static unsigned int u16le(unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static unsigned int u32le(unsigned char *p)
{
    return (unsigned int)p[0]
        | ((unsigned int)p[1] << 8)
        | ((unsigned int)p[2] << 16)
        | ((unsigned int)p[3] << 24);
}

static int path_eq(const char *a, const char *b)
{
    unsigned long i;
    i = 0;
    while (a[i] && a[i] == b[i])
        i = i + 1;
    return a[i] == 0 && b[i] == 0;
}

static const char *norm_path(const char *path)
{
    const char *p;
    const char *cwd;
    unsigned long i;
    unsigned long j;
    if (!path)
        path = "";
    p = path;
    if (p[0] == '.' && p[1] == '/')
        p = p + 2;
    i = 0;
    if (p[0] != '/') {
        cwd = proc_cwd();
        j = 0;
        while (cwd[j] && i < 255) {
            pathbuf[i] = cwd[j];
            i = i + 1;
            j = j + 1;
        }
        if (i > 0 && pathbuf[i - 1] != '/' && i < 255) {
            pathbuf[i] = '/';
            i = i + 1;
        }
    }
    j = 0;
    while (p[j] && i < 255) {
        pathbuf[i] = p[j];
        i = i + 1;
        j = j + 1;
    }
    pathbuf[i] = 0;
    return pathbuf;
}

static struct extra *extra_find(const char *path)
{
    int i;
    i = 0;
    while (i < 16) {
        if (extras[i].used && path_eq(extras[i].path, path))
            return &extras[i];
        i = i + 1;
    }
    return 0;
}

void ramfs_init(void *blob)
{
    unsigned char *p;
    int i;
    fs_base = 0;
    fs_nent = 0;
    i = 0;
    while (i < 32) {
        fds[i].used = 0;
        fds[i].writable = 0;
        i = i + 1;
    }
    i = 0;
    while (i < 16) {
        extras[i].used = 0;
        extras[i].isdir = 0;
        extras[i].data = 0;
        extras[i].size = 0;
        extras[i].cap = 0;
        extras[i].path[0] = 0;
        i = i + 1;
    }
    if (!blob)
        return;
    p = (unsigned char *)blob;
    if (p[0] != 'K' || p[1] != 'F' || p[2] != 'S' || p[3] != '1')
        return;
    fs_base = p;
    fs_nent = u32le(p + 4);
}

void *kfs_get(const char *path, unsigned long *len)
{
    unsigned char *p;
    unsigned char *pb;
    unsigned int i;
    unsigned int plen;
    unsigned int dlen;
    unsigned int j;
    unsigned long slen;
    struct extra *e;
    e = extra_find(path);
    if (e) {
        if (e->isdir)
            return 0;
        if (len)
            *len = e->size;
        return e->data;
    }
    if (!fs_base)
        return 0;
    slen = strlen(path);
    p = fs_base + 8;
    i = 0;
    while (i < fs_nent) {
        plen = u16le(p);
        p = p + 2;
        pb = p;
        p = p + plen;
        dlen = u32le(p);
        p = p + 4;
        if (slen == plen) {
            j = 0;
            while (j < plen && pb[j] == (unsigned char)path[j])
                j = j + 1;
            if (j == plen) {
                if (len)
                    *len = dlen;
                return p;
            }
        }
        p = p + dlen;
        i = i + 1;
    }
    return 0;
}

static int alloc_fd(void)
{
    int i;
    i = 3;
    while (i < 32) {
        if (!fds[i].used)
            return i;
        i = i + 1;
    }
    return -1;
}

static struct extra *extra_create(const char *path, int isdir)
{
    int i;
    unsigned long cap;
    unsigned long n;
    i = 0;
    while (i < 16) {
        if (!extras[i].used)
            break;
        i = i + 1;
    }
    if (i >= 16)
        return 0;
    n = strlen(path);
    if (n > 255)
        return 0;
    memcpy(extras[i].path, path, n + 1);
    extras[i].isdir = isdir;
    extras[i].size = 0;
    extras[i].used = 1;
    if (isdir) {
        extras[i].data = 0;
        extras[i].cap = 0;
        return &extras[i];
    }
    cap = 4UL << 20;
    extras[i].data = (unsigned char *)kalloc(cap);
    extras[i].cap = cap;
    if (!extras[i].data) {
        extras[i].used = 0;
        return 0;
    }
    memset(extras[i].data, 0, 1);
    uart_puts("kfs+ ");
    uart_puts(extras[i].path);
    uart_puts("\n");
    return &extras[i];
}

int ramfs_open(const char *path, int flags)
{
    const char *np;
    void *data;
    unsigned long sz;
    int fd;
    int creat;
    int trunc;
    struct extra *e;
    np = norm_path(path);
    creat = flags & O_CREAT;
    trunc = flags & O_TRUNC;
    e = extra_find(np);
    if (e && e->isdir)
        return -EINVAL;
    data = kfs_get(np, &sz);
    if (!data) {
        if (!creat)
            return -ENOENT;
        e = extra_create(np, 0);
        if (!e)
            return -ENOMEM;
        data = e->data;
        sz = 0;
    } else if (trunc) {
        if (e) {
            e->size = 0;
            sz = 0;
            data = e->data;
        } else {
            e = extra_create(np, 0);
            if (!e)
                return -ENOMEM;
            data = e->data;
            sz = 0;
        }
    }
    fd = alloc_fd();
    if (fd < 0)
        return -ENOMEM;
    fds[fd].data = (unsigned char *)data;
    fds[fd].size = sz;
    fds[fd].cap = e ? e->cap : sz;
    fds[fd].pos = (flags & O_APPEND) ? sz : 0;
    fds[fd].used = 1;
    fds[fd].writable = (flags & (O_WRONLY | O_RDWR | O_CREAT | O_TRUNC | O_APPEND)) ? 1 : 0;
    return fd;
}

long ramfs_read(int fd, void *buf, unsigned long n)
{
    unsigned long left;
    if (fd < 3 || fd >= 32 || !fds[fd].used)
        return -EBADF;
    if (fds[fd].pos >= fds[fd].size)
        return 0;
    left = fds[fd].size - fds[fd].pos;
    if (n > left)
        n = left;
    memcpy(buf, fds[fd].data + fds[fd].pos, n);
    fds[fd].pos = fds[fd].pos + n;
    return (long)n;
}

long ramfs_write(int fd, const void *buf, unsigned long n)
{
    unsigned long left;
    int i;
    if (fd < 3 || fd >= 32 || !fds[fd].used)
        return -EBADF;
    if (!fds[fd].writable)
        return -EBADF;
    if (fds[fd].pos > fds[fd].cap)
        return -ENOMEM;
    left = fds[fd].cap - fds[fd].pos;
    if (n > left)
        n = left;
    if (n == 0)
        return -ENOMEM;
    memcpy(fds[fd].data + fds[fd].pos, buf, n);
    fds[fd].pos = fds[fd].pos + n;
    if (fds[fd].pos > fds[fd].size)
        fds[fd].size = fds[fd].pos;
    i = 0;
    while (i < 16) {
        if (extras[i].used && extras[i].data == fds[fd].data)
            extras[i].size = fds[fd].size;
        i = i + 1;
    }
    return (long)n;
}

long ramfs_lseek(int fd, long off, int whence)
{
    long np;
    if (fd < 3 || fd >= 32 || !fds[fd].used)
        return -EBADF;
    if (whence == 0)
        np = off;
    else if (whence == 1)
        np = (long)fds[fd].pos + off;
    else if (whence == 2)
        np = (long)fds[fd].size + off;
    else
        return -EINVAL;
    if (np < 0)
        return -EINVAL;
    fds[fd].pos = (unsigned long)np;
    return np;
}

int ramfs_close(int fd)
{
    int i;
    if (fd < 3 || fd >= 32 || !fds[fd].used)
        return -EBADF;
    i = 0;
    while (i < 16) {
        if (extras[i].used && extras[i].data == fds[fd].data)
            extras[i].size = fds[fd].size;
        i = i + 1;
    }
    fds[fd].used = 0;
    return 0;
}

int ramfs_stat_size(const char *path, unsigned long *sz)
{
    void *p;
    unsigned long n;
    p = kfs_get(norm_path(path), &n);
    if (!p)
        return -ENOENT;
    if (sz)
        *sz = n;
    return 0;
}

int ramfs_mkdir(const char *path)
{
    const char *np;
    struct extra *e;
    unsigned long n;
    np = norm_path(path);
    if (kfs_get(np, &n))
        return -EEXIST;
    e = extra_find(np);
    if (e)
        return 0;
    e = extra_create(np, 1);
    if (!e)
        return -ENOMEM;
    return 0;
}

int ramfs_exists(const char *path)
{
    unsigned long n;
    if (ramfs_stat_size(path, &n) == 0)
        return 1;
    return ramfs_isdir(path);
}

int ramfs_isdir(const char *path)
{
    const char *np;
    struct extra *e;
    unsigned char *p;
    unsigned int i;
    unsigned int plen;
    unsigned int dlen;
    unsigned long nlen;
    np = norm_path(path);
    e = extra_find(np);
    if (e)
        return e->isdir;
    nlen = strlen(np);
    if (!fs_base)
        return 0;
    p = fs_base + 8;
    i = 0;
    while (i < fs_nent) {
        plen = u16le(p);
        p = p + 2;
        if (plen > nlen && p[nlen] == '/' ) {
            unsigned int j;
            j = 0;
            while (j < nlen && p[j] == (unsigned char)np[j])
                j = j + 1;
            if (j == nlen)
                return 1;
        }
        p = p + plen;
        dlen = u32le(p);
        p = p + 4 + dlen;
        i = i + 1;
    }
    return 0;
}

int ramfs_fd_size(int fd, unsigned long *sz)
{
    if (fd < 3 || fd >= 32 || !fds[fd].used)
        return -EBADF;
    if (sz)
        *sz = fds[fd].size;
    return 0;
}
