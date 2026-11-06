/* The first musl program under Tilck: crt1.o -> __libc_start_main ->
   main, stdio through musl's FILE buffering, exit flushes stdout. */
#include <stdio.h>

int main(void)
{
    printf("hello from %s\n", "musl");
    return 0;
}
