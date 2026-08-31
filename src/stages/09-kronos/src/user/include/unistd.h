#ifndef _UNISTD_H
#define _UNISTD_H

#include <stddef.h>
#include <sys/types.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

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
int brk(void *addr);
int pipe(int *p);
int fork(void);
int execv(const char *file, char **argv);
int execvp(const char *file, char **argv);
int getpid(void);
void _exit(int n);
int access(const char *path, int mode);

#endif
