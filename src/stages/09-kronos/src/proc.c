#include "k.h"

struct proc {
    int exit_code;
    char cwd[2];
};

static struct proc cur;

const char *proc_cwd(void)
{
    return cur.cwd;
}

void enter_user(unsigned long entry, unsigned long user_sp, char **argv)
{
    unsigned long *sp;
    char *str;
    unsigned long argc;
    unsigned long i;
    unsigned long sl;
    unsigned long off;
    char *base;

    argc = 0;
    if (argv) {
        while (argv[argc])
            argc = argc + 1;
    }
    off = 64 + argc * 8;
    base = (char *)(user_sp + off);
    str = base;
    i = 0;
    while (i < argc) {
        sl = strlen(argv[i]);
        memcpy(str, argv[i], sl + 1);
        str = str + sl + 1;
        i = i + 1;
    }
    sp = (unsigned long *)user_sp;
    sp[0] = argc;
    str = base;
    i = 0;
    while (i < argc) {
        sp[1 + i] = (unsigned long)str;
        sl = strlen(str);
        str = str + sl + 1;
        i = i + 1;
    }
    sp[1 + argc] = 0;
    sp[2 + argc] = 0;
    sp[3 + argc] = 0;
    sp[4 + argc] = 0;
    cur.exit_code = 0;
    cur.cwd[0] = '/';
    cur.cwd[1] = 0;
    mret_user(entry, user_sp);
}

void run(char *path, char **argv)
{
    unsigned long len;
    void *blob;
    unsigned long entry;
    blob = kfs_get(path, &len);
    if (!blob) {
        uart_puts("kfs miss ");
        uart_puts(path);
        uart_puts("\n");
        panic(path);
    }
    entry = elf_load(blob, len);
    enter_user(entry, kphys_usp(), argv);
}
