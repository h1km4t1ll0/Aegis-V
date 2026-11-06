/* Aegis-V: config.h for make 3.82 on musl (live-bootstrap creates an empty
   one for mes libc). Without these, make.h declares `char *alloca ()` and
   `int strncasecmp (..., int)` against musl's prototypes, and string.h,
   stdlib.h, unistd.h are not included (implicit `int` for pointer-returning
   functions such as strchr, cut to 32 bits on riscv64). */
#define STDC_HEADERS 1
#define HAVE_STRING_H 1
#define HAVE_UNISTD_H 1
#define HAVE_LIMITS_H 1
#define HAVE_ALLOCA_H 1
#define HAVE_STRCASECMP 1
#define HAVE_STRNCASECMP 1
