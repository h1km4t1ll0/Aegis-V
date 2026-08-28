#include "k.h"

struct Elf64_Ehdr {
    unsigned char e_ident[16];
    unsigned short e_type;
    unsigned short e_machine;
    unsigned int e_version;
    unsigned long e_entry;
    unsigned long e_phoff;
    unsigned long e_shoff;
    unsigned int e_flags;
    unsigned short e_ehsize;
    unsigned short e_phentsize;
    unsigned short e_phnum;
    unsigned short e_shentsize;
    unsigned short e_shnum;
    unsigned short e_shstrndx;
};

struct Elf64_Phdr {
    unsigned int p_type;
    unsigned int p_flags;
    unsigned long p_offset;
    unsigned long p_vaddr;
    unsigned long p_paddr;
    unsigned long p_filesz;
    unsigned long p_memsz;
    unsigned long p_align;
};

unsigned long elf_load(void *blob, unsigned long len)
{
    struct Elf64_Ehdr *eh;
    unsigned char *ph;
    unsigned int i;
    unsigned long load_end;
    unsigned long load_end_va;
    struct Elf64_Phdr *p;
    unsigned char *src;
    void *dst;
    unsigned long n;
    unsigned long dest_u;
    unsigned long pa;
    unsigned long va;
    eh = (struct Elf64_Ehdr *)blob;
    uart_puts("elf len ");
    uart_hex(len);
    uart_puts(" type ");
    uart_hex(eh->e_type);
    uart_puts(" mach ");
    uart_hex(eh->e_machine);
    uart_puts(" entry ");
    uart_hex(eh->e_entry);
    uart_puts(" phnum ");
    uart_hex(eh->e_phnum);
        uart_puts(" phoff ");
        uart_hex(eh->e_phoff);
        if (eh->e_phoff && eh->e_phentsize && eh->e_phnum) {
            p = (struct Elf64_Phdr *)((unsigned char *)blob + eh->e_phoff);
            uart_puts(" p_off ");
            uart_hex(p->p_offset);
            uart_puts(" p_va ");
            uart_hex(p->p_vaddr);
            uart_puts(" p_fsz ");
            uart_hex(p->p_filesz);
            uart_puts(" p_msz ");
            uart_hex(p->p_memsz);
        }
        uart_puts("\n");
    if (len < 64)
        panic("elf");
    if (eh->e_ident[0] != 0x7f || eh->e_ident[1] != 'E'
        || eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F')
        panic("elf");
    if (eh->e_type != 2)
        panic("elf");
    if (eh->e_machine != 243)
        panic("elf");
    /* pnut-built tcc-0.9.26 can emit ET_EXEC with phnum=0 (switch bug),
     * leaving Ttext unused and .text at file offset 0x1000. */
    if (eh->e_phoff == 0 || eh->e_phentsize == 0 || eh->e_phnum == 0) {
        if (len <= 4096)
            panic("elf");
        n = len - 4096;
        dest_u = kphys_user();
        memcpy((void *)dest_u, (unsigned char *)blob + 4096, n);
        memset((unsigned char *)dest_u + n, 0, 0x200000);
        kmem_set_end(0x10000 + n);
        if (eh->e_entry)
            return eh->e_entry;
        return 0x10000;
    }
    load_end = 0;
    load_end_va = 0;
    ph = (unsigned char *)blob + eh->e_phoff;
    i = 0;
    while (i < eh->e_phnum) {
        p = (struct Elf64_Phdr *)(ph + i * eh->e_phentsize);
        i = i + 1;
        if (p->p_type != 1)
            continue;
        va = p->p_vaddr;
        pa = uva2pa(va);
        if (!pa)
            pa = va;
        dst = (void *)pa;
        src = (unsigned char *)blob + p->p_offset;
        memcpy(dst, src, p->p_filesz);
        if (p->p_memsz > p->p_filesz)
            memset((unsigned char *)dst + p->p_filesz, 0,
                   p->p_memsz - p->p_filesz);
        if (pa + p->p_memsz > load_end)
            load_end = pa + p->p_memsz;
        if (va + p->p_memsz > load_end_va)
            load_end_va = va + p->p_memsz;
    }
    kmem_set_end(load_end_va);
    if (eh->e_entry)
        return eh->e_entry;
    return load_end_va ? (load_end_va - (load_end - kphys_user())) : kphys_user();
}
