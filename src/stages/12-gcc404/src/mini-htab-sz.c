typedef unsigned int hashval_t;
typedef void *PTR;
typedef struct htab *htab_t;

extern PTR *htab_find_slot(htab_t, const void *, int);

void
htab_remove_elt_with_hash(htab_t htab, void *element, hashval_t hash)
{
    PTR *slot;

    (void)hash;
    slot = htab_find_slot(htab, element, 0);
    if (slot)
        *slot = 0;
}

unsigned long
htab_size(htab_t htab)
{
    (void)htab;
    return 0;
}
