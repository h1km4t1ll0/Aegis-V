#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"

const char *
probe_declname(tree decl)
{
    return IDENTIFIER_POINTER(DECL_NAME(decl));
}
