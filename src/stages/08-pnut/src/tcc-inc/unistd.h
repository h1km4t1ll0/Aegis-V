#ifndef _UNISTD_H
#define _UNISTD_H

#include <stddef.h>

typedef long off_t;
typedef int pid_t;
typedef int ssize_t;

int close(int fd);
ssize_t read(int fd, void *buf, size_t n);
ssize_t write(int fd, const void *buf, size_t n);
off_t lseek(int fd, off_t off, int whence);
int unlink(const char *path);
int chdir(const char *path);
char *getcwd(char *buf, size_t size);
int isatty(int fd);
int dup(int fd);
int dup2(int a, int b);
void *sbrk(ptrdiff_t inc);
int pipe(int *p);
int fork(void);
int execv(const char *file, char **argv);
int execvp(const char *file, char **argv);
int getpid(void);

#endif
