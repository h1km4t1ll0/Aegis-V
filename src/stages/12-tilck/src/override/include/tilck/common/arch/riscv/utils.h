/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once
#include <tilck/common/basic_defs.h>

#ifndef __riscv
   #error This header can be used only when building for riscv.
#endif

#ifdef __TINYC__

/* tcc binds a local register variable only for asm operands: a plain read
   of "register ... __asm__("sp")" reads an uninitialized stack slot. */
static ALWAYS_INLINE ulong get_stack_ptr(void)
{
   ulong current_sp;

   __asm__ volatile ("mv %0, sp" : "=r"(current_sp));
   return current_sp;
}

#else

static ALWAYS_INLINE ulong get_stack_ptr(void)
{
   register ulong current_sp __asm__ ("sp");

   return current_sp;
}

#endif
