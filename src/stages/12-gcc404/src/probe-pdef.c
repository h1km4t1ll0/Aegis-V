#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "params.h"

static const param_info lang_independent_params[] = {
#define DEFPARAM(ENUM, OPTION, HELP, DEFAULT, MIN, MAX) \
  { OPTION, DEFAULT, MIN, MAX, HELP },
#include "params.def"
#undef DEFPARAM
  { 0, 0, 0, 0, 0 }
};

int
probe_pdef(void)
{
    return lang_independent_params[0].value;
}
