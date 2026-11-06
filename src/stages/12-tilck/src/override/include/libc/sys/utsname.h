/* Aegis-V: minimal libc header for building the Tilck kernel with tcc7
 * (Tilck's toolchain is musl; types and layouts as musl 1.1.24 riscv64,
 * constants from the Linux uapi headers where they exist). */
#pragma once
struct utsname {
   char sysname[65];
   char nodename[65];
   char release[65];
   char version[65];
   char machine[65];
   char domainname[65];
};
