#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "toplev.h"
#include "diagnostic.h"

void
probe_dc(void)
{
    pp_needs_newline(global_dc->printer) = 1;
    diagnostic_set_last_function(global_dc);
}
