typedef void *(*probe_alloc)(unsigned long, unsigned long);

void *
probe_callalloc(probe_alloc a)
{
    return a(1, 16);
}
