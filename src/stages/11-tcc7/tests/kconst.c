#include <stdio.h>

int main(void)
{
    unsigned long v;
    __asm__ volatile ("addi %0, zero, %1" : "=r" (v) : "K" (17));
    if (v != 17)
        return 1;
    puts("kconst ok");
    return 0;
}
