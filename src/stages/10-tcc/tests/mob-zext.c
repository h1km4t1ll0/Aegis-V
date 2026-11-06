/* riscv64 32->64 zero-extension (tcc mob riscv64-gen.c:320, :1046 relied on it).
 * Expected values are built with 64-bit shifts only, never from a 32-bit
 * unsigned literal, so a broken compiler cannot fake them. */
#include <stdio.h>

static unsigned long lit(void)
{
    return 0xffffffffUL;
}

static unsigned long mk(unsigned a)
{
    return a;
}

static int eq(int a, unsigned long b)
{
    return a == b;
}

static unsigned long mask(unsigned long ul)
{
    return ul & 0xFF000000;
}

int main(void)
{
    unsigned long low32;
    unsigned long top8;
    unsigned u;
    unsigned long v;
    int bad;

    low32 = 1;
    low32 = (low32 << 32) - 1;
    top8 = 255;
    top8 = top8 << 24;
    u = 0u - 1u;
    v = u;
    bad = 0;
    if (lit() != low32)
        bad = bad | 1;
    if (mk(0u - 1u) != low32)
        bad = bad | 2;
    if (eq(-1, low32) != 0)
        bad = bad | 4;
    if (v != low32)
        bad = bad | 8;
    if (mask(~0UL) != top8)
        bad = bad | 16;
    if ((unsigned long)0xffffffffu != low32)
        bad = bad | 32;
    if (bad)
        printf("zext fail %d lit=%lx mk=%lx\n", bad, lit(), mk(0u - 1u));
    else
        puts("zext ok");
    return bad;
}
