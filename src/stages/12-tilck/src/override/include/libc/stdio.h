/* Aegis-V: minimal libc header for building the Tilck kernel with tcc7
 * (Tilck's toolchain is musl; types and layouts as musl 1.1.24 riscv64,
 * constants from the Linux uapi headers where they exist). */
#pragma once
#include <stddef.h>
#include <stdarg.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define EOF (-1)

int printf(const char *, ...);
int vprintf(const char *, va_list);
int sprintf(char *, const char *, ...);
int snprintf(char *, size_t, const char *, ...);
int vsnprintf(char *, size_t, const char *, va_list);
