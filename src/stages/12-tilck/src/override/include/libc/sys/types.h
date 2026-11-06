/* Aegis-V: minimal libc header for building the Tilck kernel with tcc7
 * (Tilck's toolchain is musl; types and layouts as musl 1.1.24 riscv64,
 * constants from the Linux uapi headers where they exist). */
#pragma once
#include <stddef.h>
#include <stdint.h>

typedef long ssize_t;
typedef long off_t;
typedef int pid_t;
typedef unsigned uid_t;
typedef unsigned gid_t;
typedef unsigned id_t;
typedef unsigned mode_t;
typedef unsigned long dev_t;
typedef unsigned long ino_t;
typedef unsigned int nlink_t;
typedef int blksize_t;
typedef long blkcnt_t;
typedef long time_t;
typedef long suseconds_t;
typedef long clock_t;
typedef int clockid_t;
typedef int key_t;
typedef void *timer_t;
typedef unsigned useconds_t;
typedef unsigned long fsblkcnt_t;
typedef unsigned long fsfilcnt_t;
typedef long register_t;

typedef unsigned char u_char;
typedef unsigned short u_short, ushort;
typedef unsigned u_int, uint;
typedef unsigned long u_long, ulong;
typedef long long quad_t;
typedef unsigned long long u_quad_t;
typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;

#include <sys/select.h>
