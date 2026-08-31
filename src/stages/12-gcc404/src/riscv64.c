#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "regs.h"
#include "hard-reg-set.h"
#include "real.h"
#include "insn-config.h"
#include "conditions.h"
#include "output.h"
#include "insn-attr.h"
#include "flags.h"
#include "recog.h"
#include "tree.h"
#include "expr.h"
#include "obstack.h"
#include "except.h"
#include "function.h"
#include "toplev.h"
#include "tm_p.h"

int target_flags;

rtx riscv64_compare_op0;
rtx riscv64_compare_op1;
static struct function *riscv64_compare_fun;

void
riscv64_init_expanders(void)
{
  riscv64_compare_op0 = 0;
  riscv64_compare_op1 = 0;
  riscv64_compare_fun = cfun;
}

void
riscv64_save_compare(rtx op0, rtx op1)
{
  riscv64_compare_fun = cfun;
  riscv64_compare_op0 = op0;
  riscv64_compare_op1 = op1;
}

rtx
riscv64_cmp_op0(void)
{
  if (riscv64_compare_fun != cfun || !riscv64_compare_op0)
    return const0_rtx;
  return riscv64_compare_op0;
}

rtx
riscv64_cmp_op1(void)
{
  if (riscv64_compare_fun != cfun || !riscv64_compare_op1)
    return const0_rtx;
  return riscv64_compare_op1;
}

static HOST_WIDE_INT riscv64_compute_frame_size(void);

#define RV_T1_REGNUM 6

static HOST_WIDE_INT
riscv64_compute_frame_size(void)
{
  HOST_WIDE_INT size;
  int regno;

  size = get_frame_size();
  size += current_function_outgoing_args_size;
  size += 16;
  for (regno = 9; regno <= 27; regno++) {
    if (regs_ever_live[regno] && !call_used_regs[regno])
      size += UNITS_PER_WORD;
  }
  size = (size + 15) & ~15;
  if (size < 16)
    size = 16;
  return size;
}

static void
riscv64_emit_sp_add(HOST_WIDE_INT delta)
{
  rtx sp;
  rtx t;

  sp = stack_pointer_rtx;
  if (SMALL_OPERAND(delta)) {
    emit_insn(gen_adddi3(sp, sp, GEN_INT(delta)));
    return;
  }
  t = gen_rtx_REG(Pmode, RV_T1_REGNUM);
  emit_move_insn(t, GEN_INT(delta));
  emit_insn(gen_adddi3(sp, sp, t));
}

static rtx
riscv64_frame_mem(rtx sp, HOST_WIDE_INT off)
{
  rtx addr;
  rtx t;

  if (SMALL_OPERAND(off))
    addr = plus_constant(sp, off);
  else {
    t = gen_rtx_REG(Pmode, RV_T1_REGNUM);
    emit_move_insn(t, GEN_INT(off));
    emit_insn(gen_adddi3(t, sp, t));
    addr = t;
  }
  return gen_rtx_MEM(Pmode, addr);
}

HOST_WIDE_INT
riscv64_initial_elimination_offset(int from, int to)
{
  (void)from;
  (void)to;
  return riscv64_compute_frame_size();
}

void
riscv64_expand_prologue(void)
{
  HOST_WIDE_INT size;
  HOST_WIDE_INT off;
  rtx sp;
  rtx ra;
  rtx fp;
  int regno;

  riscv64_compare_op0 = 0;
  riscv64_compare_op1 = 0;
  size = riscv64_compute_frame_size();
  sp = stack_pointer_rtx;
  ra = gen_rtx_REG(Pmode, RV_RA_REGNUM);
  fp = gen_rtx_REG(Pmode, RV_FP_REGNUM);
  riscv64_emit_sp_add(-size);
  emit_move_insn(riscv64_frame_mem(sp, size - 8), ra);
  emit_move_insn(riscv64_frame_mem(sp, size - 16), fp);
  off = size - 24;
  for (regno = 9; regno <= 27; regno++) {
    if (regs_ever_live[regno] && !call_used_regs[regno]) {
      emit_move_insn(riscv64_frame_mem(sp, off),
		     gen_rtx_REG(Pmode, regno));
      off -= UNITS_PER_WORD;
    }
  }
  if (SMALL_OPERAND(size))
    emit_move_insn(fp, plus_constant(sp, size));
  else {
    emit_move_insn(fp, GEN_INT(size));
    emit_insn(gen_adddi3(fp, sp, fp));
  }
}

void
riscv64_expand_epilogue(void)
{
  HOST_WIDE_INT size;
  HOST_WIDE_INT off;
  rtx sp;
  rtx ra;
  rtx fp;
  int regno;

  size = riscv64_compute_frame_size();
  sp = stack_pointer_rtx;
  ra = gen_rtx_REG(Pmode, RV_RA_REGNUM);
  fp = gen_rtx_REG(Pmode, RV_FP_REGNUM);
  off = size - 24;
  for (regno = 9; regno <= 27; regno++) {
    if (regs_ever_live[regno] && !call_used_regs[regno]) {
      emit_move_insn(gen_rtx_REG(Pmode, regno),
		     riscv64_frame_mem(sp, off));
      off -= UNITS_PER_WORD;
    }
  }
  emit_move_insn(fp, riscv64_frame_mem(sp, size - 16));
  emit_move_insn(ra, riscv64_frame_mem(sp, size - 8));
  riscv64_emit_sp_add(size);
  emit_jump_insn(gen_return());
  riscv64_compare_op0 = 0;
  riscv64_compare_op1 = 0;
}

void
riscv64_initialize_trampoline(rtx tramp, rtx fnaddr, rtx cxt)
{
  emit_move_insn(gen_rtx_MEM(SImode, tramp), GEN_INT(0x00000297));
  emit_move_insn(gen_rtx_MEM(SImode, plus_constant(tramp, 4)),
		 GEN_INT(0x0182B303));
  emit_move_insn(gen_rtx_MEM(SImode, plus_constant(tramp, 8)),
		 GEN_INT(0x0102B283));
  emit_move_insn(gen_rtx_MEM(SImode, plus_constant(tramp, 12)),
		 GEN_INT(0x00030067));
  emit_move_insn(gen_rtx_MEM(DImode, plus_constant(tramp, 16)), cxt);
  emit_move_insn(gen_rtx_MEM(DImode, plus_constant(tramp, 24)), fnaddr);
}

static char *
riscv64_fmt_li32(char *p, const char *rd, unsigned int v32)
{
  unsigned int hi;
  int lo;
  HOST_WIDE_INT v;

  v = (HOST_WIDE_INT)(int)v32;
  if (SMALL_OPERAND(v)) {
    p += sprintf(p, "addi\t%s, zero, " HOST_WIDE_INT_PRINT_DEC, rd, v);
    return p;
  }
  hi = (unsigned int)(((unsigned HOST_WIDE_INT)(HOST_WIDE_INT)(int)v + 0x800)
		      >> 12) & 0xfffff;
  lo = (int)v - (int)(hi << 12);
  p += sprintf(p, "lui\t%s, %u\n\taddi\t%s, %s, %d", rd, hi, rd, rd, lo);
  return p;
}

static const char *
riscv64_output_u64(const char *rd, unsigned HOST_WIDE_INT v)
{
  static char buf[400];
  char *p;
  unsigned int lo32;
  unsigned int hi32;
  const char *tmp;
  HOST_WIDE_INT sv;

  sv = (HOST_WIDE_INT)v;
  if (sv == (HOST_WIDE_INT)(int)sv) {
    riscv64_fmt_li32(buf, rd, (unsigned int)v);
    return buf;
  }
  lo32 = (unsigned int)v;
  hi32 = (unsigned int)(v >> 32);
  tmp = (rd[0] == 't' && rd[1] == '1' && rd[2] == 0) ? "t0" : "t1";
  p = buf;
  p = riscv64_fmt_li32(p, rd, lo32);
  if (lo32 >> 31) {
    p += sprintf(p, "\n\tslli\t%s, %s, 32\n\tsrli\t%s, %s, 32", rd, rd, rd, rd);
  }
  *p++ = '\n';
  *p++ = '\t';
  p = riscv64_fmt_li32(p, tmp, hi32);
  p += sprintf(p, "\n\tslli\t%s, %s, 32\n\tadd\t%s, %s, %s", tmp, tmp, rd, rd,
	       tmp);
  (void)p;
  return buf;
}

const char *
riscv64_output_li(rtx *operands)
{
  return riscv64_output_u64(reg_names[REGNO(operands[0])],
			    (unsigned HOST_WIDE_INT)INTVAL(operands[1]));
}

const char *
riscv64_output_df_li(rtx *operands)
{
  REAL_VALUE_TYPE r;
  long l[2];
  unsigned HOST_WIDE_INT v;

  REAL_VALUE_FROM_CONST_DOUBLE(r, operands[1]);
  REAL_VALUE_TO_TARGET_DOUBLE(r, l);
  v = (unsigned HOST_WIDE_INT)(unsigned int)l[0]
      | ((unsigned HOST_WIDE_INT)(unsigned int)l[1] << 32);
  return riscv64_output_u64(reg_names[REGNO(operands[0])], v);
}

const char *
riscv64_output_sf_li(rtx *operands)
{
  static char buf[160];
  REAL_VALUE_TYPE r;
  long l;

  REAL_VALUE_FROM_CONST_DOUBLE(r, operands[1]);
  REAL_VALUE_TO_TARGET_SINGLE(r, l);
  riscv64_fmt_li32(buf, reg_names[REGNO(operands[0])], (unsigned int)l);
  return buf;
}

rtx
riscv64_function_arg(CUMULATIVE_ARGS *cum, enum machine_mode mode,
		     tree type, int named)
{
  int nregs;
  (void)type;
  (void)named;
  if (mode == VOIDmode)
    return 0;
  nregs = (GET_MODE_SIZE(mode) + UNITS_PER_WORD - 1) / UNITS_PER_WORD;
  if (nregs < 1)
    nregs = 1;
  if (cum->nregs + nregs > 8)
    return 0;
  return gen_rtx_REG(mode, RV_A0_REGNUM + cum->nregs);
}

void
riscv64_function_arg_advance(CUMULATIVE_ARGS *cum, enum machine_mode mode,
			     tree type, int named)
{
  int nregs;
  (void)type;
  (void)named;
  nregs = (GET_MODE_SIZE(mode) + UNITS_PER_WORD - 1) / UNITS_PER_WORD;
  if (nregs < 1)
    nregs = 1;
  if (cum->nregs + nregs <= 8)
    cum->nregs += nregs;
  else
    cum->nregs = 8;
}

const char *
riscv64_output_branch(rtx insn, rtx *operands)
{
  enum rtx_code code;
  (void)insn;
  code = GET_CODE(operands[3]);
  if (GET_CODE(operands[2]) == CONST_INT && INTVAL(operands[2]) == 0)
    operands[2] = gen_rtx_REG(GET_MODE(operands[1]), 0);
  switch (code) {
  case EQ:
    return "bne\t%1, %2, 8\n\tjal\tzero, %0";
  case NE:
    return "beq\t%1, %2, 8\n\tjal\tzero, %0";
  case LT:
    return "bge\t%1, %2, 8\n\tjal\tzero, %0";
  case GE:
    return "blt\t%1, %2, 8\n\tjal\tzero, %0";
  case LTU:
    return "bgeu\t%1, %2, 8\n\tjal\tzero, %0";
  case GEU:
    return "bltu\t%1, %2, 8\n\tjal\tzero, %0";
  case GT:
    return "bge\t%2, %1, 8\n\tjal\tzero, %0";
  case LE:
    return "blt\t%2, %1, 8\n\tjal\tzero, %0";
  case GTU:
    return "bgeu\t%2, %1, 8\n\tjal\tzero, %0";
  case LEU:
    return "bltu\t%2, %1, 8\n\tjal\tzero, %0";
  default:
    abort();
  }
}

void
riscv64_print_operand(FILE *file, rtx x, int code)
{
  (void)code;
  if (!x) {
    fputs("(null)", file);
    return;
  }
  switch (GET_CODE(x)) {
  case REG:
    fputs(reg_names[REGNO(x)], file);
    break;
  case MEM:
    riscv64_print_operand_address(file, XEXP(x, 0));
    break;
  case CONST_INT:
    fprintf(file, HOST_WIDE_INT_PRINT_DEC, INTVAL(x));
    break;
  case CONST_DOUBLE:
    {
      REAL_VALUE_TYPE r;
      long l[2];
      unsigned HOST_WIDE_INT v;

      REAL_VALUE_FROM_CONST_DOUBLE(r, x);
      if (GET_MODE(x) == SFmode) {
	REAL_VALUE_TO_TARGET_SINGLE(r, l[0]);
	fprintf(file, "%ld", l[0]);
      } else {
	REAL_VALUE_TO_TARGET_DOUBLE(r, l);
	v = (unsigned HOST_WIDE_INT)(unsigned int)l[0]
	    | ((unsigned HOST_WIDE_INT)(unsigned int)l[1] << 32);
	fprintf(file, HOST_WIDE_INT_PRINT_DEC, (HOST_WIDE_INT)v);
      }
    }
    break;
  case CONST:
  case LABEL_REF:
  case SYMBOL_REF:
    output_addr_const(file, x);
    break;
  default:
    output_addr_const(file, x);
    break;
  }
}

void
riscv64_print_operand_address(FILE *file, rtx x)
{
  rtx base;
  HOST_WIDE_INT off;

  off = 0;
  if (GET_CODE(x) == PLUS && GET_CODE(XEXP(x, 1)) == CONST_INT) {
    base = XEXP(x, 0);
    off = INTVAL(XEXP(x, 1));
  } else {
    base = x;
  }
  if (GET_CODE(base) == REG)
    fprintf(file, "%s, " HOST_WIDE_INT_PRINT_DEC, reg_names[REGNO(base)], off);
  else {
    output_addr_const(file, x);
  }
}
