#ifndef _STDARG_H
#define _STDARG_H

typedef char *va_list;
#define va_start(ap, last) ((ap) = (char *)(&(last) + 1))
#define va_arg(ap, type) (*(type *)(((ap) += ((sizeof(type) + 7) & ~7)) - ((sizeof(type) + 7) & ~7)))
#define va_end(ap) ((void)0)
#define va_copy(dst, src) ((dst) = (src))
typedef va_list __gnuc_va_list;

#endif
