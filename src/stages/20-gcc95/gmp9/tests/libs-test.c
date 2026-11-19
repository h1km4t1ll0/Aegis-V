/* Rung gmp9 (20-gcc95 README): the installed gmp.h, mpfr.h, mpc.h and
   libgmp.a, libmpfr.a, libmpc.a from /usr, built by gcc 4.6.4, as GCC
   9.5's configure uses them; the versions GCC 9.5's
   contrib/download_prerequisites names. Prints "gmp mpfr mpc link ok" or
   what differs, exit 1. */
#include <stdio.h>
#include <string.h>
#include <gmp.h>
#include <mpfr.h>
#include <mpc.h>

int main(void)
{
    mpz_t z;
    mpfr_t x;
    mpc_t c;
    mpfr_exp_t e;
    char *s;
    int bad = 0;

    mpz_init(z);
    mpz_ui_pow_ui(z, 2, 100);
    s = mpz_get_str(NULL, 10, z);
    if (strcmp(s, "1267650600228229401496703205376") != 0) {
        printf("gmp 2^100 = %s\n", s);
        bad = 1;
    }

    mpfr_init2(x, 100);
    mpfr_const_pi(x, MPFR_RNDN);
    s = mpfr_get_str(NULL, &e, 10, 25, x, MPFR_RNDN);
    if (strcmp(s, "3141592653589793238462643") != 0 || e != 1) {
        printf("mpfr pi = %s e%ld\n", s, (long)e);
        bad = 1;
    }

    /* i * i = -1 */
    mpc_init2(c, 64);
    mpc_set_ui_ui(c, 0, 1, MPC_RNDNN);
    mpc_sqr(c, c, MPC_RNDNN);
    if (mpfr_cmp_si(mpc_realref(c), -1) != 0 || !mpfr_zero_p(mpc_imagref(c))) {
        printf("mpc i*i = (%g %g)\n", mpfr_get_d(mpc_realref(c), MPFR_RNDN),
               mpfr_get_d(mpc_imagref(c), MPFR_RNDN));
        bad = 1;
    }

    printf("gmp %s, mpfr %s, mpc %s, GMP_LIMB_BITS %d\n", gmp_version,
           mpfr_get_version(), mpc_get_version(), (int)GMP_LIMB_BITS);
    if (strcmp(gmp_version, "6.1.0") != 0 || strcmp(mpfr_get_version(), "3.1.4") != 0
        || strcmp(mpc_get_version(), "1.0.3") != 0 || GMP_LIMB_BITS != 64) {
        printf("versions differ\n");
        bad = 1;
    }
    if (bad)
        return 1;
    printf("gmp mpfr mpc link ok\n");
    return 0;
}
