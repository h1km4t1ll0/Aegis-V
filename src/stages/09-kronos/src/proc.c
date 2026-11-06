#include "k.h"

struct proc {
    int exit_code;
    char cwd[256];
};

static char start_cwd[256];
static int start_cwd_set;
static struct proc cur;

void proc_set_cwd(const char *path)
{
    unsigned long i;

    i = 0;
    while (path[i] && i < 255) {
        start_cwd[i] = path[i];
        i = i + 1;
    }
    start_cwd[i] = 0;
    start_cwd_set = 1;
}

void proc_set_exit(int code)
{
    cur.exit_code = code;
}

int proc_last_exit(void)
{
    return cur.exit_code;
}

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
    if (start_cwd_set) {
        i = 0;
        while (start_cwd[i] && i < 255) {
            cur.cwd[i] = start_cwd[i];
            i = i + 1;
        }
        cur.cwd[i] = 0;
    } else {
        cur.cwd[0] = '/';
        cur.cwd[1] = 0;
    }
    mret_user(entry, user_sp);
}

void run(char *path, char **argv)
{
    unsigned long len;
    void *blob;
    unsigned long entry;
    const char *p;
    const char *q;
    int same;

    /* After skip-tcc rebuild, /bin/tcc6n has ET_REL -c; keep /bin/tcc6
     * as the compiler for that rebuild itself (tcc6n not yet present). */
    if (argv && argv[0] && path) {
        p = path;
        q = "/bin/tcc6";
        same = 1;
        while (*q) {
            if (*p != *q) {
                same = 0;
                break;
            }
            p = p + 1;
            q = q + 1;
        }
        if (same && *p == 0 && kfs_get("/bin/tcc6n", &len)) {
            path = "/bin/tcc6n";
            argv[0] = "/bin/tcc6n";
        }
    }
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
