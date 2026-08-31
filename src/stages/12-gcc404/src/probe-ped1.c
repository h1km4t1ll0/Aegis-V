#include "config.h"
#undef FLOAT
#undef FFS
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "toplev.h"
#include "diagnostic.h"

void
probe_ped_plain (void)
{
  pedwarn ("%s", "x");
}
