/* Minimal repro of mini-htab pieces that hello/probes do not cover. */
typedef unsigned int hashval_t;
typedef hashval_t (*htab_hash)(const void *);
typedef int (*htab_eq)(const void *, const void *);
typedef void *(*htab_alloc)(unsigned long, unsigned long);
typedef void *PTR;

struct htab {
    htab_hash hash_f;
    htab_eq eq_f;
    PTR *entries;
    unsigned long size;
    unsigned long n_elements;
    htab_alloc alloc_f;
};

extern void *xcalloc(unsigned long, unsigned long);

static hashval_t
hash_pointer(const void *p)
{
    return (hashval_t)(unsigned long)p;
}

static int
eq_pointer(const void *a, const void *b)
{
    return a == b;
}

htab_hash probe_hash_pointer = hash_pointer;
htab_eq probe_eq_pointer = eq_pointer;

struct htab *
probe_htab_create(htab_hash hash_f, htab_eq eq_f, htab_alloc alloc_f)
{
    struct htab *t;

    t = (struct htab *)alloc_f(1, sizeof(struct htab));
    if (!t)
        return 0;
    t->entries = (PTR *)alloc_f(8, sizeof(PTR));
    if (!t->entries)
        return 0;
    t->size = 8;
    t->n_elements = 0;
    t->hash_f = hash_f;
    t->eq_f = eq_f;
    t->alloc_f = alloc_f;
    return t;
}

PTR
probe_htab_find(struct htab *htab, const void *element)
{
    unsigned long i;

    if (!htab)
        return 0;
    for (i = 0; i < htab->size; i++) {
        if (htab->entries[i] && htab->eq_f(htab->entries[i], element))
            return htab->entries[i];
    }
    return 0;
}

struct htab *
probe_htab_default(void)
{
    return probe_htab_create(hash_pointer, eq_pointer, xcalloc);
}
