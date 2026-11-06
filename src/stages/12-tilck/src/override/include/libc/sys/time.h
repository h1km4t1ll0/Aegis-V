/* Aegis-V: minimal libc header for building the Tilck kernel with tcc7
 * (Tilck's toolchain is musl; types and layouts as musl 1.1.24 riscv64,
 * constants from the Linux uapi headers where they exist). */
#pragma once
#include <sys/types.h>
#include <sys/select.h>

struct timeval { time_t tv_sec; suseconds_t tv_usec; };

struct timezone {
   int tz_minuteswest;
   int tz_dsttime;
};

struct itimerval {
   struct timeval it_interval;
   struct timeval it_value;
};

#define ITIMER_REAL    0
#define ITIMER_VIRTUAL 1
#define ITIMER_PROF    2
