#ifndef _STDARG_H
#define _STDARG_H

/* RV64: tcc saves unused a-regs; __builtin_va_start points at that save area. */
typedef char *va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) (*(type *)((ap += 8) - 8))
#define va_end(ap) ((void)0)
#define va_copy(dst, src) ((dst) = (src))
typedef va_list __gnuc_va_list;

#endif
