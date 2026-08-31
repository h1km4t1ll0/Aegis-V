/* Minimal system.h for gcc 4.0.4 generators on Kronos/tcc. Not the 4.0.4 system.h. */
#ifndef GCC_SYSTEM_H
#define GCC_SYSTEM_H

#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef BUFSIZ
#define BUFSIZ 1024
#endif
#include "ansidecl.h"
#include "libiberty.h"
#include "safe-ctype.h"
#include "hwint.h"
#ifndef HAVE_UCHAR
typedef unsigned char uchar;
#define HAVE_UCHAR 1
#endif

#ifndef SUCCESS_EXIT_CODE
#define SUCCESS_EXIT_CODE 0
#endif
#ifndef FATAL_EXIT_CODE
#define FATAL_EXIT_CODE 1
#endif

#define gcc_assert(EXPR) ((void)(0 && (EXPR)))
#define gcc_unreachable() abort()
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#define DIR_SEPARATOR '/'
#define IS_DIR_SEPARATOR(c) ((c) == '/')
#define IS_ABSOLUTE_PATH(f) ((f) && (f)[0] == '/')
#ifndef PATH_SEPARATOR
#define PATH_SEPARATOR ':'
#endif
#ifndef HOST_BIT_BUCKET
#define HOST_BIT_BUCKET "/dev/null"
#endif
#define HOST_PTR_PRINTF "%p"
#define putc fputc
#define atol(s) strtol((s), (char **)0, 10)
#define ENUM_BITFIELD(TYPE) unsigned int
#define BOOL_BITFIELD unsigned int
#define IN_RANGE(VALUE, LOWER, UPPER) \
  ((unsigned HOST_WIDE_INT)((VALUE) - (LOWER)) <= ((UPPER) - (LOWER)))
#define really_call_malloc malloc
#define really_call_calloc calloc
#define really_call_realloc realloc
#define obstack_chunk_alloc ((void *(*) (long)) xmalloc)
#define obstack_chunk_free ((void (*) (void *)) free)

/* ansidecl maps CONST to const; rtl.def uses CONST as an rtx code. */
#undef CONST
#undef PC
#undef FFS
#undef FLOAT
#undef ABS

#ifndef __builtin_expect
#define __builtin_expect(e, c) (e)
#endif

#ifndef MAX
#define MAX(A, B) (((A) > (B)) ? (A) : (B))
#endif
#ifndef MIN
#define MIN(A, B) (((A) < (B)) ? (A) : (B))
#endif
#ifndef CEIL
#define CEIL(x,y) (((x) + (y) - 1) / (y))
#endif

extern FILE *asm_out_file;

#endif
