#include "config.h"
#undef FLOAT
#undef FFS
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "rtl.h"
#include "toplev.h"

static const char *src_pwd;

bool
set_src_pwd (const char *pwd)
{
  if (src_pwd)
    {
      if (strcmp (src_pwd, pwd) == 0)
	return 1;
      else
	return 0;
    }

  src_pwd = xstrdup (pwd);
  return 1;
}

const char *
get_src_pwd (void)
{
  if (! src_pwd)
    {
      src_pwd = getpwd ();
      if (!src_pwd)
	src_pwd = ".";
    }
  return src_pwd;
}
