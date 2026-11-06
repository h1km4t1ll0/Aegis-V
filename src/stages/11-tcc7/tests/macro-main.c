#include <stdio.h>

long macro_val(void);

int main(void)
{
    if (macro_val() != 42)
        return 1;
    puts("macro ok");
    return 0;
}
