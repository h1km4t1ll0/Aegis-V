/* Stubs for Haifa scheduler symbols when the md has no DFA
   (INSN_SCHEDULING is unset). Not a stub cc1.
   Pointer params are void * so guest cc1 can compile this file
   (rtx in the signature made first cc1 trap). ABI matches: rtx is a pointer. */
struct haifa_insn_data {
  int dummy;
};
struct haifa_insn_data *h_i_d;

int
haifa_classify_insn (void *insn)
{
  (void) insn;
  return 0;
}

void
get_block_head_tail (int n, void **h, void **t)
{
  (void) n;
  if (h)
    *h = 0;
  if (t)
    *t = 0;
}

int
no_real_insns_p (void *a, void *b)
{
  (void) a;
  (void) b;
  return 1;
}

void
ready_add (void *ready, void *insn)
{
  (void) ready;
  (void) insn;
}

int
insn_cost (void *insn, void *link, void *used)
{
  (void) insn;
  (void) link;
  (void) used;
  return 1;
}

void
rm_line_notes (void *head, void *tail)
{
  (void) head;
  (void) tail;
}

void
save_line_notes (int n, void *head, void *tail)
{
  (void) n;
  (void) head;
  (void) tail;
}

void
restore_line_notes (void *head, void *tail)
{
  (void) head;
  (void) tail;
}

void
rm_redundant_line_notes (void)
{
}

void
rm_other_notes (void *head, void *tail)
{
  (void) head;
  (void) tail;
}

int
set_priorities (void *head, void *tail)
{
  (void) head;
  (void) tail;
  return 0;
}

void
schedule_block (int b, int rgn_n_insns)
{
  (void) b;
  (void) rgn_n_insns;
}

void
sched_init (void *dump)
{
  (void) dump;
}

void
sched_finish (void)
{
}
