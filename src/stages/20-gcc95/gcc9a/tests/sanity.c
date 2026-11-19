/* gcc9a at -O2 with its default -std=gnu11 (20-gcc95 README, rung gcc9a):
   rung gcc1's checks (long long, double, switch, recursion, struct by
   value) plus C11: __STDC_VERSION__, _Static_assert, _Generic, an
   anonymous struct and union, _Alignof/_Alignas, a compound literal and a
   designated initializer. volatile inputs keep the work at run time.
   Prints `gcc9a sanity c ok`, or `gcc9a sanity FAIL WHAT` per failed check. */
#include <stdio.h>
#include <stddef.h>

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
#error "not C11"
#endif

_Static_assert(sizeof(long long) == 8, "long long is 64 bits");
_Static_assert(sizeof(void *) == 8, "lp64");

#define KIND(x) _Generic((x), int: 1, long: 2, double: 3, float: 4, char *: 5, default: 0)

struct shape {
    int kind;
    union {
        struct {
            int w, h;
        };
        struct {
            long r;
        };
    };
};

static long area(struct shape s)
{
    return s.kind == 0 ? (long)s.w * s.h : 3 * s.r * s.r;
}

static _Alignas(64) char aligned[3];

struct pt {
    long long x;
    double y;
    char tag[8];
};

static int fails;

static void check(int ok, const char *what)
{
    if (!ok) {
        printf("gcc9a sanity FAIL %s\n", what);
        fails++;
    }
}

static struct pt scale(struct pt p, int k)
{
    p.x *= k;
    p.y *= k;
    p.tag[0] = (char)('A' + k);
    return p;
}

static long long fib(int n)
{
    return n < 2 ? n : fib(n - 1) + fib(n - 2);
}

static unsigned long long gcd(unsigned long long a, unsigned long long b)
{
    return b ? gcd(b, a % b) : a;
}

static int sw(int c)
{
    switch (c) {
    case 0: return 3;
    case 1: return 5;
    case 2: return 7;
    case 3: return 11;
    case 4: return 13;
    case 5: return 17;
    case 6: return 19;
    case 9: return 23;
    case 100: return 29;
    case -7: return 31;
    default: return -1;
    }
}

static double poly(const double *c, int n, double x)
{
    double r = 0;
    int i;

    for (i = n - 1; i >= 0; i--)
        r = r * x + c[i];
    return r;
}

int main(void)
{
    volatile int seven = 7, n = 30, k = 3;
    volatile long long a = 0x123456789LL, b = -987654321LL;
    volatile double x = 1.5;
    static const double c[4] = { 1.0, -2.0, 0.5, 0.25 };
    struct pt p = { 40000000000LL, 2.25, "pt" }, q;
    long long s;
    int i;

    check(a * b == -4826388488949218745LL, "mul");
    check(a / seven == 698102620LL, "div");
    check(b % seven == -3, "mod");
    check(((unsigned long long)b >> 7) == 144115188068139822ULL, "shift");
    check(fib(n) == 832040, "fib");
    check(gcd(1071ULL * 1000003, 462ULL * 1000003) == 21ULL * 1000003, "gcd");
    s = 0;
    for (i = -10; i <= 110; i++)
        s += sw(i) * (i + 1);
    check(s == -2687, "switch");
    check(poly(c, 4, x) == -0.03125, "poly");
    check((long long)(x * 1e10) == 15000000000LL, "d2ll");
    check((double)a == 4886718345.0, "ll2d");
    q = scale(p, k);
    check(q.x == 120000000000LL && q.y == 6.75 && q.tag[0] == 'D'
          && q.tag[1] == 't' && p.tag[0] == 'p', "struct");
    check(KIND(seven + 0) == 1 && KIND(10L) == 2 && KIND(x + 0) == 3
          && KIND(1.0f) == 4 && KIND((char *)0) == 5 && KIND('c' + 0) == 1, "generic");
    check(area((struct shape){ .kind = 0, .w = seven, .h = k }) == 21
          && area((struct shape){ .kind = 1, .r = n }) == 2700, "anonymous");
    check(offsetof(struct shape, w) == offsetof(struct shape, r), "union");
    check(_Alignof(long long) == 8 && ((size_t)aligned & 63) == 0, "align");
    if (!fails)
        printf("gcc9a sanity c ok\n");
    return fails;
}
