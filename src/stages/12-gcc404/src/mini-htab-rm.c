typedef void *PTR;
typedef struct htab *htab_t;

extern PTR *htab_find_slot(htab_t, const void *, int);

void
htab_clear_slot(htab_t htab, void **slot)
{
    (void)htab;
    if (slot)
        *slot = 0;
}

void
htab_remove_elt(htab_t htab, void *element)
{
    PTR *slot;

    slot = htab_find_slot(htab, element, 0);
    if (slot)
        *slot = 0;
}
