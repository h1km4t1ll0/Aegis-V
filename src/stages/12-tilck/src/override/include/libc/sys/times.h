/* Aegis-V: minimal libc header for building the Tilck kernel with tcc7
 * (Tilck's toolchain is musl; types and layouts as musl 1.1.24 riscv64,
 * constants from the Linux uapi headers where they exist). */
#pragma once
#include <sys/types.h>

struct tms {
   clock_t tms_utime;
   clock_t tms_stime;
   clock_t tms_cutime;
   clock_t tms_cstime;
};
