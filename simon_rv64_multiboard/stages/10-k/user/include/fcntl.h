#ifndef _FCNTL_H
#define _FCNTL_H

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 64
#define O_TRUNC 512
#define O_APPEND 1024
#define O_BINARY 0
#define F_GETFL 3
#define F_SETFL 4
#define AT_FDCWD (-100)

int open(const char *path, int flags, ...);
int fcntl(int fd, int cmd, ...);
int openat(int dirfd, const char *path, int flags, ...);

#endif
