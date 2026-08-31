struct probe_pi {
    const char *option;
    int value;
    int min_value;
    int max_value;
    const char *help;
};

static const struct probe_pi probe_ctor_table[] = {
    { "a", 1, 0, 0, "help a" },
    { "b", 2, 0, 0, "help b" },
    { 0, 0, 0, 0, 0 }
};

int
probe_ctor(void)
{
    return probe_ctor_table[0].value;
}
