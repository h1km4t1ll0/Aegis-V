#include <stdio.h>

enum small { s_a = 0, s_b = 5 } __attribute__((packed));
enum wide { w_a = 0, w_b = 70000 } __attribute__((packed));

int main(void)
{
    if (sizeof(enum small) != 1)
        return 1;
    if (sizeof(enum wide) != 4)
        return 2;
    puts("penum ok");
    return 0;
}
