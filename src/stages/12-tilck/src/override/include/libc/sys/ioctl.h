/* Aegis-V: minimal libc header for building the Tilck kernel with tcc7
 * (Tilck's toolchain is musl; types and layouts as musl 1.1.24 riscv64,
 * constants from the Linux uapi headers where they exist). */
#pragma once
#include <asm/ioctls.h>

struct winsize {
   unsigned short ws_row;
   unsigned short ws_col;
   unsigned short ws_xpixel;
   unsigned short ws_ypixel;
};
