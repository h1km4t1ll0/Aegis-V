/* Decimal float literals (the compiler's strtod/strtold) and run-time
 * unsigned long <-> double/float.  Expected bit patterns are built with
 * shifts only, never from a float literal or a hex literal >= 0x80000000.
 * Built by tcc6 with -DFLOATLIT6 and by tcc7b.  tcc6 skips the long
 * double checks: it keeps only a0 of a long double call result (07-tcc
 * tccgen.c sets no r2 for it), so its own strtold() literals lose the
 * high word. */
#include <stdio.h>
#include <stdlib.h>

#ifdef FLOATLIT6
#define TAG "floatlit6"
#else
#define TAG "floatlit"
#endif

union uld {
    long double v;
    unsigned long w[2];
};

union ud {
    double v;
    unsigned long w;
};

union uf {
    float v;
    unsigned long w;
};

static int bad;

static void check(int n, int ok)
{
    if (!ok && !bad)
        bad = n;
}

/* high word of 2^e2 as binary128 (low word is 0) */
static unsigned long ld_hi(int e2)
{
    unsigned long e;
    e = 16383 + e2;
    return e << 48;
}

/* double bits of an integer n, 0 < n < 2^53 */
static unsigned long dbits(unsigned long n)
{
    unsigned long one;
    unsigned long e;
    int p;
    one = 1;
    p = 0;
    while (n >> (p + 1))
        p = p + 1;
    e = 1023 + p;
    return e << 52 | (n ^ one << p) << (52 - p);
}

/* float bits of an integer n, 0 < n < 2^24 */
static unsigned long fbits(unsigned long n)
{
    unsigned long one;
    unsigned long e;
    int p;
    one = 1;
    p = 0;
    while (n >> (p + 1))
        p = p + 1;
    e = 127 + p;
    return e << 23 | (n ^ one << p) << (23 - p);
}

static unsigned long dw(double v)
{
    union ud u;
    u.v = v;
    return u.w;
}

static unsigned long fw(float v)
{
    union uf u;
    u.w = 0;
    u.v = v;
    return u.w;
}

static double to_d(unsigned long x)
{
    return (double)x;
}

static float to_f(unsigned long x)
{
    return (float)x;
}

static unsigned long from_d(double d)
{
    return (unsigned long)d;
}

static unsigned long from_f(float f)
{
    return (unsigned long)f;
}

int main(void)
{
    union uld u;
    unsigned long one;
    unsigned long e;
    unsigned long big;
    unsigned long fbig;

    one = 1;

#ifndef FLOATLIT6
    u.v = 79228162514264337593543950336.0L;
    check(1, u.w[0] == 0 && u.w[1] == ld_hi(96));
    u.v = 18446744073709551616.0L;
    check(2, u.w[0] == 0 && u.w[1] == ld_hi(64));
    u.v = 4294967296.0L;
    check(3, u.w[0] == 0 && u.w[1] == ld_hi(32));
#endif

    e = 1023;
    check(4, dw(1.5) == (e << 52 | one << 51));
    e = 1022;
    check(5, dw(0.5) == e << 52);
    e = 100000;
    check(6, dw(1e10) == dbits(e * e));
    check(7, dw(123456789.0) == dbits(123456789));

    /* 2^63 + 2^11 is exact in double */
    big = one << 63 | one << 11;
    e = 1086;
    check(8, dw(to_d(big)) == (e << 52 | 1));
    check(9, from_d(to_d(big)) == big);
    check(10, dw(to_d(12345)) == dbits(12345));
    check(11, from_d(to_d(12345)) == 12345);
    /* round to nearest: above half rounds up, a tie goes to even */
    check(12, dw(to_d(one << 63 | one << 10 | 1)) == (e << 52 | 1));
    check(13, dw(to_d(one << 63 | one << 10)) == e << 52);

    /* 2^63 + 2^40 is exact in float */
    fbig = one << 63 | one << 40;
    e = 190;
    check(14, fw(to_f(fbig)) == (e << 23 | 1));
    check(15, from_f(to_f(fbig)) == fbig);
    check(16, fw(to_f(12345)) == fbits(12345));
    check(17, from_f(to_f(12345)) == 12345);

    /* the program's own libc at run time */
    check(18, dw(strtod("123456789.0", 0)) == dbits(123456789));
#ifndef FLOATLIT6
    u.v = strtold("79228162514264337593543950336", 0);
    check(19, u.w[0] == 0 && u.w[1] == ld_hi(96));
#endif
    check(20, dw(strtod("-0.5e-0", 0)) == (one << 63 | dw(0.5)));

    if (bad) {
        printf(TAG " fail %d\n", bad);
        return 1;
    }
    puts(TAG " ok");
    return 0;
}
