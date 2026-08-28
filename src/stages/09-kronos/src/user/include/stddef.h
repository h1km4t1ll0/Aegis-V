#ifndef _STDDEF_H
#define _STDDEF_H

typedef long size_t;
typedef long ptrdiff_t;
typedef long wchar_t;
#define NULL ((void *)0)
#ifndef offsetof
#define offsetof(t, m) ((size_t)&((t *)0)->m)
#endif

#endif
