/* Open-addressed stand-in for libiberty hashtab.c.
   Fixed power-of-two size (no grow: guest cc1 traps on grow/recursion).
   No system headers: guest cc1 aborts on stdlib.h statics. */
typedef unsigned int hashval_t;
typedef hashval_t (*htab_hash)(const void *);
typedef int (*htab_eq)(const void *, const void *);
typedef void (*htab_del)(void *);
typedef void *(*htab_alloc)(unsigned long, unsigned long);
typedef void (*htab_free)(void *);
typedef int (*htab_trav)(void **, void *);
typedef void *PTR;
typedef struct htab *htab_t;

struct htab {
    htab_hash hash_f;
    htab_eq eq_f;
    htab_del del_f;
    PTR *entries;
    unsigned long size;
    unsigned long n_elements;
    unsigned long n_deleted;
    unsigned int searches;
    unsigned int collisions;
    htab_alloc alloc_f;
    htab_free free_f;
    PTR alloc_arg;
    void *alloc_with_arg_f;
    void *free_with_arg_f;
    unsigned int size_prime_index;
};

extern void *xcalloc(unsigned long, unsigned long);

#define MINI_HTAB_SLOTS 32768

static void
mini_nofree(void *p)
{
    (void)p;
}

htab_t
htab_create_alloc(unsigned long size, htab_hash hash_f, htab_eq eq_f,
                  htab_del del_f, htab_alloc alloc_f, htab_free free_f)
{
    htab_t t;

    (void)size;
    t = (htab_t)alloc_f(1, sizeof(struct htab));
    if (!t)
        return 0;
    t->entries = (PTR *)alloc_f(MINI_HTAB_SLOTS, sizeof(PTR));
    if (!t->entries)
        return 0;
    t->size = MINI_HTAB_SLOTS;
    t->n_elements = 0;
    t->n_deleted = 0;
    t->searches = 0;
    t->collisions = 0;
    t->hash_f = hash_f;
    t->eq_f = eq_f;
    t->del_f = del_f;
    t->alloc_f = alloc_f;
    t->free_f = free_f;
    t->alloc_arg = 0;
    t->alloc_with_arg_f = 0;
    t->free_with_arg_f = 0;
    t->size_prime_index = 0;
    return t;
}

static unsigned long
htab_index(htab_t htab, hashval_t hash)
{
    return hash & (htab->size - 1);
}

PTR
htab_find_with_hash(htab_t htab, const void *element, hashval_t hash)
{
    unsigned long i;
    unsigned long steps;
    PTR e;

    if (!htab)
        return 0;
    i = htab_index(htab, hash);
    steps = 0;
    while (steps < htab->size) {
        e = htab->entries[i];
        if (!e)
            return 0;
        if (e != (PTR)1 && htab->eq_f(e, element))
            return e;
        i = i + 1;
        if (i >= htab->size)
            i = 0;
        steps = steps + 1;
    }
    return 0;
}

PTR
htab_find(htab_t htab, const void *element)
{
    hashval_t h;

    if (!htab)
        return 0;
    if (htab->hash_f)
        h = htab->hash_f(element);
    else
        h = (hashval_t)(unsigned long)element;
    return htab_find_with_hash(htab, element, h);
}

PTR *
htab_find_slot_with_hash(htab_t htab, const void *element, hashval_t hash,
                         int insert)
{
    unsigned long i;
    unsigned long steps;
    PTR *first_del;
    PTR e;

    if (!htab)
        return 0;
    i = htab_index(htab, hash);
    first_del = 0;
    steps = 0;
    while (steps < htab->size) {
        e = htab->entries[i];
        if (!e) {
            if (insert != 1)
                return 0;
            if (first_del)
                return first_del;
            htab->n_elements = htab->n_elements + 1;
            return htab->entries + i;
        }
        if (e == (PTR)1) {
            if (!first_del)
                first_del = htab->entries + i;
        } else if (htab->eq_f(e, element)) {
            return htab->entries + i;
        }
        i = i + 1;
        if (i >= htab->size)
            i = 0;
        steps = steps + 1;
    }
    if (insert == 1 && first_del)
        return first_del;
    return 0;
}

PTR *
htab_find_slot(htab_t htab, const void *element, int insert)
{
    hashval_t h;

    if (!htab)
        return 0;
    if (htab->hash_f)
        h = htab->hash_f(element);
    else
        h = (hashval_t)(unsigned long)element;
    return htab_find_slot_with_hash(htab, element, h, insert);
}

hashval_t
htab_hash_string(const PTR p)
{
    const unsigned char *s;
    hashval_t r;

    s = (const unsigned char *)p;
    r = 0;
    while (*s) {
        r = r * 67 + *s;
        s = s + 1;
    }
    return r + 1;
}

htab_t
htab_create(unsigned long size, htab_hash hash_f, htab_eq eq_f, htab_del del_f)
{
    return htab_create_alloc(size, hash_f, eq_f, del_f, xcalloc, mini_nofree);
}

unsigned long
htab_elements(htab_t htab)
{
    if (!htab)
        return 0;
    return htab->n_elements;
}

void
htab_delete(htab_t htab)
{
    if (!htab)
        return;
    if (htab->free_f && htab->entries)
        htab->free_f(htab->entries);
    if (htab->free_f)
        htab->free_f(htab);
}

void
htab_traverse(htab_t htab, htab_trav callback, void *info)
{
    unsigned long i;

    if (!htab || !callback)
        return;
    i = 0;
    while (i < htab->size) {
        if (htab->entries[i] && htab->entries[i] != (PTR)1) {
            if (callback(htab->entries + i, info) == 0)
                return;
        }
        i = i + 1;
    }
}

hashval_t
htab_hash_pointer(const void *p)
{
    return (hashval_t)(unsigned long)p;
}

int
htab_eq_pointer(const void *a, const void *b)
{
    return a == b;
}

void
htab_empty(htab_t htab)
{
    unsigned long i;

    if (!htab || !htab->entries)
        return;
    i = 0;
    while (i < htab->size) {
        htab->entries[i] = 0;
        i = i + 1;
    }
    htab->n_elements = 0;
    htab->n_deleted = 0;
}

void
htab_clear_slot(htab_t htab, void **slot)
{
    if (!htab || !slot)
        return;
    *slot = 0;
    if (htab->n_elements)
        htab->n_elements = htab->n_elements - 1;
}

void
htab_remove_elt(htab_t htab, void *element)
{
    PTR *slot;

    slot = htab_find_slot(htab, element, 0);
    if (slot)
        htab_clear_slot(htab, slot);
}

void
htab_remove_elt_with_hash(htab_t htab, void *element, hashval_t hash)
{
    PTR *slot;

    slot = htab_find_slot_with_hash(htab, element, hash, 0);
    if (slot)
        htab_clear_slot(htab, slot);
}

void
htab_traverse_noresize(htab_t htab, htab_trav callback, void *info)
{
    htab_traverse(htab, callback, info);
}

unsigned long
htab_size(htab_t htab)
{
    if (!htab)
        return 0;
    return htab->size;
}

unsigned long
htab_collisions(htab_t htab)
{
    (void)htab;
    return 0;
}

hashval_t
iterative_hash(const void *k, unsigned long l, hashval_t init)
{
    const unsigned char *s;
    hashval_t r;
    unsigned long i;

    s = (const unsigned char *)k;
    r = init;
    if (!s)
        return r;
    i = 0;
    while (i < l) {
        r = r * 67 + s[i];
        i = i + 1;
    }
    return r;
}

htab_t
htab_create_alloc_ex(unsigned long size, htab_hash hash_f, htab_eq eq_f,
                     htab_del del_f, PTR alloc_arg, void *alloc_f,
                     void *free_f)
{
    (void)alloc_arg;
    (void)alloc_f;
    (void)free_f;
    return htab_create_alloc(size, hash_f, eq_f, del_f, xcalloc, mini_nofree);
}

htab_t
htab_try_create(unsigned long size, htab_hash hash_f, htab_eq eq_f,
                htab_del del_f)
{
    return htab_create(size, hash_f, eq_f, del_f);
}

void
htab_set_functions_ex(htab_t htab, htab_hash hash_f, htab_eq eq_f,
                      htab_del del_f, PTR alloc_arg, void *alloc_f,
                      void *free_f)
{
    if (!htab)
        return;
    htab->hash_f = hash_f;
    htab->eq_f = eq_f;
    htab->del_f = del_f;
    htab->alloc_arg = alloc_arg;
    htab->alloc_with_arg_f = alloc_f;
    htab->free_with_arg_f = free_f;
}
