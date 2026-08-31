typedef struct htab *htab_t;
typedef int (*htab_trav)(void **, void *);

void
htab_traverse(htab_t htab, htab_trav callback, void *info)
{
    (void)htab;
    (void)callback;
    (void)info;
}

void
htab_traverse_noresize(htab_t htab, htab_trav callback, void *info)
{
    (void)htab;
    (void)callback;
    (void)info;
}
