/* RV64 integer tm.h for gcc 4.0.4 on Kronos. Used by gen* and cc1. */
#ifndef GCC_TM_H
#define GCC_TM_H

#define BITS_PER_WORD 64
#define UNITS_PER_WORD 8
#define MIN_UNITS_PER_WORD 8
#define POINTER_SIZE 64
#define MAX_REGS_PER_ADDRESS 1
#define STORE_FLAG_VALUE 1
#define DEFAULT_CONSTRAINT_LEN(C, STR) 1
#define CONSTRAINT_LEN(C, STR) DEFAULT_CONSTRAINT_LEN ((C), (STR))

#define OBJECT_FORMAT_ELF
#define TARGET_CPU_CPP_BUILTINS()			\
  do {							\
    builtin_define ("__riscv");				\
    builtin_define ("__riscv64");			\
    builtin_define ("__riscv_xlen=64");			\
    builtin_assert ("cpu=riscv64");			\
    builtin_assert ("machine=riscv64");			\
  } while (0)

extern int target_flags;
#define TARGET_DEFAULT 0
#define TARGET_SWITCHES { { "", TARGET_DEFAULT, "" } }
#define TARGET_VERSION fputs (" (riscv64)", stderr);

#define BITS_BIG_ENDIAN 0
#define BYTES_BIG_ENDIAN 0
#define WORDS_BIG_ENDIAN 0
#define FLOAT_WORDS_BIG_ENDIAN 0

#define INT_TYPE_SIZE 32
#define SHORT_TYPE_SIZE 16
#define LONG_TYPE_SIZE 64
#define LONG_LONG_TYPE_SIZE 64
#define FLOAT_TYPE_SIZE 32
#define DOUBLE_TYPE_SIZE 64
#define LONG_DOUBLE_TYPE_SIZE 64
#define DEFAULT_SIGNED_CHAR 1
#define WCHAR_TYPE "int"
#define WCHAR_TYPE_SIZE 32
#define SIZE_TYPE "long unsigned int"
#define PTRDIFF_TYPE "long int"

#define PARM_BOUNDARY 64
#define STACK_BOUNDARY 128
#define FUNCTION_BOUNDARY 32
#define BIGGEST_ALIGNMENT 64
#define EMPTY_FIELD_BOUNDARY 32
#define STRUCTURE_SIZE_BOUNDARY 8
#define STRICT_ALIGNMENT 1
#define PCC_BITFIELD_TYPE_MATTERS 1
#define MAX_FIXED_MODE_SIZE 64

#define FIRST_PSEUDO_REGISTER 32
#define RV_RA_REGNUM 1
#define RV_SP_REGNUM 2
#define RV_GP_REGNUM 3
#define RV_TP_REGNUM 4
#define RV_FP_REGNUM 8
#define RV_A0_REGNUM 10
#define STACK_POINTER_REGNUM RV_SP_REGNUM
#define FRAME_POINTER_REGNUM RV_FP_REGNUM
#define HARD_FRAME_POINTER_REGNUM RV_FP_REGNUM
#define ARG_POINTER_REGNUM RV_FP_REGNUM
#define STATIC_CHAIN_REGNUM 5
#define RETURN_ADDRESS_REGNUM RV_RA_REGNUM

#define FIXED_REGISTERS \
  { 1, 0, 1, 1, 1, 0, 0, 0, \
    0, 0, 0, 0, 0, 0, 0, 0, \
    0, 0, 0, 0, 0, 0, 0, 0, \
    0, 0, 0, 0, 0, 0, 0, 0 }

#define CALL_USED_REGISTERS \
  { 1, 1, 1, 1, 1, 1, 1, 1, \
    0, 0, 1, 1, 1, 1, 1, 1, \
    1, 1, 0, 0, 0, 0, 0, 0, \
    0, 0, 0, 0, 1, 1, 1, 1 }

#define REGISTER_NAMES \
  { "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", \
    "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5", \
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7", \
    "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6" }

#define HARD_REGNO_NREGS(REGNO, MODE) \
  ((GET_MODE_SIZE (MODE) + UNITS_PER_WORD - 1) / UNITS_PER_WORD)
#define HARD_REGNO_MODE_OK(REGNO, MODE) 1
#define MODES_TIEABLE_P(MODE1, MODE2) 1

enum reg_class
{
  NO_REGS,
  GR_REGS,
  ALL_REGS,
  LIM_REG_CLASSES
};
#define GENERAL_REGS GR_REGS
#define N_REG_CLASSES ((int) LIM_REG_CLASSES)
#define REG_CLASS_NAMES { "NO_REGS", "GR_REGS", "ALL_REGS" }
#define REG_CLASS_CONTENTS { 0, 0xffffffff, 0xffffffff }
#define REGNO_REG_CLASS(REGNO) GR_REGS
#define BASE_REG_CLASS GR_REGS
#define INDEX_REG_CLASS NO_REGS
#define REG_CLASS_FROM_LETTER(C) NO_REGS
#define REGNO_OK_FOR_INDEX_P(N) 0
#define PREFERRED_RELOAD_CLASS(X, CLASS) (CLASS)
#define CLASS_MAX_NREGS(CLASS, MODE) HARD_REGNO_NREGS (0, MODE)
#define CLASS_LIKELY_SPILLED_P(CLASS) 0

#define SMALL_OPERAND(VAL) \
  (((unsigned HOST_WIDE_INT) (HOST_WIDE_INT) (VAL) + 0x800) < 0x1000)

#ifdef REG_OK_STRICT
#define REGNO_OK_FOR_BASE_P(N) ((N) < FIRST_PSEUDO_REGISTER)
#else
#define REGNO_OK_FOR_BASE_P(N) 1
#endif
#define REG_OK_FOR_BASE_P(X) REGNO_OK_FOR_BASE_P (REGNO (X))

#define CONSTANT_ADDRESS_P(X) CONSTANT_P (X)
#define GO_IF_LEGITIMATE_ADDRESS(MODE, X, ADDR) \
  do { \
    if (GET_CODE (X) == REG && REG_OK_FOR_BASE_P (X)) \
      goto ADDR; \
    if (GET_CODE (X) == PLUS \
	&& GET_CODE (XEXP (X, 0)) == REG && REG_OK_FOR_BASE_P (XEXP (X, 0)) \
	&& GET_CODE (XEXP (X, 1)) == CONST_INT \
	&& SMALL_OPERAND (INTVAL (XEXP (X, 1)))) \
      goto ADDR; \
  } while (0)

#define LEGITIMIZE_ADDRESS(X, OLDX, MODE, WIN)
#define GO_IF_MODE_DEPENDENT_ADDRESS(ADDR, LABEL)
#define LEGITIMATE_CONSTANT_P(X) 1

#define CONST_OK_FOR_LETTER_P(VALUE, C) \
  ((C) == 'I' ? SMALL_OPERAND (VALUE) \
   : (C) == 'J' ? (VALUE) == 0 \
   : 0)
#define CONST_DOUBLE_OK_FOR_LETTER_P(VALUE, C) ((C) == 'G')
#define EXTRA_CONSTRAINT(OP, C) 0

#define ELIMINABLE_REGS \
  { { FRAME_POINTER_REGNUM, STACK_POINTER_REGNUM } }
#define CAN_ELIMINATE(FROM, TO) 1
#define FRAME_POINTER_REQUIRED 0
#define EXIT_IGNORE_STACK 1
#define STARTING_FRAME_OFFSET 0
#define FIRST_PARM_OFFSET(FNDECL) 0
#define INITIAL_ELIMINATION_OFFSET(FROM, TO, OFFSET) \
  ((OFFSET) = riscv64_initial_elimination_offset (FROM, TO))

typedef struct { int nregs; } CUMULATIVE_ARGS;
#define INIT_CUMULATIVE_ARGS(CUM, FNTYPE, LIBNAME, INDIRECT, N_NAMED_ARGS) \
  do { (CUM).nregs = 0; } while (0)
#define FUNCTION_ARG_REGNO_P(N) ((N) >= 10 && (N) < 18)
#define FUNCTION_ARG(CUM, MODE, TYPE, NAMED) \
  riscv64_function_arg (&(CUM), MODE, TYPE, NAMED)
#define FUNCTION_ARG_ADVANCE(CUM, MODE, TYPE, NAMED) \
  riscv64_function_arg_advance (&(CUM), MODE, TYPE, NAMED)
#define FUNCTION_VALUE(VALTYPE, FUNC) \
  gen_rtx_REG (TYPE_MODE (VALTYPE), RV_A0_REGNUM)
#define LIBCALL_VALUE(MODE) gen_rtx_REG (MODE, RV_A0_REGNUM)
#define FUNCTION_VALUE_REGNO_P(N) ((N) == RV_A0_REGNUM)
#define RETURN_IN_MEMORY(TYPE) (int_size_in_bytes (TYPE) > 2 * UNITS_PER_WORD)
#define STRUCT_VALUE 0
#define DEFAULT_PCC_STRUCT_RETURN 0

#define FUNCTION_PROFILER(FILE, LABELNO) \
  fprintf ((FILE), "# profile %d\n", (LABELNO))
#define TRAMPOLINE_SIZE 32
#define TRAMPOLINE_ALIGNMENT 64
#define INITIALIZE_TRAMPOLINE(TRAMP, FNADDR, CXT) \
  riscv64_initialize_trampoline (TRAMP, FNADDR, CXT)

#define MOVE_MAX 8
#define MOVE_RATIO 4
#define SLOW_BYTE_ACCESS 0
#define SHIFT_COUNT_TRUNCATED 1
#define CASE_VECTOR_MODE DImode
#define CASE_VALUES_THRESHOLD 10000
#define CASE_USE_BIT_TESTS 0
#define Pmode DImode
#define FUNCTION_MODE DImode
#define WORD_REGISTER_OPERATIONS

#define TEXT_SECTION_ASM_OP "\t.text"
#define DATA_SECTION_ASM_OP "\t.data"
#define BSS_SECTION_ASM_OP "\t.bss"
#define READONLY_DATA_SECTION_ASM_OP "\t.section\t.rodata"
#define ASM_COMMENT_START "#"
#define ASM_APP_ON ""
#define ASM_APP_OFF ""
#define LOCAL_LABEL_PREFIX "."
#define USER_LABEL_PREFIX ""
#define REGISTER_PREFIX ""
#define IMMEDIATE_PREFIX ""

#define ASM_OUTPUT_ALIGN(STREAM, POWER) \
  fprintf ((STREAM), "\t.align %d\n", 1 << (POWER))
#define ASM_OUTPUT_SKIP(STREAM, N) \
  fprintf ((STREAM), "\t.space %d\n", (int) (N))
#define ASM_OUTPUT_ALIGNED_BSS(FILE, DECL, NAME, SIZE, ALIGN) \
  do { \
    bss_section (); \
    ASM_OUTPUT_ALIGN ((FILE), ((ALIGN) / BITS_PER_UNIT) ? floor_log2 ((ALIGN) / BITS_PER_UNIT) : 0); \
    ASM_OUTPUT_LABEL ((FILE), (NAME)); \
    ASM_OUTPUT_SKIP ((FILE), (SIZE) ? (SIZE) : 1); \
  } while (0)
#define ASM_OUTPUT_COMMON(STREAM, NAME, SIZE, ROUNDED) \
  do { \
    fputs ("\t.globl ", (STREAM)); \
    assemble_name ((STREAM), (NAME)); \
    fprintf ((STREAM), "\n\t.bss\n"); \
    assemble_name ((STREAM), (NAME)); \
    fprintf ((STREAM), ":\n\t.space %u\n", (unsigned) (ROUNDED)); \
  } while (0)
#define ASM_OUTPUT_LOCAL(STREAM, NAME, SIZE, ROUNDED) \
  do { \
    fputs ("\t.bss\n", (STREAM)); \
    assemble_name ((STREAM), (NAME)); \
    fprintf ((STREAM), ":\n\t.space %u\n", (unsigned) (ROUNDED)); \
  } while (0)
#define ASM_OUTPUT_LABEL(STREAM, NAME) \
  do { assemble_name ((STREAM), (NAME)); fputs (":\n", (STREAM)); } while (0)
#define ASM_GLOBALIZE_LABEL(STREAM, NAME) \
  do { fputs ("\t.globl ", (STREAM)); assemble_name ((STREAM), (NAME)); \
       fputs ("\n", (STREAM)); } while (0)
#define GLOBAL_ASM_OP "\t.globl\t"
#define ASM_OUTPUT_TYPE_DIRECTIVE(STREAM, NAME, TYPE) do { } while (0)
#define ASM_OUTPUT_SIZE_DIRECTIVE(STREAM, NAME, SIZE) do { } while (0)
#define ASM_GENERATE_INTERNAL_LABEL(LABEL, PREFIX, NUM) \
  sprintf ((LABEL), "*.%s%ld", (PREFIX), (long) (NUM))
#define ASM_OUTPUT_ADDR_DIFF_ELT(STREAM, BODY, VALUE, REL) \
  fprintf ((STREAM), "\t.quad .L%d\n", (VALUE))
#define ASM_OUTPUT_ADDR_VEC_ELT(STREAM, VALUE) \
  fprintf ((STREAM), "\t.quad .L%d\n", (VALUE))

#define PRINT_OPERAND(STREAM, X, CODE) riscv64_print_operand (STREAM, X, CODE)
#define PRINT_OPERAND_ADDRESS(STREAM, X) \
  riscv64_print_operand_address (STREAM, X)
#define PRINT_OPERAND_PUNCT_VALID_P(CODE) 0

#define NO_DOLLAR_IN_LABEL
#define PREFERRED_DEBUGGING_TYPE NO_DEBUG
#define REG_OK_FOR_INDEX_P(X) 0
#define INCOMING_RETURN_ADDR_RTX gen_rtx_REG (Pmode, RETURN_ADDRESS_REGNUM)
#define INCOMING_FRAME_SP_OFFSET 0

#ifndef BITS_PER_UNIT
#define BITS_PER_UNIT 8
#endif
#define TRULY_NOOP_TRUNCATION(OUTPREC, INPREC) 1
#define RETURN_POPS_ARGS(FUNDECL, FUNTYPE, SIZE) 0
#define HAS_INIT_SECTION
#define INIT_EXPANDERS riscv64_init_expanders ()

#include "defaults.h"

#endif
