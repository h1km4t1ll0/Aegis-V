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

int
wrap2c (tree decl)
{
  if (TREE_USED (decl) || TREE_USED (DECL_ASSEMBLER_NAME (decl)))
    return 1;
  if (DECL_COMDAT (decl))
    return 0;
  if (TREE_READONLY (decl) && !TREE_PUBLIC (decl)
      && (optimize || !flag_keep_static_consts || DECL_ARTIFICIAL (decl)))
    return 0;
  rest_of_decl_compilation (decl, 1, 1);
  return 1;
}
