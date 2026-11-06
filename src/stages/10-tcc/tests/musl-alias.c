/* strlen body: a may_alias word and the HASZERO loop. */
typedef unsigned long size_t;
typedef unsigned long uintptr_t;

#define ALIGN (sizeof(size_t))
#define ONES ((size_t)-1 / 255)
#define HIGHS (ONES * (255 / 2 + 1))
#define HASZERO(x) ((x)-ONES & ~(x) & HIGHS)

size_t strlen(const char *s)
{
    const char *a = s;
    typedef size_t __attribute__((__may_alias__)) word;
    const word *w;

    for (; (uintptr_t)s % ALIGN; s++)
        if (!*s)
            return s - a;
    for (w = (const void *)s; !HASZERO(*w); w++)
        ;
    s = (const void *)w;
    for (; *s; s++)
        ;
    return s - a;
}
