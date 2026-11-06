/* tccrun.c includes <sys/mman.h>; Kronos /include has none. libc.c defines these. */
#ifndef _COMPAT_SYS_MMAN_H
#define _COMPAT_SYS_MMAN_H

#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_SHARED 1
#define MAP_PRIVATE 2
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED ((void *)-1)

void *mmap(void *addr, unsigned long len, int prot, int flags, int fd, long off);
int munmap(void *addr, unsigned long len);
int mprotect(void *addr, unsigned long len, int prot);

#endif
