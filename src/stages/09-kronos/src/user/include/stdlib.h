#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

void *malloc(size_t n);
void *calloc(size_t a, size_t b);
void *realloc(void *p, size_t n);
void free(void *p);
void exit(int n);
void abort(void);
int atoi(const char *s);
long strtol(const char *s, char **end, int base);
unsigned long strtoul(const char *s, char **end, int base);
long long strtoll(const char *s, char **end, int base);
unsigned long long strtoull(const char *s, char **end, int base);
double strtod(const char *s, char **end);
float strtof(const char *s, char **end);
long double strtold(const char *s, char **end);
double atof(const char *s);
char *getenv(const char *name);
int abs(int n);
long labs(long n);
void qsort(void *base, size_t nmemb, size_t size, int (*cmp)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t n, size_t sz,
              int (*cmp)(const void *, const void *));
int system(const char *cmd);
int atexit(void (*fn)(void));
int rand(void);
void srand(unsigned s);

void *dlopen(const char *filename, int flag);
void dlclose(void *p);
void *dlsym(void *handle, const char *symbol);
const char *dlerror(void);

#endif
