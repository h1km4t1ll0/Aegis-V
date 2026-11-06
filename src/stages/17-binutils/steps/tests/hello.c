/* Rung binutils (17-binutils README): tcc -c, then GNU ld with musl's
   crt files, libc.a and tcc's libtcc1.a */
#include <stdio.h>

int main(void)
{
    printf("binutils hello ok\n");
    return 0;
}
