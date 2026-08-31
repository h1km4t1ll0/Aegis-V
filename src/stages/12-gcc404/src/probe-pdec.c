int
probe_pdec(const char *filename, const char *p)
{
    const char *q;
    int len;

    len = 0;
    for (q = p - 1; filename <= q && *q == '\\'; q--)
        len++;
    return len;
}
