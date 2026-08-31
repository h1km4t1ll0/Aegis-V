#ifndef GCC_RISCV64_PROTOS_H
#define GCC_RISCV64_PROTOS_H

extern rtx riscv64_compare_op0;
extern rtx riscv64_compare_op1;
extern void riscv64_init_expanders (void);
extern void riscv64_save_compare (rtx, rtx);
extern rtx riscv64_cmp_op0 (void);
extern rtx riscv64_cmp_op1 (void);
extern HOST_WIDE_INT riscv64_initial_elimination_offset (int, int);
extern void riscv64_expand_prologue (void);
extern void riscv64_expand_epilogue (void);

#ifdef RTX_CODE
extern void riscv64_print_operand (FILE *, rtx, int);
extern void riscv64_print_operand_address (FILE *, rtx);
extern const char *riscv64_output_branch (rtx, rtx *);
extern const char *riscv64_output_li (rtx *);
extern const char *riscv64_output_df_li (rtx *);
extern const char *riscv64_output_sf_li (rtx *);
extern void riscv64_initialize_trampoline (rtx, rtx, rtx);
#endif

#ifdef TREE_CODE
extern rtx riscv64_function_arg (CUMULATIVE_ARGS *, enum machine_mode, tree, int);
extern void riscv64_function_arg_advance (CUMULATIVE_ARGS *, enum machine_mode, tree, int);
#endif

#endif
