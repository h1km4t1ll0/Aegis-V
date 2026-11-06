/* tccrun.c reads environ; Kronos <unistd.h> lacks it. Defined in compat.c. */
#ifndef _COMPAT_UNISTD_H
#define _COMPAT_UNISTD_H
#include_next <unistd.h>
extern char **environ;
#endif
