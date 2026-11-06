#include <stdio.h>

/* GNU ld's __bss_start, Linux-style __bss_stop (Tilck riscv start.S) */
extern char __bss_start[];
extern char __bss_stop[];
extern char _end[];

static char buf[64];

int main(void)
{
    buf[0] = 1;
    if (!(__bss_start <= buf && buf + sizeof(buf) <= __bss_stop))
        return 1;
    if (__bss_stop > _end || __bss_start >= __bss_stop)
        return 2;
    puts("bssym ok");
    return 0;
}
