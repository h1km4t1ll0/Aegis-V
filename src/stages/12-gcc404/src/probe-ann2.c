#include "config.h"
#undef FLOAT
#undef FFS
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "rtl.h"
#include "toplev.h"
#include "langhooks.h"
#include "diagnostic.h"

extern int quiet_flag;
int rtl_dump_and_exit;

void
probe_ann2(tree decl)
{
    if (!quiet_flag)
    {
        if (rtl_dump_and_exit)
            verbatim("%s ", IDENTIFIER_POINTER(DECL_NAME(decl)));
        else
            verbatim(" %s", lang_hooks.decl_printable_name(decl, 2));
        fflush(stderr);
    }
}
