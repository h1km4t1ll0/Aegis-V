/* Aegis-V: minimal libc header for building the Tilck kernel with tcc7
 * (Tilck's toolchain is musl; types and layouts as musl 1.1.24 riscv64,
 * constants from the Linux uapi headers where they exist). */
#pragma once
#include <sys/types.h>

struct iovec { void *iov_base; size_t iov_len; };

#define UIO_MAXIOV 1024
