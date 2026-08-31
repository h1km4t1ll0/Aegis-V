typedef struct htab *htab_t;
typedef int (*htab_trav)(void **, void *);

void
htab_delete(htab_t htab)
{
    (void)htab;
}

void
htab_empty(htab_t htab)
{
    (void)htab;
}
