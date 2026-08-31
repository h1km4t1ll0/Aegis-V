/* Host config for gcc 4.0.4 cc1 on Kronos. No configure. */
#ifndef GCC_AUTO_HOST_H
#define GCC_AUTO_HOST_H

#define HAVE_STDDEF_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_UNISTD_H 1
#define HAVE_FCNTL_H 1
#define HAVE_LIMITS_H 1
#define HAVE_TIME_H 1
#define HAVE_STDBOOL_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_ERRNO_H 1
#define HAVE_LONG_LONG 1
#define SIZEOF_INT 4
#define SIZEOF_SHORT 2
#define SIZEOF_LONG 8
#define SIZEOF_LONG_LONG 8
#define SIZEOF_VOID_P 8
#ifndef HOST_WIDE_INT
#define HOST_WIDE_INT long
#endif
#ifndef HAVE_UCHAR
#define HAVE_UCHAR 1
typedef unsigned char uchar;
#endif
#define BYTEORDER 1234
#define HAVE_DECL_ASPRINTF 0
#define HAVE_DECL_VASPRINTF 0
#define HAVE_DECL_BASENAME 1
#define HAVE_DECL_GETOPT 0
#define HAVE_DECL_SNPRINTF 1
#define HAVE_DECL_VSNPRINTF 1
#define HAVE_DECL_STRSTR 1
#define HAVE_DECL_ERRNO 1
#define HAVE_DECL_ABORT 1
#define HAVE_DECL_FPRINTF 1
#define HAVE_DECL_STRERROR 1
#define HAVE_DECL_MALLOC 1
#define HAVE_DECL_CALLOC 1
#define HAVE_DECL_REALLOC 1
#define HAVE_DECL_FREE 1
#define HAVE_DECL_GETCWD 1
#define HAVE_DECL_GETENV 1
#define HAVE_DECL_ATOF 1
#define HAVE_DECL_ATOL 1
#define HAVE_DECL_SBRK 1
#define HAVE_DECL_STRSIGNAL 0
#define HAVE_DECL_CLOCK 0
#define HAVE_DECL_GETRLIMIT 0
#define HAVE_DECL_GETRUSAGE 0
#define HAVE_DECL_TIMES 0
#define HAVE_DECL_PUTC_UNLOCKED 0
#define HAVE_DECL_FPUTC_UNLOCKED 0
#define HAVE_DECL_FPUTS_UNLOCKED 0
#define HAVE_DECL_FWRITE_UNLOCKED 0
#define HAVE_DECL_FPRINTF_UNLOCKED 0
#define HAVE_STRERROR 1
#define HAVE_STRSTR 1
#define HAVE_UNAME 1
#define USE_C_ALLOCA 1
#define C_ALLOCA 1
#define IN_GCC 1
#define SUCCESS_EXIT_CODE 0
#define FATAL_EXIT_CODE 1
#define HAVE_SNPRINTF 1
#define HAVE_VSNPRINTF 1
#define HAVE_CLOCK_T 1
#define HAVE_STRDUP 1
#define HAVE_GETCWD 1
#define HAVE_GETENV 1
#define PACKAGE_NAME "gcc"
#define PACKAGE_VERSION "4.0.4"
#define PACKAGE_STRING "gcc 4.0.4"
#define PACKAGE_BUGREPORT ""
#define PACKAGE_TARNAME "gcc"
#define PREFIX "/usr"
#define LOCALEDIR "/usr/share/locale"
#define STANDARD_INCLUDE_DIR "/include"
#define STANDARD_INCLUDE_COMPONENT 0
#define GCC_INCLUDE_DIR "/include"
#define GPLUSPLUS_INCLUDE_DIR "/include"
#define GPLUSPLUS_TOOL_INCLUDE_DIR "/include"
#define GPLUSPLUS_BACK_INCLUDE_DIR "/include"
#define CROSS_INCLUDE_DIR "/include"
#define TOOL_INCLUDE_DIR "/include"
#define LOCAL_INCLUDE_DIR "/include"
#define SYSTEM_INCLUDE_DIR 0
#define STANDARD_EXEC_PREFIX "/bin/"
#define STANDARD_LIBEXEC_PREFIX "/bin/"
#define STANDARD_BINDIR_PREFIX "/bin/"
#define STANDARD_STARTFILE_PREFIX "/lib/"
#define STANDARD_STARTFILE_PREFIX_1 "/lib/"
#define STANDARD_STARTFILE_PREFIX_2 "/lib/"
#define EXEC_PREFIX "/bin/"
#define TOOLDIR_BASE_PREFIX "/bin/"
#define DEFAULT_TARGET_MACHINE "riscv64-kronos"
#define DEFAULT_TARGET_VERSION "4.0.4"
#define HOST_MACHINE "riscv64-kronos"
#define TARGET_MACHINE "riscv64-kronos"
#define TARGET_NAME "riscv64-kronos"
#define HAVE_GAS_SHF_MERGE 0
#define HAVE_GAS_COMDAT_GROUP 0
#define CONFIGURE_SPECS ""

#endif
