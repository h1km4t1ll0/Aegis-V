#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "langhooks.h"

const char *
probe_lhook(tree decl)
{
    return lang_hooks.decl_printable_name(decl, 2);
}
