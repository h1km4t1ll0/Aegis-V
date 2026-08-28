#include "k.h"

static char *av_init[2];
static char *av_ver[3];
static char *av_tcc2[16];
static char *av_ver2[3];
static char *av_tcc3[16];
static char *av_ver3[3];
static char *av_tcc4[16];
static char *av_cmp[4];

static void setup_args(void)
{
    av_init[0] = "/bin/init";
    av_init[1] = 0;
    av_ver[0] = "/bin/tcc";
    av_ver[1] = "-version";
    av_ver[2] = 0;
    av_tcc2[0] = "/bin/tcc";
    av_tcc2[1] = "-o";
    av_tcc2[2] = "/bin/tcc2";
    av_tcc2[3] = "-DONE_SOURCE";
    av_tcc2[4] = "-DTCC_TARGET_RISCV64";
    av_tcc2[5] = "-I";
    av_tcc2[6] = "/include";
    av_tcc2[7] = "-nostdlib";
    av_tcc2[8] = "-Wl,-Ttext=0x10000";
    av_tcc2[9] = "-Wl,-oformat=elf64-little";
    av_tcc2[10] = "/lib/crt.s";
    av_tcc2[11] = "/lib/libc.c";
    av_tcc2[12] = "/src/tcc/tcc.c";
    av_tcc2[13] = 0;
    av_ver2[0] = "/bin/tcc2";
    av_ver2[1] = "-version";
    av_ver2[2] = 0;
    av_tcc3[0] = "/bin/tcc2";
    av_tcc3[1] = "-o";
    av_tcc3[2] = "/bin/tcc3";
    av_tcc3[3] = "-DONE_SOURCE";
    av_tcc3[4] = "-DTCC_TARGET_RISCV64";
    av_tcc3[5] = "-I";
    av_tcc3[6] = "/include";
    av_tcc3[7] = "-nostdlib";
    av_tcc3[8] = "-Wl,-Ttext=0x10000";
    av_tcc3[9] = "-Wl,-oformat=elf64-little";
    av_tcc3[10] = "/lib/crt.s";
    av_tcc3[11] = "/lib/libc.c";
    av_tcc3[12] = "/src/tcc/tcc.c";
    av_tcc3[13] = 0;
    av_ver3[0] = "/bin/tcc3";
    av_ver3[1] = "-version";
    av_ver3[2] = 0;
    av_tcc4[0] = "/bin/tcc3";
    av_tcc4[1] = "-o";
    av_tcc4[2] = "/bin/tcc4";
    av_tcc4[3] = "-DONE_SOURCE";
    av_tcc4[4] = "-DTCC_TARGET_RISCV64";
    av_tcc4[5] = "-I";
    av_tcc4[6] = "/include";
    av_tcc4[7] = "-nostdlib";
    av_tcc4[8] = "-Wl,-Ttext=0x10000";
    av_tcc4[9] = "-Wl,-oformat=elf64-little";
    av_tcc4[10] = "/lib/crt.s";
    av_tcc4[11] = "/lib/libc.c";
    av_tcc4[12] = "/src/tcc/tcc.c";
    av_tcc4[13] = 0;
    av_cmp[0] = "/bin/cmp";
    av_cmp[1] = "/bin/tcc3";
    av_cmp[2] = "/bin/tcc4";
    av_cmp[3] = 0;
}

void kmain(void)
{
    unsigned long len;
    uart_puts("Kronos\n");
    kmem_init();
    vm_init();
    ramfs_init((void *)kphys(0x80, 0x20));
    setup_args();
    if (!kfs_get("/bin/init", &len)) {
        for (;;)
            ;
    }
    run(av_init[0], av_init);
    if (!kfs_get("/bin/tcc", &len)) {
        for (;;)
            ;
    }
    run(av_ver[0], av_ver);
    run(av_tcc2[0], av_tcc2);
    run(av_ver2[0], av_ver2);
    run(av_tcc3[0], av_tcc3);
    run(av_ver3[0], av_ver3);
    run(av_tcc4[0], av_tcc4);
    {
        unsigned char *p3;
        unsigned char *p4;
        unsigned long n3;
        unsigned long n4;
        unsigned long i;
        unsigned long lim;
        unsigned long o3;
        unsigned long z3;
        unsigned long o4;
        unsigned long z4;

        p3 = kfs_get("/bin/tcc3", &n3);
        p4 = kfs_get("/bin/tcc4", &n4);
        uart_puts("tcc3sz ");
        uart_hex(n3);
        uart_puts("\n");
        uart_puts("tcc4sz ");
        uart_hex(n4);
        uart_puts("\n");
        if (p3 && p4 && n3 > 0x70 && n4 > 0x70) {
            memcpy(&o3, p3 + 0x48, 8);
            memcpy(&z3, p3 + 0x60, 8);
            memcpy(&o4, p4 + 0x48, 8);
            memcpy(&z4, p4 + 0x60, 8);
            uart_puts("tcc3ph ");
            uart_hex(o3);
            uart_puts(" ");
            uart_hex(z3);
            uart_puts("\n");
            uart_puts("tcc4ph ");
            uart_hex(o4);
            uart_puts(" ");
            uart_hex(z4);
            uart_puts("\n");
            i = 0x1000;
            lim = 0x1000 + z3;
            if (z4 < z3)
                lim = 0x1000 + z4;
            while (i < lim) {
                if (p3[i] != p4[i]) {
                    uart_puts("tdiff ");
                    uart_hex(i);
                    uart_puts(" ");
                    uart_hex((unsigned long)p3[i]);
                    uart_puts(" ");
                    uart_hex((unsigned long)p4[i]);
                    uart_puts("\n");
                    break;
                }
                i = i + 1;
            }
        }
    }
    run(av_cmp[0], av_cmp);
    for (;;)
        ;
}
