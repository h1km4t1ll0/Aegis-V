/* Rung tcc: the first program tcc-m compiles and runs on Tilck (static, musl). */
#include <stdio.h>

int main(void)
{
    printf("tilck hello %s\n", "ok");
    return 0;
}
