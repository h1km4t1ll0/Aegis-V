typedef int (*probe_eq)(const void *, const void *);
typedef void *(*probe_alloc)(unsigned long, unsigned long);
typedef void *PTR;

struct probe_ht {
    probe_eq eq_f;
    PTR *entries;
    unsigned long size;
    probe_alloc alloc_f;
};

struct probe_ht *
probe_create_nf(probe_eq eq_f, probe_alloc alloc_f)
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

PTR
probe_find_nf(struct probe_ht *h, const void *element)
{
    unsigned long i;

    if (!h)
        return 0;
    for (i = 0; i < h->size; i++) {
        if (h->entries[i] && h->eq_f(h->entries[i], element))
            return h->entries[i];
    }
    return 0;
}
