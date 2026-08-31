/* Guest cc1: same shape as probe-combo.c (2 fns, 4-field struct). */
typedef unsigned int hashval_t;
typedef hashval_t (*htab_hash)(const void *);
typedef int (*htab_eq)(const void *, const void *);
typedef void (*htab_del)(void *);
typedef void *(*htab_alloc)(unsigned long, unsigned long);
typedef void (*htab_free)(void *);
typedef void *PTR;
typedef struct htab *htab_t;

struct htab {
    htab_eq eq_f;
    PTR *entries;
    unsigned long size;
    htab_alloc alloc_f;
};

htab_t
htab_create_alloc(unsigned long size, htab_hash hash_f, htab_eq eq_f,
                  htab_del del_f, htab_alloc alloc_f, htab_free free_f)
{
    htab_t t;

    (void)size;
    (void)hash_f;
    (void)del_f;
    (void)free_f;
    t = (htab_t)alloc_f(1, sizeof(struct htab));
    if (!t)
        return 0;
    t->entries = (PTR *)alloc_f(32768, sizeof(PTR));
    if (!t->entries)
        return 0;
    t->size = 32768;
    t->eq_f = eq_f;
    t->alloc_f = alloc_f;
    return t;
}

PTR
htab_find(htab_t htab, const void *element)
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
