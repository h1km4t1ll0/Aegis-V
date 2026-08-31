typedef unsigned int hashval_t;
typedef void *PTR;
typedef struct htab *htab_t;

hashval_t
htab_hash_string(const PTR p)
{
    (void)p;
    return 1;
}

unsigned long
htab_elements(htab_t htab)
{
    (void)htab;
    return 0;
}
