/* Guest cc1 probe: indirect call. Not linked into the first cc1. */
typedef int (*probe_fn)(int);

int
probe_indirect(probe_fn f, int x)
{
    return f(x);
}
