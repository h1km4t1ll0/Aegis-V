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

bool
wrapup_global_declaration_2 (tree decl)
{
  if (TREE_ASM_WRITTEN (decl) || DECL_EXTERNAL (decl))
    return false;

  if (TREE_CODE (decl) == VAR_DECL && TREE_STATIC (decl))
    {
      struct cgraph_varpool_node *node;
      bool needed = 1;
      node = cgraph_varpool_node (decl);

      if (flag_unit_at_a_time && node->finalized)
	needed = 0;
      else if (node->alias)
	needed = 0;
      else if ((flag_unit_at_a_time && !cgraph_global_info_ready)
	       && (TREE_USED (decl)
		   || TREE_USED (DECL_ASSEMBLER_NAME (decl))))
	;
      else if (node->needed)
	;
      else if (DECL_COMDAT (decl))
	needed = 0;
      else if (TREE_READONLY (decl) && !TREE_PUBLIC (decl)
	       && (optimize || !flag_keep_static_consts
		   || DECL_ARTIFICIAL (decl)))
	needed = 0;

      if (needed)
	{
	  rest_of_decl_compilation (decl, 1, 1);
	  return true;
	}
    }

  return false;
}
