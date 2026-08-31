char *concat (const char *, ...);

char *
probe_vararg(char *key)
{
    return concat(key, "_ROOT", (char *)0);
}
