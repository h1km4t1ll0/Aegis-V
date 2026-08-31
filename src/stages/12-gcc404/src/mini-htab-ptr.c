/* Guest cc1: 2 fns, no struct. Avoid identifier b (guest cc1: undeclared). */
typedef unsigned int hashval_t;

hashval_t
htab_hash_pointer(const void *p)
{
    return (hashval_t)(unsigned long)p;
}

int
htab_eq_pointer(const void *p, const void *q)
{
    unsigned long x;
    unsigned long y;

    x = (unsigned long)p;
    y = (unsigned long)q;
    if (x == y)
        return 1;
    return 0;
}
