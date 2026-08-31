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

void
check_global_declaration_1 (tree decl)
{
  if (TREE_CODE (decl) == FUNCTION_DECL
      && DECL_INITIAL (decl) == 0
      && DECL_EXTERNAL (decl)
      && ! DECL_ARTIFICIAL (decl)
      && ! TREE_NO_WARNING (decl)
      && ! TREE_PUBLIC (decl)
      && (warn_unused_function
          || TREE_SYMBOL_REFERENCED (DECL_ASSEMBLER_NAME (decl))))
    {
      if (TREE_SYMBOL_REFERENCED (DECL_ASSEMBLER_NAME (decl)))
	pedwarn ("%J%qF used but never defined", decl, decl);
      else
	warning ("%J%qF declared %<static%> but never defined", decl, decl);
      TREE_PUBLIC (decl) = 1;
      assemble_external (decl);
    }

  if (((warn_unused_function && TREE_CODE (decl) == FUNCTION_DECL)
       || (warn_unused_variable
	   && TREE_CODE (decl) == VAR_DECL && ! TREE_READONLY (decl)))
      && ! DECL_IN_SYSTEM_HEADER (decl)
      && ! TREE_USED (decl)
      && ! (DECL_NAME (decl) && TREE_USED (DECL_NAME (decl)))
      && ! DECL_EXTERNAL (decl)
      && ! TREE_PUBLIC (decl)
      && ! TREE_THIS_VOLATILE (decl)
      && ! (TREE_CODE (decl) == VAR_DECL && DECL_REGISTER (decl))
      && lang_hooks.decls.warn_unused_global (decl))
    warning ("%J%qD defined but not used", decl, decl);
}
