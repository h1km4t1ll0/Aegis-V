/* GCC's __atomic_*_n value forms (Tilck atomics.h). Built with
   /src/tcc7/lib/atomic.S; stdatomic.c gives __atomic_exchange_N. */
#include "/src/tcc7/lib/stdatomic.c"

int puts(const char *);

/* lr/sc as musl's arch/riscv64/atomic_arch.h a_cas and lib/atomic.S write
   them: the encodings (GNU as), and musl's a_cas run */
extern unsigned int lrsc_words[4];
__asm__ (".global lrsc_words\n"
         "lrsc_words:\n"
         "lr.w.aqrl a0, (a2)\n"
         "sc.w.aqrl a1, a4, (a2)\n"
         "lr.d a3, (a5)\n"
         "sc.d.rl t3, t3, (a0)\n");

static int musl_a_cas(volatile int *p, int t, int s)
{
    int old, tmp;
    __asm__ __volatile__ (
        "\n1:	lr.w.aqrl %0, (%2)\n"
        "	bne %0, %3, 1f\n"
        "	sc.w.aqrl %1, %4, (%2)\n"
        "	bnez %1, 1b\n"
        "1:"
        : "=&r"(old), "=&r"(tmp)
        : "r"(p), "r"(t), "r"(s)
        : "memory");
    return old;
}

int main(void)
{
    static volatile int i;
    static volatile long l;
    int e;
    long le;

    __atomic_store_n(&i, 5, __ATOMIC_RELAXED);
    if (__atomic_load_n(&i, __ATOMIC_RELAXED) != 5)
        return 1;
    if (__atomic_exchange_n(&i, 7, __ATOMIC_RELAXED) != 5 || i != 7)
        return 2;
    e = 6;
    if (__atomic_compare_exchange_n(&i, &e, 9, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED) || e != 7)
        return 3;
    if (!__atomic_compare_exchange_n(&i, &e, 9, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED) || i != 9)
        return 4;
    __atomic_store_n(&l, 0x123456789L, __ATOMIC_RELAXED);
    le = 0x123456789L;
    if (!__atomic_compare_exchange_n(&l, &le, -1L, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED))
        return 5;
    if (__atomic_load_n(&l, __ATOMIC_RELAXED) != -1L)
        return 6;
    if (lrsc_words[0] != 0x1606252fu || lrsc_words[1] != 0x1ee625afu)
        return 7;
    if (lrsc_words[2] != 0x1007b6afu || lrsc_words[3] != 0x1bc53e2fu)
        return 8;
    i = 5;
    if (musl_a_cas(&i, 5, 7) != 5 || i != 7)
        return 9;
    if (musl_a_cas(&i, 5, 8) != 7 || i != 7)
        return 10;
    puts("atomicn ok");
    return 0;
}
