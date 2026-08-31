/* libiberty bits that used to live in mini-htab.c.
   Kept out of the hash table TU so guest cc1 can compile them separately. */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

extern void *xmalloc(unsigned long);

long
get_run_time(void)
{
    return 0;
}

void
unlock_std_streams(void)
{
}

void
unlock_stream(FILE *fp)
{
    (void)fp;
}

void
mudflap_init(void)
{
}

double
exp(double x)
{
    return x;
}

double
log(double x)
{
    return x;
}

char *
concat(const char *first, ...)
{
    va_list ap;
    const char *s;
    unsigned long n;
    char *out;
    char *p;

    n = 0;
    if (first)
        n = n + strlen(first);
    va_start(ap, first);
    while ((s = va_arg(ap, const char *)) != 0)
        n = n + strlen(s);
    va_end(ap);
    out = (char *)xmalloc(n + 1);
    p = out;
    if (first) {
        n = strlen(first);
        memcpy(p, first, n);
        p = p + n;
    }
    va_start(ap, first);
    while ((s = va_arg(ap, const char *)) != 0) {
        n = strlen(s);
        memcpy(p, s, n);
        p = p + n;
    }
    va_end(ap);
    *p = 0;
    return out;
}

int
vasprintf(char **buf, const char *fmt, va_list ap)
{
    int n;
    char tmp[4096];

    n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    if (n < 0) {
        *buf = 0;
        return -1;
    }
    *buf = (char *)xmalloc((unsigned long)n + 1);
    memcpy(*buf, tmp, (unsigned long)n + 1);
    return n;
}

int
asprintf(char **buf, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vasprintf(buf, fmt, ap);
    va_end(ap);
    return n;
}

char *
reconcat(char *optr, const char *first, ...)
{
    va_list ap;
    const char *s;
    unsigned long n;
    char *out;
    char *p;

    n = 0;
    if (first)
        n = n + strlen(first);
    va_start(ap, first);
    while ((s = va_arg(ap, const char *)) != 0)
        n = n + strlen(s);
    va_end(ap);
    out = (char *)xmalloc(n + 1);
    p = out;
    if (first) {
        n = strlen(first);
        memcpy(p, first, n);
        p = p + n;
    }
    va_start(ap, first);
    while ((s = va_arg(ap, const char *)) != 0) {
        n = strlen(s);
        memcpy(p, s, n);
        p = p + n;
    }
    va_end(ap);
    *p = 0;
    if (optr)
        free(optr);
    return out;
}
