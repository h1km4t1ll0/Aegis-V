#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

long gas_li_ul(void);
char *gas_la_addend(void);
long gas_in_text_gas(void);
extern unsigned int gas_enc[];
extern char gas_odd[];
extern unsigned long gas_al8[];
extern unsigned int gas_words[];
extern char gas_common[];

static unsigned char elf[65536];

/* sh_flags of section NAME in this program's own ELF, or -1 */
static long sec_flags(const char *path, const char *name)
{
    int fd;
    long n;
    unsigned long shoff;
    unsigned int i;
    unsigned int shnum;
    unsigned int shstrndx;
    unsigned char *sh;
    unsigned char *strs;
    unsigned long off;

    fd = open(path, O_RDONLY, 0);
    if (fd < 0)
        return -1;
    n = read(fd, elf, sizeof(elf));
    close(fd);
    if (n < 64)
        return -1;
    memcpy(&shoff, elf + 0x28, 8);
    shnum = elf[0x3c] | elf[0x3d] << 8;
    shstrndx = elf[0x3e] | elf[0x3f] << 8;
    if (shoff + (unsigned long)shnum * 64 > (unsigned long)n)
        return -1;
    memcpy(&off, elf + shoff + shstrndx * 64 + 0x18, 8);
    strs = elf + off;
    for (i = 0; i < shnum; i++) {
        unsigned int nm;
        unsigned long fl;
        sh = elf + shoff + i * 64;
        nm = sh[0] | sh[1] << 8 | sh[2] << 16 | sh[3] << 24;
        if (strcmp((char *)strs + nm, name) == 0) {
            memcpy(&fl, sh + 8, 8);
            return (long)fl;
        }
    }
    return -1;
}

int main(void)
{
    static const unsigned int enc[7] = {
        0x10002573, /* csrr a0, sstatus */
        0x14029073, /* csrw sscratch, t0 */
        0x142025f3, /* csrr a1, scause */
        0x14302673, /* csrr a2, stval */
        0x141026f3, /* csrr a3, sepc */
        0x00329073, /* fscsr t0 = csrrw x0, fcsr, t0 */
        0x00868693, /* addi a3, a3, 8 */
    };
    unsigned long a;
    long fl;
    int i;

    if (gas_li_ul() != (0x6000 + 8) >> 1)
        return 1;
    for (i = 0; i < 7; i++)
        if (gas_enc[i] != enc[i])
            return 10 + i;
    a = (unsigned long)gas_al8;
    if (a & 7 || a - (unsigned long)gas_odd != 8)
        return 2;
    if (gas_al8[0] != 0x1122334455667788UL)
        return 3;
    if (gas_la_addend() != (char *)&gas_al8[1])
        return 4;
    if (gas_words[0] != 0x11223344 || gas_words[1] != 5 || ((char *)gas_words)[8] != 9)
        return 5;
    if ((unsigned long)gas_common & 8191)
        return 6;
    gas_common[99] = 1;
    if (gas_in_text_gas() != 7)
        return 7;
    fl = sec_flags("/bin/t7-gasdir", ".text.gas");
    if (fl < 0 || !(fl & 4))   /* SHF_EXECINSTR */
        return 8;
    puts("gasdir ok");
    return 0;
}
