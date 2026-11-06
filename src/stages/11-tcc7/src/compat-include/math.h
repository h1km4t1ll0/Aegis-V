/* tccpp.c calls ldexpl(); Kronos <math.h> lacks it. Defined in compat.c. */
#ifndef _COMPAT_MATH_H
#define _COMPAT_MATH_H
#include_next <math.h>
long double ldexpl(long double x, int exp);
#endif
