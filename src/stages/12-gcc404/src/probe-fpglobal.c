typedef int (*probe_fn)(int);

static int
probe_id(int x)
{
    return x;
}

probe_fn probe_gfn = probe_id;
