#include <stdio.h>

int main(void)
{
    int answer = 0;
    FILE *f = fopen("/etc/aegis-demo.conf", "r");
    if (!f || fscanf(f, "answer=%d", &answer) != 1 || fclose(f) || answer != 42) return 1;
    puts("diskroot saved program ok answer=42");
    return 0;
}
