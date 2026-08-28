#include "k.h"
void *memcpy(void *d, const void *s, unsigned long n)
{
    unsigned char *dd = d;
    const unsigned char *ss = s;
    while (n--)
        *dd++ = *ss++;
    return d;
}
void *memset(void *d, int c, unsigned long n)
{
    unsigned char *dd = d;
    while (n--)
        *dd++ = (unsigned char)c;
    return d;
}
unsigned long strlen(const char *s)
{
    unsigned long n = 0;
    while (s[n])
        n++;
    return n;
}
void uart_hex(unsigned long x)
{
    char buf[16];
    int i;
    unsigned long v;
    unsigned long d;
    i = 0;
    v = x;
    if (v == 0) {
        uart_puts("0x0");
        return;
    }
    while (v != 0) {
        d = v & 15;
        if (d < 10)
            buf[i] = (char)('0' + d);
        else
            buf[i] = (char)('a' + (d - 10));
        i = i + 1;
        v = v >> 4;
    }
    uart_puts("0x");
    while (i > 0) {
        i = i - 1;
        uart_putc(buf[i]);
    }
}
void panic(const char *s)
{
    uart_puts("panic: ");
    uart_puts(s);
    uart_puts("\n");
    for (;;)
        ;
}
