#include "k.h"

static unsigned long user_brk;
static unsigned long kalloc_ptr;

unsigned long kphys(unsigned long hi, unsigned long mid)
{
    if (hi == 0x80)
        return kphys_ramfs();
    if (hi == 0x81)
        return kphys_user();
    return kphys_usp();
}

void kmem_init(void)
{
    kalloc_ptr = kphys_kalloc();
    user_brk = 0;
}

void *kalloc(unsigned long n)
{
    void *p;
    n = (n + 4095) & ~4095UL;
    p = (void *)kalloc_ptr;
    kalloc_ptr = kalloc_ptr + n;
    return p;
}

unsigned long uva2pa(unsigned long va)
{
    unsigned long dram;
    unsigned long user;
    dram = kphys_dram();
    if (va >= dram)
        return va;
    user = kphys_user();
    if (va < 0x10000)
        return user + va;
    return user + (va - 0x10000);
}

void kmem_set_end(unsigned long end)
{
    user_brk = (end + 4095) & ~4095UL;
}

long kmem_brk(unsigned long a)
{
    unsigned long old;
    unsigned long pa;
    unsigned long n;
    unsigned long dram;

    if (a == 0)
        return (long)user_brk;
    dram = kphys_dram();
    /* User VA stops where the kernel image starts. A break at or above
     * that address is identity-mapped onto the kernel, and the memset
     * below would wipe uva2pa and the trap strings. */
    if (a >= dram)
        return (long)user_brk;
    old = user_brk;
    if (old >= dram)
        return (long)user_brk;
    if (a > old) {
        n = a - old;
        pa = uva2pa(old);
        if (!pa)
            pa = old;
        memset((void *)pa, 0, n);
    }
    user_brk = a;
    return (long)a;
}

long kmem_mmap(unsigned long len, unsigned long flags)
{
    void *p;
    if ((flags & MAP_ANONYMOUS) == 0)
        return -ENOSYS;
    if (len == 0)
        return -EINVAL;
    p = kalloc(len);
    memset(p, 0, len);
    return (long)p;
}

void vm_init(void)
{
    unsigned long *l2;
    unsigned long *l1;
    unsigned long *l0;
    unsigned long flags;
    unsigned long pte;
    unsigned long dram;
    unsigned long user;
    unsigned long gig;
    unsigned long i;
    unsigned long j;
    unsigned long va;
    unsigned long pa;
    unsigned long vpn2;

    flags = 0xDF;
    dram = kphys_dram();
    user = kphys_user();
    l2 = (unsigned long *)kalloc(4096);
    memset(l2, 0, 4096);
    gig = 1;
    gig = gig << 30;
    i = 0;
    while (i < 5) {
        pte = ((dram + i * gig) >> 2) | flags;
        l2[2 + i] = pte;
        i = i + 1;
    }
    vpn2 = 0;
    while (vpn2 < 2) {
        l1 = (unsigned long *)kalloc(4096);
        memset(l1, 0, 4096);
        l2[vpn2] = ((unsigned long)l1 >> 2) | 1;
        i = 0;
        while (i < 512) {
            l0 = (unsigned long *)kalloc(4096);
            memset(l0, 0, 4096);
            l1[i] = ((unsigned long)l0 >> 2) | 1;
            j = 0;
            while (j < 512) {
                va = (vpn2 << 30) + (i << 21) + (j << 12);
                if (va < 0x10000)
                    pa = user + va;
                else
                    pa = user + (va - 0x10000);
                l0[j] = (pa >> 2) | flags;
                j = j + 1;
            }
            i = i + 1;
        }
        vpn2 = vpn2 + 1;
    }
    write_satp((unsigned long)l2);
}
