/* Stub DWARF2 writer: Kronos cc1 uses NO_DEBUG. Real dwarf2out.c is too
   large for tcc6's heap in one process. */
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "rtl.h"
#include "debug.h"

int
dwarf2out_do_frame (void)
{
  return 0;
}

void
dwarf2out_frame_init (void)
{
}

void
dwarf2out_frame_finish (void)
{
}

void
dwarf2out_begin_prologue (unsigned int line, const char *file)
{
  (void) line;
  (void) file;
}

void
dwarf2out_end_epilogue (unsigned int line, const char *file)
{
  (void) line;
  (void) file;
}

void
dwarf2out_frame_debug (rtx insn, bool after_p)
{
  (void) insn;
  (void) after_p;
}

void
dwarf2out_def_cfa (const char *label, unsigned int reg, HOST_WIDE_INT offset)
{
  (void) label;
  (void) reg;
  (void) offset;
}

void
dwarf2out_window_save (const char *label)
{
  (void) label;
}

void
dwarf2out_args_size (const char *label, HOST_WIDE_INT size)
{
  (void) label;
  (void) size;
}

void
dwarf2out_reg_save (const char *label, unsigned int reg, HOST_WIDE_INT offset)
{
  (void) label;
  (void) reg;
  (void) offset;
}

void
dwarf2out_return_save (const char *label, HOST_WIDE_INT offset)
{
  (void) label;
  (void) offset;
}

void
dwarf2out_return_reg (const char *label, unsigned int sreg)
{
  (void) label;
  (void) sreg;
}

const struct gcc_debug_hooks dwarf2_debug_hooks;

void
default_eh_frame_section (void)
{
}

void
gt_ggc_mx_die_struct (void *x)
{
  (void) x;
}

void
gt_pch_nx_die_struct (void *x)
{
  (void) x;
}

rtx
expand_builtin_dwarf_sp_column (void)
{
  return const0_rtx;
}

void
expand_builtin_init_dwarf_reg_sizes (tree addr)
{
  (void) addr;
}
