/* The function epilogue must not read the frame below sp (plan 4 phase 3,
   found by rung binutils): mob ended a function with
       addi sp, s0, N; ld ra, -8(s0); ld s0, -16(s0); ret
   so for two instructions the saved ra and s0 lay below sp. RISC-V has
   no red zone: a trap taken there in kernel mode pushes its register
   frame over them (Tilck stored sscratch, 0, over the saved s0), a
   signal frame does the same in user mode. Each function below is
   scanned from its address to its `ret`: after the instruction that
   sets sp from s0, no load may use s0. Then they are called. Exit 0 and
   `epilog ok`, or the number of the failed check. */
#include <stdarg.h>

int puts(const char *);

static int leaf(int a, int b)
{
    volatile int x[4];

    x[0] = a;
    x[3] = b;
    return x[0] + x[3];
}

static long vsum(int n, ...)
{
    va_list ap;
    long s = 0;
    int i;

    va_start(ap, n);
    for (i = 0; i < n; i++)
        s = s + va_arg(ap, long);
    va_end(ap);
    return s;
}

static int big(int a)
{
    volatile char buf[4096];

    buf[0] = (char)a;
    buf[4095] = (char)(a + 1);
    return buf[0] + buf[4095];
}

static int nested(int a)
{
    return leaf(a, 1) + leaf(a, 2) + big(a);
}

/* 0 if the code at f, up to its first ret, never loads through s0 after
   it set sp from another register (addi sp, rX, imm with rX != sp: the
   frame given back) */
static int check(void *f)
{
    unsigned int *p = f;
    int sp_back = 0;
    int i;

    for (i = 0; i < 4096; i++) {
        unsigned int w = p[i];

        if (w == 0x00008067u)                    /* ret */
            return 0;
        if ((w & 0x00007fffu) == 0x00000113u && ((w >> 15) & 31) != 2)
            sp_back = 1;                         /* addi sp, rX, imm */
        if (sp_back && (w & 0x000f807fu) == 0x00040003u) /* load imm(s0) */
            return 1;
    }
    return 2;
}

int main(void)
{
    if (check((void *)leaf))
        return 1;
    if (check((void *)vsum))
        return 2;
    if (check((void *)big))
        return 3;
    if (check((void *)nested))
        return 4;
    if (check((void *)main))
        return 5;
    if (leaf(3, 4) != 7 || vsum(3, 10L, 20L, 30L) != 60 || nested(5) != 5 + 1 + 5 + 2 + 11)
        return 6;
    puts("epilog ok");
    return 0;
}
