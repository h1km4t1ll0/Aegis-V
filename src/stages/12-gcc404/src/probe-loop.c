/* Guest cc1 probe: loop + struct fields. Not linked into the first cc1. */
struct probe_s {
    int *p;
    int n;
};

int
probe_loop(struct probe_s *s, int v)
{
    int i;
    if (!s || !s->p)
        return 0;
    for (i = 0; i < s->n; i++) {
        if (s->p[i] == v)
            return i;
    }
    return -1;
}
