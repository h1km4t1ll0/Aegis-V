/* gcc9a -static (20-gcc95 README, rung gcc9a): prints `hello from gcc`
   and __VERSION__, `hello from gcc 9.5.0`. */
#include <stdio.h>

int main(void)
{
    printf("hello from gcc %s\n", __VERSION__);
    return 0;
}
