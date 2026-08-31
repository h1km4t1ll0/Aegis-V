void *xmalloc(unsigned long);

static const char *
probe_munge(const char *filename)
{
    int len;
    const char *p, *q;
    char *dst, *buffer;

    for (p = filename, len = 0; *p; p++, len++) {
        switch (*p) {
        case ' ':
        case '\t':
            for (q = p - 1; filename <= q && *q == '\\'; q--)
                len++;
            len++;
            break;
        case '$':
            len++;
            break;
        }
    }
    buffer = xmalloc((unsigned long)(len + 1));
    for (p = filename, dst = buffer; *p; p++, dst++) {
        switch (*p) {
        case ' ':
        case '\t':
            for (q = p - 1; filename <= q && *q == '\\'; q--)
                *dst++ = '\\';
            *dst++ = '\\';
            break;
        case '$':
            *dst++ = '$';
            break;
        default:
            break;
        }
        *dst = *p;
    }
    *dst = '\0';
    return buffer;
}

const char *
probe_switch(const char *s)
{
    return probe_munge(s);
}
