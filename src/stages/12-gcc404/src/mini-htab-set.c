typedef unsigned int hashval_t;
typedef hashval_t (*htab_hash)(const void *);
typedef int (*htab_eq)(const void *, const void *);
typedef void (*htab_del)(void *);
typedef void *PTR;
typedef struct htab *htab_t;

void
htab_set_functions_ex(htab_t htab, htab_hash hash_f, htab_eq eq_f,
                      htab_del del_f, PTR alloc_arg, void *alloc_f,
                      void *free_f)
{
    (void)htab;
    (void)hash_f;
    (void)eq_f;
    (void)del_f;
    (void)alloc_arg;
    (void)alloc_f;
    (void)free_f;
}
