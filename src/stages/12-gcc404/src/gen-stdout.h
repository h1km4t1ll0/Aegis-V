/* Kronos has no shell redirect: last non-flag argv is the output path. */
#ifndef GEN_STDOUT_H
#define GEN_STDOUT_H

static int
gen_open_stdout(int *pargc, char ***pargv)
{
  int argc;
  char **argv;
  FILE *out;

  argc = *pargc;
  argv = *pargv;
  if (argc >= 2 && argv[argc - 1][0] != '-')
    {
      out = fopen(argv[argc - 1], "w");
      if (!out)
	{
	  perror(argv[argc - 1]);
	  return 0;
	}
      stdout = out;
      argv[--argc] = 0;
      *pargc = argc;
    }
  return 1;
}

#endif
