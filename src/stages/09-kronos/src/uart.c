#include "k.h"
void uart_putc(int c)
{
    volatile unsigned char *u = (volatile unsigned char *)UART0;
    while ((u[UART_LSR] & LSR_TX) == 0)
        ;
    u[0] = (unsigned char)c;
}
void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}
