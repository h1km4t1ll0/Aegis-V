/* Aegis-V: minimal libc header for building the Tilck kernel with tcc7
 * (Tilck's toolchain is musl; types and layouts as musl 1.1.24 riscv64,
 * constants from the Linux uapi headers where they exist). */
#pragma once
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long int64_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
typedef long intptr_t;
typedef unsigned long uintptr_t;
typedef long intmax_t;
typedef unsigned long uintmax_t;
typedef int8_t int_least8_t, int_fast8_t;
typedef int16_t int_least16_t;
typedef int32_t int_least32_t, int_fast16_t, int_fast32_t;
typedef int64_t int_least64_t, int_fast64_t;
typedef uint8_t uint_least8_t, uint_fast8_t;
typedef uint16_t uint_least16_t;
typedef uint32_t uint_least32_t, uint_fast16_t, uint_fast32_t;
typedef uint64_t uint_least64_t, uint_fast64_t;

#define INT8_MIN   (-1-0x7f)
#define INT16_MIN  (-1-0x7fff)
#define INT32_MIN  (-1-0x7fffffff)
#define INT64_MIN  (-1-0x7fffffffffffffffL)
#define INT8_MAX   (0x7f)
#define INT16_MAX  (0x7fff)
#define INT32_MAX  (0x7fffffff)
#define INT64_MAX  (0x7fffffffffffffffL)
#define UINT8_MAX  (0xff)
#define UINT16_MAX (0xffff)
#define UINT32_MAX (0xffffffffu)
#define UINT64_MAX (0xffffffffffffffffUL)
#define INTPTR_MIN  INT64_MIN
#define INTPTR_MAX  INT64_MAX
#define UINTPTR_MAX UINT64_MAX
#define INTMAX_MIN  INT64_MIN
#define INTMAX_MAX  INT64_MAX
#define UINTMAX_MAX UINT64_MAX
#define PTRDIFF_MIN INT64_MIN
#define PTRDIFF_MAX INT64_MAX
#define SIZE_MAX    UINT64_MAX

#define INT8_C(c)   c
#define INT16_C(c)  c
#define INT32_C(c)  c
#define INT64_C(c)  c ## L
#define UINT8_C(c)  c
#define UINT16_C(c) c
#define UINT32_C(c) c ## U
#define UINT64_C(c) c ## UL
#define INTMAX_C(c)  c ## L
#define UINTMAX_C(c) c ## UL
