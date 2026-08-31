typedef void *(*probe_alloc)(unsigned long, unsigned long);
typedef int (*probe_eq)(const void *, const void *);

void *
probe_f1(probe_alloc a)
{
    return a(1, 16);
}

int
probe_f2(probe_eq e, const void *x)
{
    return e(x, x);
}
