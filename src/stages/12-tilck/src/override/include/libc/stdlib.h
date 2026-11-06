/* Aegis-V: minimal libc header for building the Tilck kernel with tcc7
 * (Tilck's toolchain is musl; types and layouts as musl 1.1.24 riscv64,
 * constants from the Linux uapi headers where they exist). */
#pragma once
#include <stddef.h>

long strtol(const char *, char **, int);
unsigned long strtoul(const char *, char **, int);
void *malloc(size_t);
void *calloc(size_t, size_t);
void *realloc(void *, size_t);
void free(void *);
_Noreturn void abort(void);
_Noreturn void exit(int);
