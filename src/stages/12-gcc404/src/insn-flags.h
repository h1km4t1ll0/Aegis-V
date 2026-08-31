/* Stub so rtl.h can include insn-flags.h before genflags runs.
   Guest genflags overwrites /src/gcc/insn-flags.h via extras. */
#ifndef GCC_INSN_FLAGS_H
#define GCC_INSN_FLAGS_H
#define HAVE_nop 1
extern rtx gen_nop (void);
#endif
