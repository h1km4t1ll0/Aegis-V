typedef int (*probe_fn)(int);

struct probe_fp {
    probe_fn f;
};

int
probe_fpfield(struct probe_fp *s, int x)
{
    return s->f(x);
}
