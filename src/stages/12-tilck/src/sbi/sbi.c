/* Aegis-V SBI shim (M mode) for Tilck on QEMU virt; see README.md.
   Loads tilck.elf from the bootinfo block, adds initrd/bootargs to
   /chosen and the outbox region to /reserved-memory, drops to S mode,
   then serves ecalls and the machine timer. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <libfdt.h>

#define STR_(x) #x
#define STR(x) STR_(x)
#define CSR_READ(c) ({ unsigned long v_; \
    __asm__ volatile ("csrr %0, " STR(c) : "=r"(v_)); v_; })
#define CSR_WRITE(c, v) __asm__ volatile ("csrw " STR(c) ", %0" :: "r"((unsigned long)(v)))
#define CSR_SET(c, v) __asm__ volatile ("csrs " STR(c) ", %0" :: "r"((unsigned long)(v)))
#define CSR_CLEAR(c, v) __asm__ volatile ("csrc " STR(c) ", %0" :: "r"((unsigned long)(v)))

#define MSTATUS 0x300
#define MEDELEG 0x302
#define MIDELEG 0x303
#define MIE 0x304
#define MCOUNTEREN 0x306
#define MCAUSE 0x342
#define MEPC 0x341
#define MTVAL 0x343
#define MIP 0x344
#define PMPCFG0 0x3a0
#define PMPADDR0 0x3b0
#define SATP 0x180
#define MVENDORID 0xf11
#define MARCHID 0xf12
#define MIMPID 0xf13

#define MIP_STIP (1UL << 5)
#define MIP_MTIP (1UL << 7)
#define MSTATUS_MPP (3UL << 11)
#define MSTATUS_MPP_S (1UL << 11)
#define MSTATUS_MPIE (1UL << 7)

#define UART ((volatile unsigned char *)0x10000000UL)
#define CLINT_MTIMECMP 0x2004000UL
#define SIFIVE_TEST 0x100000UL

#define KERNEL_VA_PA 0xFFFFFFFF00000000UL
#define SBI_RESERVED 0x200000UL

#define SBI_ERR_NOT_SUPPORTED (-2L)
#define SBI_ERR_INVALID_PARAM (-3L)
#define EXT_BASE 0x10
#define EXT_TIME 0x54494D45
#define EXT_SRST 0x53525354

/* Written by src/scripts/tilck.py at 0x87F00000 (little endian). */
struct bootinfo {
    uint64_t magic;
    uint64_t elf_addr;
    uint64_t elf_size;
    uint64_t initrd_addr;
    uint64_t initrd_size;
    uint64_t outbox_addr;
    uint64_t outbox_size;
    char bootargs[200];
};
#define BOOTINFO_ADDR 0x87F00000UL
#define BOOTINFO_MAGIC 0x32544F4F424B4C54UL /* "TLKBOOT2" */

struct elf64_ehdr {
    unsigned char e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version;
    uint64_t e_entry, e_phoff, e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
};

struct elf64_phdr {
    uint32_t p_type, p_flags;
    uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
};

unsigned char sbi_stack[16384] __attribute__((aligned(16)));
static unsigned char fdt_buf[0x10000] __attribute__((aligned(8)));

void sbi_enter_s(unsigned long hartid, void *dtb, unsigned long entry);

/* libc for libfdt and tcc (struct copies) */
void *memcpy(void *d, const void *s, size_t n)
{
    unsigned char *dp = d;
    const unsigned char *sp = s;
    while (n--)
        *dp++ = *sp++;
    return d;
}

void *memmove(void *d, const void *s, size_t n)
{
    unsigned char *dp = d;
    const unsigned char *sp = s;
    if (dp < sp)
        return memcpy(d, s, n);
    while (n--)
        dp[n] = sp[n];
    return d;
}

void *memset(void *d, int c, size_t n)
{
    unsigned char *dp = d;
    while (n--)
        *dp++ = (unsigned char)c;
    return d;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = a;
    const unsigned char *y = b;
    for (; n; n--, x++, y++)
        if (*x != *y)
            return *x - *y;
    return 0;
}

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;
    for (; n; n--, p++)
        if (*p == (unsigned char)c)
            return (void *)p;
    return NULL;
}

size_t strlen(const char *s)
{
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}

char *strrchr(const char *s, int c)
{
    const char *r = NULL;
    for (;; s++) {
        if (*s == (char)c)
            r = s;
        if (!*s)
            return (char *)r;
    }
}

size_t strnlen(const char *s, size_t max)
{
    size_t n = 0;
    while (n < max && s[n])
        n++;
    return n;
}

/* Stores go through an address in a register: tcc mob's store() to a
   constant address uses s0 (the frame pointer) as scratch. */
static void mmio_w8(unsigned long a, unsigned char v)
{
    *(volatile unsigned char *)a = v;
}

static void mmio_w32(unsigned long a, uint32_t v)
{
    *(volatile uint32_t *)a = v;
}

static void mmio_w64(unsigned long a, uint64_t v)
{
    *(volatile uint64_t *)a = v;
}

static void uart_putc(int c)
{
    while (!(UART[5] & 0x20))
        ;
    mmio_w8(0x10000000UL, (unsigned char)c);
}

static int uart_getc(void)
{
    if (!(UART[5] & 0x01))
        return -1;
    return UART[0];
}

static void puts_(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

static void puthex(unsigned long v)
{
    int i;
    puts_("0x");
    for (i = 60; i >= 0; i -= 4)
        uart_putc("0123456789abcdef"[(v >> i) & 15]);
}

static void poweroff(uint32_t code)
{
    mmio_w32(SIFIVE_TEST, code); /* 0x5555 off, 0x7777 reset */
    for (;;)
        __asm__ volatile ("wfi");
}

static void fail(const char *msg, unsigned long v)
{
    puts_("sbi fail: ");
    puts_(msg);
    puts_(" ");
    puthex(v);
    puts_("\n");
    for (;;)
        __asm__ volatile ("wfi");
}

static void set_timer(uint64_t t)
{
    mmio_w64(CLINT_MTIMECMP, t);
    CSR_CLEAR(MIP, MIP_STIP);
    CSR_SET(MIE, MIP_MTIP);
}

static unsigned long va_to_pa(unsigned long va)
{
    return va >= KERNEL_VA_PA ? va - KERNEL_VA_PA : va;
}

/* Copy each PT_LOAD to p_vaddr - 0xFFFFFFFF00000000, zero the bss tail. */
static unsigned long load_elf(unsigned long addr, unsigned long size)
{
    struct elf64_ehdr *eh = (struct elf64_ehdr *)addr;
    struct elf64_phdr *ph;
    unsigned char *dst;
    int i;

    if (size < sizeof(*eh) || memcmp(eh->e_ident, "\177ELF\2\1", 6) || eh->e_machine != 243)
        fail("bad elf at", addr);
    for (i = 0; i < eh->e_phnum; i++) {
        ph = (struct elf64_phdr *)(addr + eh->e_phoff + (unsigned long)i * eh->e_phentsize);
        if (ph->p_type != 1)
            continue;
        if (ph->p_offset + ph->p_filesz > size || ph->p_filesz > ph->p_memsz)
            fail("bad PT_LOAD", ph->p_vaddr);
        dst = (unsigned char *)va_to_pa(ph->p_vaddr);
        memcpy(dst, (void *)(addr + ph->p_offset), ph->p_filesz);
        memset(dst + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);
    }
    __asm__ volatile ("fence.i");
    return va_to_pa(eh->e_entry);
}

/* /reserved-memory/mmode_resv0@80000000 (as OpenSBI) over the 2 MB below
   the kernel: this shim and Tilck's DTB copy. Tilck reads /memreserve/
   entries only when a /reserved-memory node exists. */
static int reserve_sbi(void *fdt)
{
    fdt32_t reg[4];
    int rm;
    int node;
    int rc = 0;

    rm = fdt_path_offset(fdt, "/reserved-memory");
    if (rm < 0) {
        rm = fdt_add_subnode(fdt, 0, "reserved-memory");
        if (rm < 0)
            return rm;
        rc |= fdt_setprop_u32(fdt, rm, "#address-cells", 2);
        rc |= fdt_setprop_u32(fdt, rm, "#size-cells", 2);
        rc |= fdt_setprop_empty(fdt, rm, "ranges");
    }
    node = fdt_add_subnode(fdt, rm, "mmode_resv0@80000000");
    if (node < 0)
        return node;
    reg[0] = cpu_to_fdt32(0);
    reg[1] = cpu_to_fdt32(0x80000000UL);
    reg[2] = cpu_to_fdt32(0);
    reg[3] = cpu_to_fdt32(SBI_RESERVED);
    rc |= fdt_setprop(fdt, node, "reg", reg, sizeof(reg));
    rc |= fdt_setprop_empty(fdt, node, "no-map");
    return rc;
}

/* /reserved-memory/outbox@ADDR (no-map) over the outbox region of the
   bootinfo; Tilck's outbox module (/dev/outbox) finds it by name. */
static int reserve_outbox(void *fdt, struct bootinfo *bi)
{
    char name[32];
    fdt32_t reg[4];
    int node;
    int rc = 0;
    int i = 7;
    int sh = 60;

    memcpy(name, "outbox@", 7);
    while (sh > 0 && !((bi->outbox_addr >> sh) & 15))
        sh -= 4;
    for (; sh >= 0; sh -= 4)
        name[i++] = "0123456789abcdef"[(bi->outbox_addr >> sh) & 15];
    name[i] = 0;
    node = fdt_add_subnode(fdt, fdt_path_offset(fdt, "/reserved-memory"), name);
    if (node < 0)
        return node;
    reg[0] = cpu_to_fdt32(bi->outbox_addr >> 32);
    reg[1] = cpu_to_fdt32(bi->outbox_addr);
    reg[2] = cpu_to_fdt32(bi->outbox_size >> 32);
    reg[3] = cpu_to_fdt32(bi->outbox_size);
    rc |= fdt_setprop(fdt, node, "reg", reg, sizeof(reg));
    rc |= fdt_setprop_empty(fdt, node, "no-map");
    return rc;
}

/* Copy the DTB with room to grow; /chosen gets the initrd range and
   bootargs; the 2 MB below the kernel and the outbox are reserved memory. */
static void *prepare_fdt(void *dtb, struct bootinfo *bi)
{
    void *fdt = fdt_buf;
    int node;
    int rc;

    rc = fdt_open_into(dtb, fdt, sizeof(fdt_buf));
    if (rc)
        fail("fdt_open_into", (unsigned long)(long)rc);
    node = fdt_path_offset(fdt, "/chosen");
    if (node < 0)
        node = fdt_add_subnode(fdt, 0, "chosen");
    if (node < 0)
        fail("no /chosen", (unsigned long)(long)node);
    rc = 0;
    if (bi->initrd_size) {
        rc |= fdt_setprop_u64(fdt, node, "linux,initrd-start", bi->initrd_addr);
        rc |= fdt_setprop_u64(fdt, node, "linux,initrd-end", bi->initrd_addr + bi->initrd_size);
    }
    bi->bootargs[sizeof(bi->bootargs) - 1] = 0;
    if (bi->bootargs[0])
        rc |= fdt_setprop_string(fdt, node, "bootargs", bi->bootargs);
    rc |= reserve_sbi(fdt);
    if (bi->outbox_size)
        rc |= reserve_outbox(fdt, bi);
    if (rc)
        fail("fdt setprop", (unsigned long)(long)rc);
    return fdt;
}

void sbi_main(unsigned long hartid, void *dtb)
{
    /* via an unsigned long: tcc mob sign-extends a pointer constant
       with bit 31 set (lui) */
    unsigned long bi_addr = BOOTINFO_ADDR;
    struct bootinfo *bi = (struct bootinfo *)bi_addr;
    unsigned long entry;
    void *fdt;

    puts_("sbi: aegis-v m-mode shim, hart ");
    puthex(hartid);
    puts_(" dtb ");
    puthex((unsigned long)dtb);
    puts_("\n");
    if (bi->magic != BOOTINFO_MAGIC)
        fail("no bootinfo at", (unsigned long)bi);

    /* PMP entry 0: NAPOT over all memory, RWX for S and U */
    CSR_WRITE(PMPADDR0, ~0UL >> 10);
    CSR_WRITE(PMPCFG0, 0x1f);
    /* all exceptions to S except ecall from S (9) and from M (11) */
    CSR_WRITE(MEDELEG, 0xb1ff);
    /* S software, S timer, S external interrupts */
    CSR_WRITE(MIDELEG, 0x222);
    CSR_WRITE(MCOUNTEREN, 7);
    mmio_w64(CLINT_MTIMECMP, ~0UL);
    CSR_WRITE(MIE, 0);

    entry = load_elf(bi->elf_addr, bi->elf_size);
    fdt = prepare_fdt(dtb, bi);
    puts_("sbi: entry ");
    puthex(entry);
    puts_(" initrd ");
    puthex(bi->initrd_addr);
    puts_(" size ");
    puthex(bi->initrd_size);
    puts_(" outbox ");
    puthex(bi->outbox_addr);
    puts_(" size ");
    puthex(bi->outbox_size);
    puts_("\n");

    CSR_WRITE(SATP, 0);
    CSR_CLEAR(MSTATUS, MSTATUS_MPP | MSTATUS_MPIE);
    CSR_SET(MSTATUS, MSTATUS_MPP_S);
    sbi_enter_s(hartid, fdt, entry);
}

static int probe(unsigned long eid)
{
    return eid == EXT_BASE || eid == EXT_TIME || eid == EXT_SRST
        || eid == 0 || eid == 1 || eid == 2 || eid == 8;
}

/* regs[i] = x_i of the S context; a0 = regs[10], a6 = fid, a7 = eid */
static void ecall(unsigned long *regs)
{
    unsigned long eid = regs[17];
    unsigned long fid = regs[16];
    unsigned long a0 = regs[10];
    long err = 0;
    unsigned long val = 0;

    switch (eid) {
    case 0: /* legacy set_timer */
        set_timer(a0);
        regs[10] = 0;
        return;
    case 1: /* legacy console_putchar */
        uart_putc((int)a0);
        regs[10] = 0;
        return;
    case 2: /* legacy console_getchar */
        regs[10] = (unsigned long)(long)uart_getc();
        return;
    case 8: /* legacy shutdown */
        poweroff(0x5555);
        return;
    case EXT_BASE:
        if (fid == 0)
            val = 2; /* spec 0.2 */
        else if (fid == 1 || fid == 2)
            val = 0;
        else if (fid == 3)
            val = probe(a0);
        else if (fid == 4)
            val = CSR_READ(MVENDORID);
        else if (fid == 5)
            val = CSR_READ(MARCHID);
        else if (fid == 6)
            val = CSR_READ(MIMPID);
        else
            err = SBI_ERR_NOT_SUPPORTED;
        break;
    case EXT_TIME:
        if (fid == 0)
            set_timer(a0);
        else
            err = SBI_ERR_NOT_SUPPORTED;
        break;
    case EXT_SRST:
        if (fid == 0 && a0 == 0)
            poweroff(0x5555);
        if (fid == 0 && (a0 == 1 || a0 == 2))
            poweroff(0x7777);
        err = fid == 0 ? SBI_ERR_INVALID_PARAM : SBI_ERR_NOT_SUPPORTED;
        break;
    default:
        err = SBI_ERR_NOT_SUPPORTED;
        break;
    }
    regs[10] = (unsigned long)err;
    regs[11] = val;
}

void sbi_trap(unsigned long *regs)
{
    unsigned long cause = CSR_READ(MCAUSE);

    if ((long)cause < 0 && (cause & 0xff) == 7) {
        /* machine timer: hand it to S as STIP until the next set_timer */
        CSR_CLEAR(MIE, MIP_MTIP);
        CSR_SET(MIP, MIP_STIP);
        return;
    }
    if (cause == 9) {
        ecall(regs);
        CSR_WRITE(MEPC, CSR_READ(MEPC) + 4);
        return;
    }
    puts_("sbi trap mepc ");
    puthex(CSR_READ(MEPC));
    puts_(" mtval ");
    puthex(CSR_READ(MTVAL));
    puts_("\n");
    fail("mcause", cause);
}
