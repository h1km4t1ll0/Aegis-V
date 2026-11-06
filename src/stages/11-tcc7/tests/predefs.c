#include <stdio.h>

#if !defined(__riscv_xlen) || __riscv_xlen != 64
#error __riscv_xlen
#endif
#if !defined(__ATOMIC_RELAXED) || !defined(__ATOMIC_SEQ_CST)
#error __ATOMIC_*
#endif
#if __ATOMIC_RELAXED != 0 || __ATOMIC_CONSUME != 1 || __ATOMIC_ACQUIRE != 2 \
    || __ATOMIC_RELEASE != 3 || __ATOMIC_ACQ_REL != 4 || __ATOMIC_SEQ_CST != 5
#error __ATOMIC_ values
#endif
#if __GCC_ATOMIC_INT_LOCK_FREE != 2 || __GCC_ATOMIC_LONG_LOCK_FREE != 2 \
    || __GCC_ATOMIC_POINTER_LOCK_FREE != 2 || __GCC_ATOMIC_BOOL_LOCK_FREE != 2 \
    || __GCC_ATOMIC_CHAR_LOCK_FREE != 2 || __GCC_ATOMIC_SHORT_LOCK_FREE != 2
#error __GCC_ATOMIC_*_LOCK_FREE
#endif

int main(void)
{
    puts("predefs ok");
    return 0;
}
