/* A pointer constant with bit 31 set and the upper word 0: mob loaded it
   with lui (+ addiw), which sign-extends, so (char *)0x80200000 became
   0xffffffff80200000. 0x80200000 is Kronos's KFS image (magic "KFS1"),
   which Kronos maps 1:1 and user-accessible. The pointer values are checked
   first: a wrong address would fault in the loads below. A 64-bit constant
   is built from rounded chunks; their carries were lost for some values
   (0x12347b57ff9b came out as 0x12347b47ff9b). */
#include <stdio.h>

int main(void)
{
    char *volatile p = (char *)0x80200000;
    void (*volatile fp)(void) = (void (*)(void))0x80200004;
    unsigned int *volatile q = (unsigned int *)0xfffff000;
    char *volatile r = (char *)0x87654fff;
    volatile unsigned long w = 0x12347b57ff9bUL;
    volatile unsigned long hi = 0x12347b, lo = 0x57ff9b;
    unsigned int v;

    if ((unsigned long)p != 0x80200000UL
        || (unsigned long)fp != 0x80200004UL
        || (unsigned long)q != 0xfffff000UL
        || (unsigned long)r != 0x87654fffUL) {
        puts("luiext fail ptr");
        return 1;
    }
    if (w != (hi << 24 | lo)) {
        puts("luiext fail 64");
        return 4;
    }
    v = *(volatile unsigned int *)0x80200000;
    if (v != 0x3153464bU) {
        puts("luiext fail load");
        return 2;
    }
    *(volatile unsigned int *)0x80200000 = v;
    if (*(volatile unsigned char *)0x80200003 != '1') {
        puts("luiext fail store");
        return 3;
    }
    puts("luiext ok");
    return 0;
}
