typedef unsigned int hashval_t;
typedef void *PTR;
typedef struct htab *htab_t;

extern PTR htab_find(htab_t, const void *);
extern PTR *htab_find_slot(htab_t, const void *, int);

PTR
htab_find_with_hash(htab_t htab, const void *element, hashval_t hash)
{
    (void)hash;
    return htab_find(htab, element);
}

PTR *
htab_find_slot_with_hash(htab_t htab, const void *element, hashval_t hash,
                         int insert)
{
    (void)hash;
    return htab_find_slot(htab, element, insert);
}
