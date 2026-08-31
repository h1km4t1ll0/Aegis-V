#include "config.h"
#undef FLOAT
#undef FFS
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include <signal.h>
#include "line-map.h"
#include "input.h"
#include "tree.h"
#include "version.h"
#include "rtl.h"
#include "tm_p.h"
#include "flags.h"
#include "insn-attr.h"
#include "insn-config.h"
#include "insn-flags.h"
#include "hard-reg-set.h"
#include "recog.h"
#include "output.h"
#include "except.h"
#include "function.h"
#include "toplev.h"
#include "expr.h"
#include "basic-block.h"
#include "intl.h"
#include "ggc.h"
#include "graph.h"
#include "regs.h"
#include "timevar.h"
#include "diagnostic.h"
#include "params.h"
#include "reload.h"
#include "dwarf2asm.h"
#include "integrate.h"
#include "real.h"
#include "debug.h"
#include "target.h"
#include "langhooks.h"
#include "cfglayout.h"
#include "cfgloop.h"
#include "hosthooks.h"
#include "cgraph.h"
#include "opts.h"
#include "coverage.h"
#include "value-prof.h"
#include "alloc-pool.h"
#include "tree-mudflap.h"

extern int set_yydebug;

static void
compile_file (void)
{
  init_cgraph ();
  init_final (main_input_filename);
  coverage_init (aux_base_name);

  timevar_push (TV_PARSE);
  lang_hooks.parse_file (set_yydebug);
  lang_hooks.clear_binding_stack ();
  timevar_pop (TV_PARSE);

  if (flag_syntax_only || errorcount || sorrycount)
    return;

  lang_hooks.decls.final_write_globals ();
  cgraph_varpool_assemble_pending_decls ();
  finish_aliases_2 ();
  coverage_finish ();
  if (flag_mudflap)
    mudflap_finish_file ();
  weak_finish ();
  timevar_push (TV_SYMOUT);
  (*debug_hooks->finish) (main_input_filename);
  timevar_pop (TV_SYMOUT);
  dw2_output_indirect_constants ();
  process_pending_assemble_externals ();
}
