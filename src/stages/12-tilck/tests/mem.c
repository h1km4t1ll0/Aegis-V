/* Memory test under Tilck (plan 2, phase 4), a musl program: one anonymous
   mmap() of 3 GB, a write to every page, a check of every page, munmap().
   Prints "mem 3072M ok", or "mem fail: <what> <value>" and exits 1. */
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>

#define MEM_MB 3072UL
#define PAGE 4096UL

int main(void)
{
    unsigned long len = MEM_MB << 20;
    unsigned long i;
    unsigned long *p;
    unsigned long *w;

    p = mmap(0, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        printf("mem fail: mmap %d\n", errno);
        return 1;
    }
    /* the first word of each page holds the page's own index, mixed */
    for (i = 0; i < len / PAGE; i++) {
        w = p + i * (PAGE / sizeof(*p));
        *w = i ^ 0x5a5a5a5a5a5a5a5aUL;
    }
    for (i = 0; i < len / PAGE; i++) {
        w = p + i * (PAGE / sizeof(*p));
        if (*w != (i ^ 0x5a5a5a5a5a5a5a5aUL)) {
            printf("mem fail: page %lu\n", i);
            return 1;
        }
    }
    if (munmap(p, len) != 0) {
        printf("mem fail: munmap %d\n", errno);
        return 1;
    }
    printf("mem %luM ok\n", MEM_MB);
    return 0;
}
