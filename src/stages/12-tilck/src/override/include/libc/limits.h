/* Aegis-V: minimal libc header for building the Tilck kernel with tcc7
 * (Tilck's toolchain is musl; types and layouts as musl 1.1.24 riscv64,
 * constants from the Linux uapi headers where they exist). */
#pragma once
#include <linux/limits.h>

#define CHAR_BIT   8
#define SCHAR_MIN  (-128)
#define SCHAR_MAX  127
#define UCHAR_MAX  255
#define CHAR_MIN   0
#define CHAR_MAX   255
#define SHRT_MIN   (-1-0x7fff)
#define SHRT_MAX   0x7fff
#define USHRT_MAX  0xffff
#define INT_MIN    (-1-0x7fffffff)
#define INT_MAX    0x7fffffff
#define UINT_MAX   0xffffffffU
#define LONG_MIN   (-LONG_MAX-1)
#define LONG_MAX   0x7fffffffffffffffL
#define ULONG_MAX  (2UL*LONG_MAX+1)
#define LLONG_MIN  (-LLONG_MAX-1)
#define LLONG_MAX  0x7fffffffffffffffLL
#define ULLONG_MAX (2ULL*LLONG_MAX+1)
#define SSIZE_MAX  LONG_MAX
#define IOV_MAX    1024
#define PAGESIZE   4096
