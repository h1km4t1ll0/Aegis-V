/* Guest cc1 cannot expand va_list/va_start (trap). RV64 pointer args
   match varargs concat(a, b, ..., 0) for up to 8 strings. */
extern unsigned long strlen(const char *);
extern void *memcpy(void *, const void *, unsigned long);
extern void *xmalloc(unsigned long);

char *
concat(const char *a0, const char *a1, const char *a2, const char *a3,
       const char *a4, const char *a5, const char *a6, const char *a7)
{
    const char *s;
    unsigned long n;
    unsigned long k;
    unsigned long i;
    char *out;
    char *p;
    const char *parts[8];

    parts[0] = a0;
    parts[1] = a1;
    parts[2] = a2;
    parts[3] = a3;
    parts[4] = a4;
    parts[5] = a5;
    parts[6] = a6;
    parts[7] = a7;
    n = 0;
    i = 0;
    while (i < 8UL) {
        s = parts[i];
        if (!s)
            break;
        n = n + strlen(s);
        i = i + 1;
    }
    out = (char *)xmalloc(n + 1);
    p = out;
    i = 0;
    while (i < 8UL) {
        s = parts[i];
        if (!s)
            break;
        k = strlen(s);
        memcpy(p, s, k);
        p = p + k;
        i = i + 1;
    }
    *p = 0;
    return out;
}
