/* src/complex of musl-2 (tcc could not build it: no _Complex) */
#include <complex.h>
#include <stdio.h>

int main(void)
{
    volatile double re = -4.0;
    volatile double pi = 3.141592653589793;
    double complex z = csqrt(re + 0.0 * I);
    double complex e = cexp(I * pi);

    if (creal(z) == 0.0 && cimag(z) == 2.0 && cabs(e + 1.0) < 1e-12) {
        printf("musl2 complex ok\n");
        return 0;
    }
    printf("musl2 complex fail %g %g %g\n", creal(z), cimag(z), cabs(e + 1.0));
    return 1;
}
