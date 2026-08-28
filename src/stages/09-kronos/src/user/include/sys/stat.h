#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <sys/types.h>

#define S_IFMT  0170000
#define S_IFDIR 0040000
#define S_IFREG 0100000
#define S_IRWXU 0700
#define S_IRWXG 0070
#define S_IRWXO 0007

struct stat {
    unsigned long st_dev;
    unsigned long st_ino;
    unsigned int st_mode;
    unsigned int st_nlink;
    unsigned int st_uid;
    unsigned int st_gid;
    unsigned int __pad0;
    unsigned long st_rdev;
    unsigned long st_size;
    int st_blksize;
    int __pad1;
    long st_blocks;
    long st_atime;
    unsigned long st_atime_nsec;
    long st_mtime;
    unsigned long st_mtime_nsec;
    long st_ctime;
    unsigned long st_ctime_nsec;
    unsigned int __unused[2];
};

int fstat(int fd, struct stat *st);
int stat(const char *path, struct stat *st);
int mkdir(const char *path, mode_t mode);

#endif
