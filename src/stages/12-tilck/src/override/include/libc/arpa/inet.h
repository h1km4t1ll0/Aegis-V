/* Aegis-V: minimal libc header for building the Tilck kernel with tcc7
 * (Tilck's toolchain is musl; types and layouts as musl 1.1.24 riscv64,
 * constants from the Linux uapi headers where they exist). */
#pragma once
#include <stdint.h>

typedef uint32_t in_addr_t;
typedef uint16_t in_port_t;
struct in_addr { in_addr_t s_addr; };

static inline uint16_t htons(uint16_t n) { return (uint16_t)(n << 8 | n >> 8); }
static inline uint16_t ntohs(uint16_t n) { return (uint16_t)(n << 8 | n >> 8); }
static inline uint32_t htonl(uint32_t n)
{
   return n >> 24 | (n >> 8 & 0xff00) | (n << 8 & 0xff0000) | n << 24;
}
static inline uint32_t ntohl(uint32_t n) { return htonl(n); }
