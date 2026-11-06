/* The tcc miscompile behind patches/ssa-operand-alloc.patch: Ekaitz's
   reproducer (tinycc-devel, 2024-04, msg00027), the shape of GCC's
   ssa_operand_alloc(): &(f()->mem[f()->index]) with an inline f. On tcc
   mob of 2024-04 for riscv64 it crashed; exit 0 and `tcc ssaidx ok` here
   means the tcc of this rung builds it right. */
#include <stdio.h>

struct thing {
    unsigned index;
    char mem[1];
};

struct function {
    struct thing *t;
};

static inline struct thing *obtain(struct function *f)
{
    return f->t;
}

int main(void)
{
    struct thing th;
    struct function fn;
    struct function *f = &fn;
    char *ptr;

    th.index = 0;
    th.mem[0] = 'a';
    fn.t = &th;
    ptr = &(obtain(f)->mem[obtain(f)->index]);
    if (*ptr != 'a')
        return 1;
    printf("tcc ssaidx ok\n");
    return 0;
}
