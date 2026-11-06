/* libc pieces tcc mob needs that Kronos /lib/libc.c lacks. Linked into tcc7, tcc7b, tcc7c. */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* tccrun.c (-run only): Kronos passes no environment. */
char **environ;

/* tccpp.c hex float literals. Kronos ldexp is an int stub, so scale by
   powers of two in long double (libc __*tf* helpers); no float literals,
   Kronos strtold is a stub too. */
long double ldexpl(long double x, int exp)
{
    long double b;
    long double one;

    b = 2;
    one = 1;
    if (exp < 0) {
        b = one / b;
        exp = -exp;
    }
    while (exp) {
        if (exp & 1)
            x = x * b;
        b = b * b;
        exp = exp >> 1;
    }
    return x;
}

/* tccpp.c decimal long double literals; see compat-include/stdlib.h.
   libc.c returns strtold() as two unsigned longs in a0:a1, so call it
   with that type and keep the value in memory. */
struct compat_f128 {
    unsigned long lo;
    unsigned long hi;
};
typedef struct compat_f128 (*compat_strtold_fn)(const char *, char **);
static long double compat_ld;

long double *compat_strtold(const char *s, char **end)
{
    struct compat_f128 r;

    r = ((compat_strtold_fn)strtold)(s, end);
    memcpy(&compat_ld, &r, sizeof r);
    return &compat_ld;
}

/* libtcc.c #pragma once compare: Kronos has no symlinks, a path names itself. */
char *realpath(const char *path, char *buf)
{
    if (!buf)
        buf = malloc(strlen(path) + 1);
    if (buf)
        strcpy(buf, path);
    return buf;
}
