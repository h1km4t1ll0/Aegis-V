/* Aegis-V: minimal libc header for building the Tilck kernel with tcc7
 * (Tilck's toolchain is musl; types and layouts as musl 1.1.24 riscv64,
 * constants from the Linux uapi headers where they exist). */
#pragma once
#include <sys/types.h>
#include <time.h>

#define SIGHUP     1
#define SIGINT     2
#define SIGQUIT    3
#define SIGILL     4
#define SIGTRAP    5
#define SIGABRT    6
#define SIGIOT     SIGABRT
#define SIGBUS     7
#define SIGFPE     8
#define SIGKILL    9
#define SIGUSR1   10
#define SIGSEGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGSTKFLT 16
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGURG    23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF   27
#define SIGWINCH  28
#define SIGIO     29
#define SIGPOLL   SIGIO
#define SIGPWR    30
#define SIGSYS    31
#define SIGUNUSED SIGSYS
#define _NSIG     65

/* SA_*, SIG_BLOCK..SIG_SETMASK, SIG_DFL/SIG_IGN/SIG_ERR */
#include <asm-generic/signal-defs.h>
#define SA_RESTORER  0x04000000

#define POLL_IN  1
#define POLL_OUT 2
#define POLL_MSG 3
#define POLL_ERR 4
#define POLL_PRI 5
#define POLL_HUP 6

#define MINSIGSTKSZ 2048
#define SIGSTKSZ 8192

typedef struct __sigset_t { unsigned long __bits[128/sizeof(long)]; } sigset_t;

typedef struct sigaltstack {
   void *ss_sp;
   int ss_flags;
   size_t ss_size;
} stack_t;

union sigval {
   int sival_int;
   void *sival_ptr;
};
