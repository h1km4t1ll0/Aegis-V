typedef unsigned int hashval_t;
typedef hashval_t (*htab_hash)(const void *);
typedef int (*htab_eq)(const void *, const void *);
typedef void (*htab_del)(void *);
typedef void *(*htab_alloc)(unsigned long, unsigned long);
typedef void (*htab_free)(void *);
typedef void *PTR;
typedef struct htab *htab_t;

extern void *xcalloc(unsigned long, unsigned long);
extern htab_t htab_create_alloc(unsigned long, htab_hash, htab_eq, htab_del,
                                htab_alloc, htab_free);

static void
mini_nofree2(void *p)
{
    (void)p;
}

htab_t
htab_create_alloc_ex(unsigned long size, htab_hash hash_f, htab_eq eq_f,
                     htab_del del_f, PTR alloc_arg, void *alloc_f,
                     void *free_f)
{
    (void)alloc_arg;
    (void)alloc_f;
    (void)free_f;
    return htab_create_alloc(size, hash_f, eq_f, del_f, xcalloc, mini_nofree2);
}

htab_t
htab_try_create(unsigned long size, htab_hash hash_f, htab_eq eq_f,
                htab_del del_f)
{
    return htab_create_alloc(size, hash_f, eq_f, del_f, xcalloc, mini_nofree2);
}
