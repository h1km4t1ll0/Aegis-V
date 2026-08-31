typedef int (*probe_eq)(const void *, const void *);
typedef void *(*probe_alloc)(unsigned long, unsigned long);
typedef void *PTR;

extern void *xcalloc(unsigned long, unsigned long);

static int
eq_pointer(const void *a, const void *b)
{
    return a == b;
}

struct probe_ht {
    probe_eq eq_f;
    PTR *entries;
    unsigned long size;
    probe_alloc alloc_f;
};

struct probe_ht *
probe_create2(probe_eq eq_f, probe_alloc alloc_f)
{
    struct probe_ht *t;

    t = (struct probe_ht *)alloc_f(1, sizeof(struct probe_ht));
    if (!t)
        return 0;
    t->entries = (PTR *)alloc_f(8, sizeof(PTR));
    if (!t->entries)
        return 0;
    t->size = 8;
    t->eq_f = eq_f;
    t->alloc_f = alloc_f;
    return t;
}

struct probe_ht *
probe_default(void)
{
    return probe_create2(eq_pointer, xcalloc);
}
