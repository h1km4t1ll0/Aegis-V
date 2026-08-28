#include "k.h"
void trap_c(unsigned long mcause, unsigned long *regs)
{
    unsigned long epc;
    unsigned long pa;
    if (mcause == MCAUSE_ECALL_U || mcause == MCAUSE_ECALL_M) {
        regs[10] = (unsigned long)syscall(
            (long)regs[17],
            (long)regs[10],
            (long)regs[11],
            (long)regs[12],
            (long)regs[13],
            (long)regs[14],
            (long)regs[15]);
        epc = read_mepc();
        write_mepc(epc + 4);
        return;
    }
    epc = read_mepc();
    uart_puts("trap mcause ");
    uart_hex(mcause);
    uart_puts(" mepc ");
    uart_hex(epc);
    uart_puts(" sp ");
    uart_hex(regs[2]);
    uart_puts(" gp ");
    uart_hex(regs[3]);
    uart_puts(" s0 ");
    uart_hex(regs[8]);
    uart_puts(" a0 ");
    uart_hex(regs[10]);
    uart_puts(" t0 ");
    uart_hex(regs[5]);
    uart_puts(" mtval ");
    uart_hex(read_mtval());
    uart_puts(" ins ");
    pa = uva2pa(epc);
    if (!pa)
        pa = epc;
    uart_hex((unsigned long)(*(unsigned int *)pa));
    uart_puts("\n");
    panic("trap");
}
