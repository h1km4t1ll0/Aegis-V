typedef int (*probe_eq)(const void *, const void *);
typedef void *PTR;

struct probe_ht {
    probe_eq eq_f;
    PTR *entries;
    unsigned long size;
};

PTR
probe_find(struct probe_ht *h, const void *element)
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
