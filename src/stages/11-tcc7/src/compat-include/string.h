/* tcctools.c uses the char * result of strpbrk(); libc.c defines it, Kronos <string.h> does not declare it. */
#ifndef _COMPAT_STRING_H
#define _COMPAT_STRING_H
#include_next <string.h>
char *strpbrk(const char *s, const char *accept);
#endif
