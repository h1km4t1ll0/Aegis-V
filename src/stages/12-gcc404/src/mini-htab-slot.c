/* Guest cc1: find_slot only. Avoid PTR* + i (abort); use integer addrs. */
typedef int (*htab_eq)(const void *, const void *);
typedef void *(*htab_alloc)(unsigned long, unsigned long);
typedef void *PTR;
typedef struct htab *htab_t;

struct htab {
    htab_eq eq_f;
    PTR *entries;
    unsigned long size;
    htab_alloc alloc_f;
};

PTR *
htab_find_slot(htab_t htab, const void *element, int insert)
{
    unsigned long i;
    unsigned long base;
    unsigned long empty;
    PTR e;

    if (!htab || !htab->entries)
        return 0;
    base = (unsigned long)htab->entries;
    empty = 0;
    for (i = 0; i < htab->size; i++) {
        e = *(PTR *)(base + (i << 3));
        if (e) {
            if (htab->eq_f(e, element))
                return (PTR *)(base + (i << 3));
        } else {
            if (!empty)
                empty = base + (i << 3);
        }
    }
    if (insert == 1 && empty)
        return (PTR *)empty;
    return 0;
}
