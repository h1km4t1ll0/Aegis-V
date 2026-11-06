/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * Aegis-V: C version of Tilck's common/itoa.cpp (tcc has no C++). The
 * templates __uitoa_fixed<T>, __itoa<T> and __tilck_strtol<T> become
 * macros that define one static function per type; the extern "C"
 * functions keep their names and signatures.
 */

#include <tilck/common/basic_defs.h>
#include <tilck/common/assert.h>
#include <tilck/common/string_util.h>
#include <tilck/kernel/errno.h>
#include <tilck/kernel/test/itoa.h>

extern const s8 digit_to_val[128];

#define DIGITS "0123456789abcdef"

#define DEF_UITOA_FIXED(name, T)                                          \
static void __uitoa_fixed_##name(T value, char *buf)                      \
{                                                                         \
   u32 j = sizeof(value) * 8 - 4;                                         \
   char *ptr = buf;                                                       \
                                                                          \
   for (u32 i = 0; i < sizeof(value) * 2; i++, j-=4) {                    \
      *ptr++ = DIGITS[(value >> j) & 0xf];                                \
   }                                                                      \
                                                                          \
   *ptr = 0;                                                              \
}

/* U: unsigned type of T (same as T, if T is unsigned) */
#define DEF_ITOA(name, T, U)                                              \
static void __itoa_##name(T svalue, char *buf, int base)                  \
{                                                                         \
   ASSERT(IN_RANGE_INC(base, 2, 16));                                     \
                                                                          \
   char *ptr = buf;                                                       \
                                                                          \
   if (svalue == 0) {                                                     \
      *ptr++ = DIGITS[0];                                                 \
      *ptr = 0;                                                           \
      return;                                                             \
   }                                                                      \
                                                                          \
   U value = svalue >= 0 ? (U) svalue : (U) -svalue;                      \
                                                                          \
   while (value) {                                                        \
      *ptr++ = DIGITS[value % (U)base];                                   \
      value /= (U)base;                                                   \
   }                                                                      \
                                                                          \
   if (svalue < 0)                                                        \
      *ptr++ = '-';                                                       \
                                                                          \
   *ptr = 0;                                                              \
   str_reverse(buf, (size_t)ptr - (size_t)buf);                           \
}

static inline bool is_valid_digit(u8 d, int base)
{
   return d < 128 && IN_RANGE(digit_to_val[d], 0, base);
}

/* is_uns: is_unsigned<T>::val */
#define DEF_STRTOL(name, T, is_uns)                                       \
static T                                                                  \
__tilck_strtol_##name(const char *str, const char **endptr,               \
                      int base, int *error)                               \
{                                                                         \
   T next, res = 0, sign = 1;                                             \
   const char *p;                                                         \
   bool overflow;                                                         \
                                                                          \
   ASSERT(IN_RANGE_INC(base, 2, 16));                                     \
                                                                          \
   if (error)                                                             \
      *error = 0;                                                         \
                                                                          \
   if (!(is_uns)) {                                                       \
      if (*str == '-') {                                                  \
         sign = (T) -1;                                                   \
         str++;                                                           \
      }                                                                   \
   }                                                                      \
                                                                          \
   for (p = str; *p; p++) {                                               \
                                                                          \
      u8 up = (u8)*p;                                                     \
                                                                          \
      if (!is_valid_digit(up, base))                                      \
         break;                                                           \
                                                                          \
      if (is_uns) {                                                       \
         next = res * (T)base + (T)digit_to_val[up];                      \
         overflow = next < res;                                           \
      } else {                                                            \
         next = res * (T)base + sign * (T)digit_to_val[up];               \
         overflow = (sign > 0) != (next >= 0);                            \
      }                                                                   \
                                                                          \
      if (overflow) {                                                     \
                                                                          \
         if (error)                                                       \
            *error = -ERANGE;                                             \
                                                                          \
         if (endptr)                                                      \
            *endptr = str;                                                \
                                                                          \
         return 0; /* signed int overflow */                              \
      }                                                                   \
                                                                          \
      res = next;                                                         \
   }                                                                      \
                                                                          \
   if (p == str && error)                                                 \
      *error = -EINVAL;                                                   \
                                                                          \
   if (endptr)                                                            \
      *endptr = p;                                                        \
                                                                          \
   return res;                                                            \
}

DEF_UITOA_FIXED(u32, u32)
DEF_UITOA_FIXED(u64, u64)
DEF_UITOA_FIXED(ulong, ulong)

DEF_ITOA(s32, s32, u32)
DEF_ITOA(s64, s64, u64)
DEF_ITOA(long, long, ulong)
DEF_ITOA(u32, u32, u32)
DEF_ITOA(u64, u64, u64)
DEF_ITOA(ulong, ulong, ulong)

DEF_STRTOL(long, long, 0)

long tilck_strtol(const char *s, const char **endptr, int base, int *err)
{
   return __tilck_strtol_long(s, endptr, base, err);
}

void uitoa32_hex_fixed(u32 value, char *buf)
{
   __uitoa_fixed_u32(value, buf);
}

void uitoa64_hex_fixed(u64 value, char *buf)
{
   __uitoa_fixed_u64(value, buf);
}

void uitoaN_hex_fixed(ulong value, char *buf)
{
   __uitoa_fixed_ulong(value, buf);
}

void itoa32(s32 value, char *buf)
{
   __itoa_s32(value, buf, 10);
}

void itoa64(s64 value, char *buf)
{
   __itoa_s64(value, buf, 10);
}

void itoaN(long value, char *buf)
{
   __itoa_long(value, buf, 10);
}

void uitoa32(u32 value, char *buf, int base)
{
   __itoa_u32(value, buf, base);
}

void uitoa64(u64 value, char *buf, int base)
{
   __itoa_u64(value, buf, base);
}

void uitoaN(ulong value, char *buf, int base)
{
   __itoa_ulong(value, buf, base);
}

#ifdef __TILCK_KERNEL__

/*
 * No other reason for this #ifdef except to avoid unnecessary code-bloat
 * in the legacy bootloader.
 */

DEF_STRTOL(ulong, ulong, 1)

ulong tilck_strtoul(const char *s, const char **endptr, int base, int *err) {
   return __tilck_strtol_ulong(s, endptr, base, err);
}

#ifdef KERNEL_TEST
DEF_STRTOL(s32, s32, 0)
DEF_STRTOL(u32, u32, 1)
DEF_STRTOL(s64, s64, 0)
DEF_STRTOL(u64, u64, 1)
s32 tilck_strtol32(const char *s, const char **endptr, int base, int *err) {
   return __tilck_strtol_s32(s, endptr, base, err);
}
u32 tilck_strtoul32(const char *s, const char **endptr, int base, int *err) {
   return __tilck_strtol_u32(s, endptr, base, err);
}
s64 tilck_strtol64(const char *s, const char **endptr, int base, int *err) {
   return __tilck_strtol_s64(s, endptr, base, err);
}
u64 tilck_strtoul64(const char *s, const char **endptr, int base, int *err) {
   return __tilck_strtol_u64(s, endptr, base, err);
}
#endif // #ifdef KERNEL_TEST
#endif // #ifdef __TILCK_KERNEL__

#if defined(__TILCK_KERNEL__) && !defined(KERNEL_TEST)

long strtol(const char *s, char **endptr, int base) {
   return tilck_strtol(s, (const char **)endptr, base, NULL);
}

ulong strtoul(const char *s, char **endptr, int base) {
   return tilck_strtoul(s, (const char **)endptr, base, NULL);
}

#endif
