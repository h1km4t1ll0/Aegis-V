/* tccrun.c uses the FILE * result of freopen(); libc.c defines it, Kronos <stdio.h> does not declare it. */
#ifndef _COMPAT_STDIO_H
#define _COMPAT_STDIO_H
#include_next <stdio.h>
FILE *freopen(const char *path, const char *mode, FILE *f);
#endif
