#include <stdio.h>

extern unsigned int priv_words[4];
__asm__ (".global priv_words\n"
         "priv_words:\n"
         "sret\n"
         "mret\n"
         "sfence.vma\n"
         "sfence.vma a0, zero\n");

int main(void)
{
    if (priv_words[0] != 0x10200073u)
        return 1;
    if (priv_words[1] != 0x30200073u)
        return 2;
    if (priv_words[2] != 0x12000073u)
        return 3;
    if (priv_words[3] != 0x12050073u)
        return 4;
    puts("priv ok");
    return 0;
}
