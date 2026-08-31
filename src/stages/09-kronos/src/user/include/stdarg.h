#ifndef _STDARG_H
#define _STDARG_H

/* gcc 4.0.4 cc1 rejects char* as __builtin_va_start's ap. tcc is __GNUC__ 2. */
#if defined(__GNUC__) && __GNUC__ >= 4
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)
#define va_copy(dst, src) __builtin_va_copy(dst, src)
#else
typedef char *va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) (*(type *)((ap += 8) - 8))
#define va_end(ap) ((void)0)
#define va_copy(dst, src) ((dst) = (src))
#endif
typedef va_list __gnuc_va_list;

#endif
