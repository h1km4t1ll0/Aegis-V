#include <stdio.h>

/* Built with -fno-pic -static: tcc7b reaches the globals of nopic-ext.c
   PC-relative (auipc + addi/ld/sd), not through a GOT. kronos.py checks
   that /bin/t7-nopic has no .got section. */
extern int nopic_var;
extern long nopic_arr[1024];
extern int (*nopic_fp)(int);
int nopic_add(int x);

int main(void)
{
    int (*f)(int);

    f = nopic_add;
    if (nopic_var != 42)
        return 1;
    nopic_var = 7;
    nopic_arr[1000] = 5;
    if (nopic_add(1) != 8 + 5)
        return 2;
    if (f(2) != 9 + 5 || f != nopic_add)
        return 3;
    nopic_fp = f;
    if (nopic_fp(3) != 10 + 5)
        return 4;
    if (&nopic_arr[1000] - nopic_arr != 1000)
        return 5;
    puts("nopic ok");
    return 0;
}
