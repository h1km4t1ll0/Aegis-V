typedef unsigned int hashval_t;
typedef struct htab *htab_t;

unsigned long
htab_collisions(htab_t htab)
{
    (void)htab;
    return 0;
}

hashval_t
iterative_hash(const void *k, unsigned long l, hashval_t init)
{
    (void)k;
    (void)l;
    return init;
}
