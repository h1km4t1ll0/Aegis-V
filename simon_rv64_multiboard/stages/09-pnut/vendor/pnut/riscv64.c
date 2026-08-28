/* RISC-V RV64 codegen for pnut-exe on Simon.
 * Raw binary at USER_CODE_BASE. Syscalls match kernel/simon.S. */

#define WORD_SIZE 8
#ifndef MAX_CODE_SIZE
#define MAX_CODE_SIZE 16000000
#endif
#ifndef RT_GLO_SIZE
#define RT_GLO_SIZE 67108864
#endif
#ifndef RT_HEAP_SIZE
#define RT_HEAP_SIZE 33554432
#endif
#ifndef CODE_SIZE
#define CODE_SIZE 4000000
#endif
/* Keep the whole image in the code buffer so init/jump relocs are not
 * split across generate_exe flushes. TCC is ~1.8MB; 2MB was too tight
 * once CRT markers and inits are included. */
#define ONE_PASS_GENERATOR_NO_EARLY_OUTPUT
/* Use pnut_tcc_libc.c (printf/malloc) instead of exe.c stubs. M2 may not
 * see BOOTSTRAP_TCC's NO_BUILTIN_LIBC before this include. */
#define NO_BUILTIN_LIBC

#include "exe.c"

void generate_exe() {
  int i;
  i = 0;
  while (i < code_alloc) {
    write_i8(code[i]);
    i = i + 1;
  }
}

const int REG_ZERO = 0;
const int REG_RA = 1;
const int REG_T0 = 5;
const int REG_T1 = 6;
const int REG_T2 = 7;
const int REG_A3 = 13;
const int REG_A7 = 17;

const int reg_X = 10;
const int reg_Y = 11;
const int reg_Z = 12;
const int reg_SP = 2;
const int reg_glo = 9;

const int EQ = 4;
const int NE = 5;
const int LT = 12;
const int LT_U = 2;
const int GE = 13;
const int GE_U = 3;
const int LE = 14;
const int LE_U = 6;
const int GT = 15;
const int GT_U = 7;

void rv_emit(int w) {
  emit_i32_le(w);
}

void rv_r(int opcode, int rd, int funct3, int rs1, int rs2, int funct7) {
  int w;
  w = opcode;
  w = w + (rd << 7);
  w = w + (funct3 << 12);
  w = w + (rs1 << 15);
  w = w + (rs2 << 20);
  w = w + (funct7 << 25);
  rv_emit(w);
}

void rv_i(int opcode, int rd, int funct3, int rs1, int imm) {
  int w;
  w = opcode;
  w = w + (rd << 7);
  w = w + (funct3 << 12);
  w = w + (rs1 << 15);
  w = w + ((imm & 4095) << 20);
  rv_emit(w);
}

void rv_s(int opcode, int funct3, int rs1, int rs2, int imm) {
  int w;
  w = opcode;
  w = w + ((imm & 31) << 7);
  w = w + (funct3 << 12);
  w = w + (rs1 << 15);
  w = w + (rs2 << 20);
  w = w + (((imm >> 5) & 127) << 25);
  rv_emit(w);
}

void rv_u(int opcode, int rd, int imm20) {
  int w;
  w = opcode;
  w = w + (rd << 7);
  w = w + (imm20 << 12);
  rv_emit(w);
}

void rv_b(int rs1, int rs2, int funct3, int offset) {
  int w;
  w = 99;
  w = w + (((offset >> 11) & 1) << 7);
  w = w + (((offset >> 1) & 15) << 8);
  w = w + (funct3 << 12);
  w = w + (rs1 << 15);
  w = w + (rs2 << 20);
  w = w + (((offset >> 5) & 63) << 25);
  w = w + (((offset >> 12) & 1) << 31);
  rv_emit(w);
}

void rv_jal_zero(int offset) {
  int w;
  w = 111;
  w = w + (((offset >> 12) & 255) << 12);
  w = w + (((offset >> 11) & 1) << 20);
  w = w + (((offset >> 1) & 1023) << 21);
  w = w + (((offset >> 20) & 1) << 31);
  rv_emit(w);
}

int rv_fits12(int n) {
  if (n < -2048) {
    return 0;
  }
  if (n > 2047) {
    return 0;
  }
  return 1;
}

void rv_addi(int rd, int rs1, int imm) {
  rv_i(19, rd, 0, rs1, imm);
}

void rv_add(int rd, int rs1, int rs2) {
  rv_r(51, rd, 0, rs1, rs2, 0);
}

void rv_sub(int rd, int rs1, int rs2) {
  rv_r(51, rd, 0, rs1, rs2, 32);
}

void rv_and(int rd, int rs1, int rs2) {
  rv_r(51, rd, 7, rs1, rs2, 0);
}

void rv_or(int rd, int rs1, int rs2) {
  rv_r(51, rd, 6, rs1, rs2, 0);
}

void rv_xor(int rd, int rs1, int rs2) {
  rv_r(51, rd, 4, rs1, rs2, 0);
}

void rv_sll(int rd, int rs1, int rs2) {
  rv_r(51, rd, 1, rs1, rs2, 0);
}

void rv_srl(int rd, int rs1, int rs2) {
  rv_r(51, rd, 5, rs1, rs2, 0);
}

void rv_sra(int rd, int rs1, int rs2) {
  rv_r(51, rd, 5, rs1, rs2, 32);
}

void rv_mul(int rd, int rs1, int rs2) {
  rv_r(51, rd, 0, rs1, rs2, 1);
}

void rv_div(int rd, int rs1, int rs2) {
  rv_r(51, rd, 4, rs1, rs2, 1);
}

void rv_divu(int rd, int rs1, int rs2) {
  rv_r(51, rd, 5, rs1, rs2, 1);
}

void rv_rem(int rd, int rs1, int rs2) {
  rv_r(51, rd, 6, rs1, rs2, 1);
}

void rv_remu(int rd, int rs1, int rs2) {
  rv_r(51, rd, 7, rs1, rs2, 1);
}

void rv_auipc(int rd, int imm20) {
  rv_u(23, rd, imm20);
}

void rv_jalr_zero(int rs1) {
  rv_i(103, 0, 0, rs1, 0);
}

void rv_ld(int rd, int rs1, int imm) {
  rv_i(3, rd, 3, rs1, imm);
}

void rv_lw(int rd, int rs1, int imm) {
  rv_i(3, rd, 2, rs1, imm);
}

void rv_lwu(int rd, int rs1, int imm) {
  rv_i(3, rd, 6, rs1, imm);
}

void rv_lh(int rd, int rs1, int imm) {
  rv_i(3, rd, 1, rs1, imm);
}

void rv_lhu(int rd, int rs1, int imm) {
  rv_i(3, rd, 5, rs1, imm);
}

void rv_lb(int rd, int rs1, int imm) {
  rv_i(3, rd, 0, rs1, imm);
}

void rv_lbu(int rd, int rs1, int imm) {
  rv_i(3, rd, 4, rs1, imm);
}

void rv_sd(int rs2, int rs1, int imm) {
  rv_s(35, 3, rs1, rs2, imm);
}

void rv_sw(int rs2, int rs1, int imm) {
  rv_s(35, 2, rs1, rs2, imm);
}

void rv_sh(int rs2, int rs1, int imm) {
  rv_s(35, 1, rs1, rs2, imm);
}

void rv_sb(int rs2, int rs1, int imm) {
  rv_s(35, 0, rs1, rs2, imm);
}

void rv_slli(int rd, int rs1, int shamt) {
  rv_i(19, rd, 1, rs1, shamt);
}

void rv_srli(int rd, int rs1, int shamt) {
  rv_i(19, rd, 5, rs1, shamt);
}

void rv_srai(int rd, int rs1, int shamt) {
  /* I-imm bit 10 is insn bit 30 (SRAI). 2048 set bit 11 → illegal. */
  rv_i(19, rd, 5, rs1, 1024 + shamt);
}

void rv_andi(int rd, int rs1, int imm) {
  rv_i(19, rd, 7, rs1, imm);
}

void rv_addiw(int rd, int rs1, int imm) {
  rv_i(27, rd, 0, rs1, imm);
}

void rv_ecall() {
  rv_emit(115);
}

void rv_emit_i64(int n) {
  emit_i32_le(n);
  emit_i32_le(n >> 32);
}

void rv_addr(int base, int offset) {
  if (rv_fits12(offset) != 0) {
    rv_addi(REG_T2, base, offset);
  } else {
    mov_reg_imm(REG_T2, offset);
    rv_add(REG_T2, base, REG_T2);
  }
}

void mov_reg_reg(int dst, int src) {
  rv_addi(dst, src, 0);
}

void mov_reg_imm(int dst, int imm) {
  if (rv_fits12(imm) != 0) {
    rv_addi(dst, 0, imm);
    return;
  }
  rv_auipc(dst, 0);
  rv_ld(dst, dst, 12);
  rv_jal_zero(12);
  rv_emit_i64(imm);
}

#ifdef SUPPORT_64_BIT_LITERALS
void mov_reg_large_imm(int dst, int large_imm) {
  rv_auipc(dst, 0);
  rv_ld(dst, dst, 12);
  rv_jal_zero(12);
  emit_i64_le_large_imm(large_imm);
}
#endif

void add_reg_imm(int dst, int imm) {
  if (rv_fits12(imm) != 0) {
    rv_addi(dst, dst, imm);
    return;
  }
  mov_reg_imm(REG_T2, imm);
  rv_add(dst, dst, REG_T2);
}

void add_reg_reg(int dst, int src) {
  rv_add(dst, dst, src);
}

void sub_reg_reg(int dst, int src) {
  rv_sub(dst, dst, src);
}

void and_reg_reg(int dst, int src) {
  rv_and(dst, dst, src);
}

void or_reg_reg(int dst, int src) {
  rv_or(dst, dst, src);
}

void xor_reg_reg(int dst, int src) {
  rv_xor(dst, dst, src);
}

void imul_reg_reg(int dst, int src) {
  rv_mul(dst, dst, src);
}

void mul_reg_reg(int dst, int src) {
  rv_mul(dst, dst, src);
}

void idiv_reg_reg(int dst, int src) {
  rv_div(dst, dst, src);
}

void div_reg_reg(int dst, int src) {
  rv_divu(dst, dst, src);
}

void irem_reg_reg(int dst, int src) {
  rv_rem(dst, dst, src);
}

void rem_reg_reg(int dst, int src) {
  rv_remu(dst, dst, src);
}

void s_l_reg_reg(int dst, int src) {
  rv_sll(dst, dst, src);
}

void sar_reg_reg(int dst, int src) {
  rv_sra(dst, dst, src);
}

void shr_reg_reg(int dst, int src) {
  rv_srl(dst, dst, src);
}

void add_reg_lbl(int dst, int lbl) {
  rv_auipc(REG_T0, 0);
  rv_lw(REG_T1, REG_T0, 16);
  rv_add(dst, dst, REG_T1);
  rv_jal_zero(8);
  use_label(lbl);
}

void mov_reg_lbl(int reg, int lbl) {
  rv_auipc(reg, 0);
  rv_lw(REG_T1, reg, 20);
  rv_addi(reg, reg, 24);
  rv_add(reg, reg, REG_T1);
  rv_jal_zero(8);
  use_label(lbl);
}

void mov_mem8_reg(int base, int offset, int src) {
  if (rv_fits12(offset) != 0) {
    rv_sb(src, base, offset);
    return;
  }
  rv_addr(base, offset);
  rv_sb(src, REG_T2, 0);
}

void mov_mem16_reg(int base, int offset, int src) {
  if (rv_fits12(offset) != 0) {
    rv_sh(src, base, offset);
    return;
  }
  rv_addr(base, offset);
  rv_sh(src, REG_T2, 0);
}

void mov_mem32_reg(int base, int offset, int src) {
  if (rv_fits12(offset) != 0) {
    rv_sw(src, base, offset);
    return;
  }
  rv_addr(base, offset);
  rv_sw(src, REG_T2, 0);
}

void mov_mem64_reg(int base, int offset, int src) {
  if (rv_fits12(offset) != 0) {
    rv_sd(src, base, offset);
    return;
  }
  rv_addr(base, offset);
  rv_sd(src, REG_T2, 0);
}

void mov_reg_mem8(int dst, int base, int offset) {
  if (rv_fits12(offset) != 0) {
    rv_lbu(dst, base, offset);
    return;
  }
  rv_addr(base, offset);
  rv_lbu(dst, REG_T2, 0);
}

void mov_reg_mem16(int dst, int base, int offset) {
  if (rv_fits12(offset) != 0) {
    rv_lhu(dst, base, offset);
    return;
  }
  rv_addr(base, offset);
  rv_lhu(dst, REG_T2, 0);
}

void mov_reg_mem32(int dst, int base, int offset) {
  if (rv_fits12(offset) != 0) {
    rv_lwu(dst, base, offset);
    return;
  }
  rv_addr(base, offset);
  rv_lwu(dst, REG_T2, 0);
}

void mov_reg_mem8_sign_ext(int dst, int base, int offset) {
  if (rv_fits12(offset) != 0) {
    rv_lb(dst, base, offset);
    return;
  }
  rv_addr(base, offset);
  rv_lb(dst, REG_T2, 0);
}

void mov_reg_mem16_sign_ext(int dst, int base, int offset) {
  if (rv_fits12(offset) != 0) {
    rv_lh(dst, base, offset);
    return;
  }
  rv_addr(base, offset);
  rv_lh(dst, REG_T2, 0);
}

void mov_reg_mem32_sign_ext(int dst, int base, int offset) {
  if (rv_fits12(offset) != 0) {
    rv_lw(dst, base, offset);
    return;
  }
  rv_addr(base, offset);
  rv_lw(dst, REG_T2, 0);
}

void mov_reg_mem64(int dst, int base, int offset) {
  if (rv_fits12(offset) != 0) {
    rv_ld(dst, base, offset);
    return;
  }
  rv_addr(base, offset);
  rv_ld(dst, REG_T2, 0);
}

#ifdef SUPPORT_FULL_ARITHMETIC
void extend_reg(int reg, int width, int is_signed) {
  if (width == 1) {
    if (is_signed != 0) {
      rv_slli(reg, reg, 56);
      rv_srai(reg, reg, 56);
    } else {
      rv_andi(reg, reg, 255);
    }
    return;
  }
  if (width == 2) {
    rv_slli(reg, reg, 48);
    if (is_signed != 0) {
      rv_srai(reg, reg, 48);
    } else {
      rv_srli(reg, reg, 48);
    }
    return;
  }
  if (width == 4) {
    if (is_signed != 0) {
      rv_addiw(reg, reg, 0);
    } else {
      rv_slli(reg, reg, 32);
      rv_srli(reg, reg, 32);
    }
  }
}
#endif

void push_reg(int src) {
  /* x86 PUSH ESP snapshots SP before the decrement. Do the same so
   * stack_push(reg_SP) (address of a TOS local) is not the new slot. */
  if (src == reg_SP) {
    rv_addi(REG_T2, reg_SP, 0);
    rv_addi(reg_SP, reg_SP, -8);
    rv_sd(REG_T2, reg_SP, 0);
    return;
  }
  rv_addi(reg_SP, reg_SP, -8);
  rv_sd(src, reg_SP, 0);
}

void pop_reg(int dst) {
  rv_ld(dst, reg_SP, 0);
  rv_addi(reg_SP, reg_SP, 8);
}

void jump(int lbl) {
  rv_auipc(REG_T0, 0);
  rv_lw(REG_T1, REG_T0, 20);
  rv_addi(REG_T0, REG_T0, 24);
  rv_add(REG_T0, REG_T0, REG_T1);
  rv_jalr_zero(REG_T0);
  use_label(lbl);
}

void jump_rel(int offset) {
  rv_auipc(REG_T0, 0);
  rv_lw(REG_T1, REG_T0, 20);
  rv_addi(REG_T0, REG_T0, 24);
  rv_add(REG_T0, REG_T0, REG_T1);
  rv_jalr_zero(REG_T0);
  emit_i32_le(offset);
}

void call(int lbl) {
  rv_auipc(REG_T0, 0);
  rv_lw(REG_T1, REG_T0, 32);
  rv_addi(REG_RA, REG_T0, 36);
  rv_addi(REG_T0, REG_T0, 36);
  rv_add(REG_T0, REG_T0, REG_T1);
  rv_addi(reg_SP, reg_SP, -8);
  rv_sd(REG_RA, reg_SP, 0);
  rv_jalr_zero(REG_T0);
  use_label(lbl);
}

void call_reg(int reg) {
  rv_addi(reg_SP, reg_SP, -8);
  rv_auipc(REG_T0, 0);
  rv_addi(REG_T0, REG_T0, 16);
  rv_sd(REG_T0, reg_SP, 0);
  rv_jalr_zero(reg);
}

void ret() {
  rv_ld(REG_T0, reg_SP, 0);
  rv_addi(reg_SP, reg_SP, 8);
  rv_jalr_zero(REG_T0);
}

void debug_interrupt() {
  rv_emit(1048691);
}

void jump_cond_reg_reg(int cond, int lbl, int reg1, int reg2) {
  int rs1;
  int rs2;
  int f3;
  rs1 = reg1;
  rs2 = reg2;
  f3 = 1;
  if (cond == EQ) {
    f3 = 1;
  } else if (cond == NE) {
    f3 = 0;
  } else if (cond == LT) {
    f3 = 5;
  } else if (cond == GE) {
    f3 = 4;
  } else if (cond == LT_U) {
    f3 = 7;
  } else if (cond == GE_U) {
    f3 = 6;
  } else if (cond == LE) {
    rs1 = reg2;
    rs2 = reg1;
    f3 = 4;
  } else if (cond == GT) {
    rs1 = reg2;
    rs2 = reg1;
    f3 = 5;
  } else if (cond == LE_U) {
    rs1 = reg2;
    rs2 = reg1;
    f3 = 6;
  } else if (cond == GT_U) {
    rs1 = reg2;
    rs2 = reg1;
    f3 = 7;
  }
  rv_b(rs1, rs2, f3, 28);
  jump(lbl);
}

void setup_proc_args(int global_vars_size) {
  mov_reg_reg(reg_X, reg_SP);
  add_reg_imm(reg_X, global_vars_size + WORD_SIZE);
  push_reg(reg_X);
  mov_reg_mem(reg_Y, reg_X, -WORD_SIZE);
  push_reg(reg_Y);
}

void rv_syscall(int sysno) {
  mov_reg_imm(REG_A7, sysno);
  rv_ecall();
}

void os_allocate_memory(int size) {
  mov_reg_imm(reg_X, 0);
  rv_syscall(214);
  push_reg(reg_X);
  add_reg_imm(reg_X, size);
  rv_syscall(214);
  pop_reg(reg_X);
}

void os_exit() {
  rv_syscall(93);
}

void os_read() {
  rv_syscall(63);
}

void os_write() {
  rv_syscall(64);
}

void os_open() {
  mov_reg_reg(REG_A3, reg_Z);
  mov_reg_reg(reg_Z, reg_Y);
  mov_reg_reg(reg_Y, reg_X);
  mov_reg_imm(reg_X, -100);
  rv_syscall(56);
}

void os_close() {
  rv_syscall(57);
}

void os_seek() {
  rv_syscall(62);
}

void os_unlink() {
  mov_reg_imm(reg_X, -1);
}

void os_mkdir() {
  mov_reg_imm(reg_X, -1);
}

void os_chmod() {
  mov_reg_imm(reg_X, -1);
}

void os_access() {
  mov_reg_reg(reg_Z, reg_Y);
  mov_reg_reg(reg_Y, reg_X);
  mov_reg_imm(reg_X, -100);
  mov_reg_imm(REG_A3, 0);
  rv_syscall(48);
}
