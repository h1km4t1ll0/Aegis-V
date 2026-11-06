/* libtcc.c calls realpath() and uses the char * result; Kronos <stdlib.h> lacks it. Defined in compat.c. */
/* Kronos <stdlib.h> declares dl* extern; mob then ignores its own static dl* stubs and they clash with libc.c's. Hide them. */
#ifndef _COMPAT_STDLIB_H
#define _COMPAT_STDLIB_H
#define dlopen kronos_dlopen
#define dlclose kronos_dlclose
#define dlsym kronos_dlsym
#define dlerror kronos_dlerror
#include_next <stdlib.h>
#undef dlopen
#undef dlclose
#undef dlsym
#undef dlerror
char *realpath(const char *path, char *buf);
/* tcc6 keeps only a0 of a long double call result, so tcc7 (built by
   tcc6) gets tccpp.c's strtold() through a pointer.  The macro also
   turns tcc.h's own strtold prototype into this one. */
long double *compat_strtold(const char *s, char **end);
#define strtold(s, end) (*compat_strtold(s, end))
#endif
