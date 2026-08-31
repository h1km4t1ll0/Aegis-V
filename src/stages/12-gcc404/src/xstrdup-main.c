#include <stdio.h>
#include "libiberty.h"

int main(void)
{
    char *p;

    p = xstrdup("xstrdup-ok");
    puts(p);
    return 0;
}
