#include <stdio.h>

long br_count(long n);
long br_cmp(long a, long b);
long br_call(void);

int main(void)
{
    if (br_count(0) != 0 || br_count(5) != 5)
        return 1;
    if (br_cmp(1, 2) != 7 || br_cmp(2, 2) != 2 || br_cmp(3, 2) != 8)
        return 2;
    if (br_cmp(-1, 2) != 3) /* signed less, unsigned greater */
        return 3;
    if (br_call() != 42)
        return 4;
    puts("branch ok");
    return 0;
}
