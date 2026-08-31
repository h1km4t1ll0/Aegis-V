/* No stdarg.h: guest cc1 traps on va_list. Stubs keep the symbols. */
extern void *xmalloc(unsigned long);

int
vasprintf(char **buf, const char *fmt, void *ap)
{
    (void)fmt;
    (void)ap;
    *buf = 0;
    return -1;
}

int
asprintf(char **buf, const char *fmt)
{
    (void)fmt;
    *buf = 0;
    return -1;
}
