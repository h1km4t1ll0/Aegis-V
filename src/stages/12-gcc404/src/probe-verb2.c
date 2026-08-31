#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "toplev.h"
#include "diagnostic.h"

void
probe_verb2(tree decl)
{
    verbatim("%s ", IDENTIFIER_POINTER(DECL_NAME(decl)));
}
