/* store() to a constant address: mob put the address in s0, the frame
   pointer, so the locals read after it and the epilogue's sp/ra were wrong.
   0x00F00000 is user VA that Kronos maps for every program, far above this
   test's image; 0x100000000 (the 64-bit form of the address) is DRAM that
   Kronos maps 1:1, inside the unused top of the user region, as is user VA
   0x7ffff800 (lui of its rounded upper part, 0x80000, sign-extends). */
#include <stdio.h>

static long st(long a)
{
    volatile long loc[2];

    loc[0] = a;
    loc[1] = a + 1;
    *(volatile char *)0x00F00000 = 7;
    *(volatile short *)0x00F00002 = 300;
    *(volatile int *)0x00F00004 = (int)a;
    *(volatile long *)0x00F00008 = a * 3;
    *(volatile double *)0x00F00010 = 2.5;
    *(volatile long *)0x100000000 = a + 5;
    *(volatile int *)0x7ffff800 = (int)a + 6;
    return loc[0] + loc[1];
}

int main(void)
{
    if (st(40) != 81) {
        puts("cstore fail locals");
        return 1;
    }
    if (*(volatile char *)0x00F00000 != 7
        || *(volatile short *)0x00F00002 != 300
        || *(volatile int *)0x00F00004 != 40
        || *(volatile long *)0x00F00008 != 120
        || *(volatile double *)0x00F00010 != 2.5
        || *(volatile long *)0x100000000 != 45
        || *(volatile int *)0x7ffff800 != 46) {
        puts("cstore fail values");
        return 2;
    }
    puts("cstore ok");
    return 0;
}
