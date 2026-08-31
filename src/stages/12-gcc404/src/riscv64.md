;; RV64 integer md for gcc 4.0.4. Asm dialect is TCC-legal:
;; ld rd, rs, imm   sd rs2, rs1, imm   jal ra, lab   no .cfi_*

(define_insn "nop"
  [(const_int 0)]
  ""
  "addi\tzero, zero, 0")

;; Moves

(define_expand "movqi"
  [(set (match_operand:QI 0 "general_operand" "")
	(match_operand:QI 1 "general_operand" ""))]
  ""
  "
{
  if (GET_CODE (operands[0]) == MEM)
    operands[1] = force_reg (QImode, operands[1]);
}")

(define_insn "*movqi_internal"
  [(set (match_operand:QI 0 "nonimmediate_operand" "=r,r,m")
	(match_operand:QI 1 "general_operand" "r,m,r"))]
  ""
  "@
   addi\t%0, %1, 0
   lb\t%0, %1
   sb\t%1, %0")

(define_expand "movhi"
  [(set (match_operand:HI 0 "general_operand" "")
	(match_operand:HI 1 "general_operand" ""))]
  ""
  "
{
  if (GET_CODE (operands[0]) == MEM)
    operands[1] = force_reg (HImode, operands[1]);
}")

(define_insn "*movhi_internal"
  [(set (match_operand:HI 0 "nonimmediate_operand" "=r,r,m")
	(match_operand:HI 1 "general_operand" "r,m,r"))]
  ""
  "@
   addi\t%0, %1, 0
   lh\t%0, %1
   sh\t%1, %0")

(define_expand "movsi"
  [(set (match_operand:SI 0 "general_operand" "")
	(match_operand:SI 1 "general_operand" ""))]
  ""
  "
{
  if (GET_CODE (operands[0]) == MEM)
    operands[1] = force_reg (SImode, operands[1]);
}")

(define_insn "*movsi_li"
  [(set (match_operand:SI 0 "register_operand" "=r")
	(match_operand:SI 1 "const_int_operand" "n"))]
  "!SMALL_OPERAND (INTVAL (operands[1]))"
  "* return riscv64_output_li (operands);")

(define_insn "*movsi_internal"
  [(set (match_operand:SI 0 "nonimmediate_operand" "=r,r,r,m")
	(match_operand:SI 1 "general_operand" "r,I,m,r"))]
  ""
  "@
   addi\t%0, %1, 0
   addi\t%0, zero, %1
   lw\t%0, %1
   sw\t%1, %0")

(define_expand "movdi"
  [(set (match_operand:DI 0 "general_operand" "")
	(match_operand:DI 1 "general_operand" ""))]
  ""
  "
{
  if (GET_CODE (operands[0]) == MEM)
    operands[1] = force_reg (DImode, operands[1]);
  else if (GET_CODE (operands[1]) == CONST_INT
	   && INTVAL (operands[1]) != (HOST_WIDE_INT) (int) INTVAL (operands[1]))
    operands[1] = force_const_mem (DImode, operands[1]);
}")

(define_insn "*movdi_li"
  [(set (match_operand:DI 0 "register_operand" "=r")
	(match_operand:DI 1 "const_int_operand" "n"))]
  "!SMALL_OPERAND (INTVAL (operands[1]))"
  "* return riscv64_output_li (operands);")

(define_insn "*movdi_internal"
  [(set (match_operand:DI 0 "nonimmediate_operand" "=r,r,r,r,m")
	(match_operand:DI 1 "general_operand" "r,I,s,m,r"))]
  ""
  "@
   addi\t%0, %1, 0
   addi\t%0, zero, %1
   lla\t%0, %1
   ld\t%0, %1
   sd\t%1, %0")

;; Arithmetic

(define_insn "adddi3"
  [(set (match_operand:DI 0 "register_operand" "=r,r")
	(plus:DI (match_operand:DI 1 "register_operand" "%r,r")
		 (match_operand:DI 2 "nonmemory_operand" "r,I")))]
  ""
  "@
   add\t%0, %1, %2
   addi\t%0, %1, %2")

(define_insn "addsi3"
  [(set (match_operand:SI 0 "register_operand" "=r,r")
	(plus:SI (match_operand:SI 1 "register_operand" "%r,r")
		 (match_operand:SI 2 "nonmemory_operand" "r,I")))]
  ""
  "@
   addw\t%0, %1, %2
   addiw\t%0, %1, %2")

(define_insn "subdi3"
  [(set (match_operand:DI 0 "register_operand" "=r")
	(minus:DI (match_operand:DI 1 "register_operand" "r")
		  (match_operand:DI 2 "register_operand" "r")))]
  ""
  "sub\t%0, %1, %2")

(define_insn "subsi3"
  [(set (match_operand:SI 0 "register_operand" "=r")
	(minus:SI (match_operand:SI 1 "register_operand" "r")
		  (match_operand:SI 2 "register_operand" "r")))]
  ""
  "subw\t%0, %1, %2")

(define_insn "anddi3"
  [(set (match_operand:DI 0 "register_operand" "=r,r")
	(and:DI (match_operand:DI 1 "register_operand" "%r,r")
		(match_operand:DI 2 "nonmemory_operand" "r,I")))]
  ""
  "@
   and\t%0, %1, %2
   andi\t%0, %1, %2")

(define_insn "iordi3"
  [(set (match_operand:DI 0 "register_operand" "=r,r")
	(ior:DI (match_operand:DI 1 "register_operand" "%r,r")
		(match_operand:DI 2 "nonmemory_operand" "r,I")))]
  ""
  "@
   or\t%0, %1, %2
   ori\t%0, %1, %2")

(define_insn "xordi3"
  [(set (match_operand:DI 0 "register_operand" "=r,r")
	(xor:DI (match_operand:DI 1 "register_operand" "%r,r")
		(match_operand:DI 2 "nonmemory_operand" "r,I")))]
  ""
  "@
   xor\t%0, %1, %2
   xori\t%0, %1, %2")

(define_insn "andsi3"
  [(set (match_operand:SI 0 "register_operand" "=r,r")
	(and:SI (match_operand:SI 1 "register_operand" "%r,r")
		(match_operand:SI 2 "nonmemory_operand" "r,I")))]
  ""
  "@
   and\t%0, %1, %2
   andi\t%0, %1, %2")

(define_insn "iorsi3"
  [(set (match_operand:SI 0 "register_operand" "=r,r")
	(ior:SI (match_operand:SI 1 "register_operand" "%r,r")
		(match_operand:SI 2 "nonmemory_operand" "r,I")))]
  ""
  "@
   or\t%0, %1, %2
   ori\t%0, %1, %2")

(define_insn "xorsi3"
  [(set (match_operand:SI 0 "register_operand" "=r,r")
	(xor:SI (match_operand:SI 1 "register_operand" "%r,r")
		(match_operand:SI 2 "nonmemory_operand" "r,I")))]
  ""
  "@
   xor\t%0, %1, %2
   xori\t%0, %1, %2")

(define_insn "one_cmpldi2"
  [(set (match_operand:DI 0 "register_operand" "=r")
	(not:DI (match_operand:DI 1 "register_operand" "r")))]
  ""
  "xori\t%0, %1, -1")

(define_insn "negdi2"
  [(set (match_operand:DI 0 "register_operand" "=r")
	(neg:DI (match_operand:DI 1 "register_operand" "r")))]
  ""
  "sub\t%0, zero, %1")

(define_insn "ashldi3"
  [(set (match_operand:DI 0 "register_operand" "=r,r")
	(ashift:DI (match_operand:DI 1 "register_operand" "r,r")
		   (match_operand:QI 2 "nonmemory_operand" "r,I")))]
  ""
  "@
   sll\t%0, %1, %2
   slli\t%0, %1, %2")

(define_insn "ashrdi3"
  [(set (match_operand:DI 0 "register_operand" "=r,r")
	(ashiftrt:DI (match_operand:DI 1 "register_operand" "r,r")
		     (match_operand:QI 2 "nonmemory_operand" "r,I")))]
  ""
  "@
   sra\t%0, %1, %2
   srai\t%0, %1, %2")

(define_insn "lshrdi3"
  [(set (match_operand:DI 0 "register_operand" "=r,r")
	(lshiftrt:DI (match_operand:DI 1 "register_operand" "r,r")
		     (match_operand:QI 2 "nonmemory_operand" "r,I")))]
  ""
  "@
   srl\t%0, %1, %2
   srli\t%0, %1, %2")

(define_insn "ashlsi3"
  [(set (match_operand:SI 0 "register_operand" "=r,r")
	(ashift:SI (match_operand:SI 1 "register_operand" "r,r")
		   (match_operand:QI 2 "nonmemory_operand" "r,I")))]
  ""
  "@
   sllw\t%0, %1, %2
   slliw\t%0, %1, %2")

(define_insn "ashrsi3"
  [(set (match_operand:SI 0 "register_operand" "=r,r")
	(ashiftrt:SI (match_operand:SI 1 "register_operand" "r,r")
		     (match_operand:QI 2 "nonmemory_operand" "r,I")))]
  ""
  "@
   sraw\t%0, %1, %2
   sraiw\t%0, %1, %2")

(define_insn "lshrsi3"
  [(set (match_operand:SI 0 "register_operand" "=r,r")
	(lshiftrt:SI (match_operand:SI 1 "register_operand" "r,r")
		     (match_operand:QI 2 "nonmemory_operand" "r,I")))]
  ""
  "@
   srlw\t%0, %1, %2
   srliw\t%0, %1, %2")

(define_insn "muldi3"
  [(set (match_operand:DI 0 "register_operand" "=r")
	(mult:DI (match_operand:DI 1 "register_operand" "r")
		 (match_operand:DI 2 "register_operand" "r")))]
  ""
  "mul\t%0, %1, %2")

(define_insn "mulsi3"
  [(set (match_operand:SI 0 "register_operand" "=r")
	(mult:SI (match_operand:SI 1 "register_operand" "r")
		 (match_operand:SI 2 "register_operand" "r")))]
  ""
  "mulw\t%0, %1, %2")

(define_insn "divdi3"
  [(set (match_operand:DI 0 "register_operand" "=r")
	(div:DI (match_operand:DI 1 "register_operand" "r")
		(match_operand:DI 2 "register_operand" "r")))]
  ""
  "div\t%0, %1, %2")

(define_insn "udivdi3"
  [(set (match_operand:DI 0 "register_operand" "=r")
	(udiv:DI (match_operand:DI 1 "register_operand" "r")
		 (match_operand:DI 2 "register_operand" "r")))]
  ""
  "divu\t%0, %1, %2")

(define_insn "moddi3"
  [(set (match_operand:DI 0 "register_operand" "=r")
	(mod:DI (match_operand:DI 1 "register_operand" "r")
		(match_operand:DI 2 "register_operand" "r")))]
  ""
  "rem\t%0, %1, %2")

(define_insn "umoddi3"
  [(set (match_operand:DI 0 "register_operand" "=r")
	(umod:DI (match_operand:DI 1 "register_operand" "r")
		 (match_operand:DI 2 "register_operand" "r")))]
  ""
  "remu\t%0, %1, %2")

(define_insn "divsi3"
  [(set (match_operand:SI 0 "register_operand" "=r")
	(div:SI (match_operand:SI 1 "register_operand" "r")
		(match_operand:SI 2 "register_operand" "r")))]
  ""
  "divw\t%0, %1, %2")

(define_insn "udivsi3"
  [(set (match_operand:SI 0 "register_operand" "=r")
	(udiv:SI (match_operand:SI 1 "register_operand" "r")
		 (match_operand:SI 2 "register_operand" "r")))]
  ""
  "divuw\t%0, %1, %2")

(define_insn "modsi3"
  [(set (match_operand:SI 0 "register_operand" "=r")
	(mod:SI (match_operand:SI 1 "register_operand" "r")
		(match_operand:SI 2 "register_operand" "r")))]
  ""
  "remw\t%0, %1, %2")

(define_insn "umodsi3"
  [(set (match_operand:SI 0 "register_operand" "=r")
	(umod:SI (match_operand:SI 1 "register_operand" "r")
		 (match_operand:SI 2 "register_operand" "r")))]
  ""
  "remuw\t%0, %1, %2")

(define_insn "extendqidi2"
  [(set (match_operand:DI 0 "register_operand" "=r,r")
	(sign_extend:DI (match_operand:QI 1 "nonimmediate_operand" "r,m")))]
  ""
  "@
   slli\t%0, %1, 56\n\tsrai\t%0, %0, 56
   lb\t%0, %1")

(define_insn "extendhidi2"
  [(set (match_operand:DI 0 "register_operand" "=r,r")
	(sign_extend:DI (match_operand:HI 1 "nonimmediate_operand" "r,m")))]
  ""
  "@
   slli\t%0, %1, 48\n\tsrai\t%0, %0, 48
   lh\t%0, %1")

(define_insn "extendsidi2"
  [(set (match_operand:DI 0 "register_operand" "=r,r")
	(sign_extend:DI (match_operand:SI 1 "nonimmediate_operand" "r,m")))]
  ""
  "@
   slli\t%0, %1, 32\n\tsrai\t%0, %0, 32
   lw\t%0, %1")

(define_insn "zero_extendqidi2"
  [(set (match_operand:DI 0 "register_operand" "=r,r")
	(zero_extend:DI (match_operand:QI 1 "nonimmediate_operand" "r,m")))]
  ""
  "@
   andi\t%0, %1, 255
   lbu\t%0, %1")

(define_insn "zero_extendhidi2"
  [(set (match_operand:DI 0 "register_operand" "=r,r")
	(zero_extend:DI (match_operand:HI 1 "nonimmediate_operand" "r,m")))]
  ""
  "@
   slli\t%0, %1, 48\n\tsrli\t%0, %0, 48
   lhu\t%0, %1")

(define_insn "zero_extendsidi2"
  [(set (match_operand:DI 0 "register_operand" "=r,r")
	(zero_extend:DI (match_operand:SI 1 "nonimmediate_operand" "r,m")))]
  ""
  "@
   slli\t%0, %1, 32\n\tsrli\t%0, %0, 32
   lwu\t%0, %1")

(define_insn "truncdisi2"
  [(set (match_operand:SI 0 "register_operand" "=r")
	(truncate:SI (match_operand:DI 1 "register_operand" "r")))]
  ""
  "addiw\t%0, %1, 0")

(define_insn "truncdihi2"
  [(set (match_operand:HI 0 "register_operand" "=r")
	(truncate:HI (match_operand:DI 1 "register_operand" "r")))]
  ""
  "slli\t%0, %1, 48\n\tsrai\t%0, %0, 48")

(define_insn "truncdiqi2"
  [(set (match_operand:QI 0 "register_operand" "=r")
	(truncate:QI (match_operand:DI 1 "register_operand" "r")))]
  ""
  "andi\t%0, %1, 255")

(define_insn "*movsf_li"
  [(set (match_operand:SF 0 "register_operand" "=r")
	(match_operand:SF 1 "const_double_operand" "G"))]
  ""
  "* return riscv64_output_sf_li (operands);")

(define_insn "*movsf_internal"
  [(set (match_operand:SF 0 "nonimmediate_operand" "=r,r,m")
	(match_operand:SF 1 "general_operand" "r,m,r"))]
  ""
  "@
   addi\t%0, %1, 0
   lw\t%0, %1
   sw\t%1, %0")

(define_insn "*movdf_li"
  [(set (match_operand:DF 0 "register_operand" "=r")
	(match_operand:DF 1 "const_double_operand" "G"))]
  ""
  "* return riscv64_output_df_li (operands);")

(define_insn "*movdf_internal"
  [(set (match_operand:DF 0 "nonimmediate_operand" "=r,r,m")
	(match_operand:DF 1 "general_operand" "r,m,r"))]
  ""
  "@
   addi\t%0, %1, 0
   ld\t%0, %1
   sd\t%1, %0")

(define_expand "movsf"
  [(set (match_operand:SF 0 "general_operand" "")
	(match_operand:SF 1 "general_operand" ""))]
  ""
  "
{
  if (GET_CODE (operands[0]) == MEM)
    operands[1] = force_reg (SFmode, operands[1]);
}")

(define_expand "movdf"
  [(set (match_operand:DF 0 "general_operand" "")
	(match_operand:DF 1 "general_operand" ""))]
  ""
  "
{
  if (GET_CODE (operands[0]) == MEM)
    operands[1] = force_reg (DFmode, operands[1]);
}")

(define_expand "slt"
  [(set (match_operand:DI 0 "register_operand" "")
	(lt:DI (match_dup 1) (match_dup 2)))]
  ""
  "
{
  operands[1] = riscv64_cmp_op0 ();
  operands[2] = riscv64_cmp_op1 ();
}")

(define_expand "sltu"
  [(set (match_operand:DI 0 "register_operand" "")
	(ltu:DI (match_dup 1) (match_dup 2)))]
  ""
  "
{
  operands[1] = riscv64_cmp_op0 ();
  operands[2] = riscv64_cmp_op1 ();
}")

(define_expand "sgt"
  [(set (match_operand:DI 0 "register_operand" "")
	(gt:DI (match_dup 1) (match_dup 2)))]
  ""
  "
{
  operands[1] = riscv64_cmp_op0 ();
  operands[2] = riscv64_cmp_op1 ();
}")

(define_expand "sgtu"
  [(set (match_operand:DI 0 "register_operand" "")
	(gtu:DI (match_dup 1) (match_dup 2)))]
  ""
  "
{
  operands[1] = riscv64_cmp_op0 ();
  operands[2] = riscv64_cmp_op1 ();
}")

(define_insn "*sldi_lt"
  [(set (match_operand:DI 0 "register_operand" "=r,r")
	(lt:DI (match_operand:DI 1 "register_operand" "r,r")
	       (match_operand:DI 2 "nonmemory_operand" "r,I")))]
  ""
  "@
   slt\t%0, %1, %2
   slti\t%0, %1, %2")

(define_insn "*sldi_ltu"
  [(set (match_operand:DI 0 "register_operand" "=r,r")
	(ltu:DI (match_operand:DI 1 "register_operand" "r,r")
		(match_operand:DI 2 "nonmemory_operand" "r,I")))]
  ""
  "@
   sltu\t%0, %1, %2
   sltiu\t%0, %1, %2")

(define_insn "*sldi_gt"
  [(set (match_operand:DI 0 "register_operand" "=r")
	(gt:DI (match_operand:DI 1 "register_operand" "r")
	       (match_operand:DI 2 "register_operand" "r")))]
  ""
  "slt\t%0, %2, %1")

(define_insn "*sldi_gtu"
  [(set (match_operand:DI 0 "register_operand" "=r")
	(gtu:DI (match_operand:DI 1 "register_operand" "r")
		(match_operand:DI 2 "register_operand" "r")))]
  ""
  "sltu\t%0, %2, %1")

(define_insn "*sldi_eq"
  [(set (match_operand:DI 0 "register_operand" "=r")
	(eq:DI (match_operand:DI 1 "register_operand" "r")
	       (match_operand:DI 2 "register_operand" "r")))]
  ""
  "xor\t%0, %1, %2\n\tsltiu\t%0, %0, 1")

(define_insn "*sldi_ne"
  [(set (match_operand:DI 0 "register_operand" "=r")
	(ne:DI (match_operand:DI 1 "register_operand" "r")
	       (match_operand:DI 2 "register_operand" "r")))]
  ""
  "xor\t%0, %1, %2\n\tsltu\t%0, zero, %0")

;; Compare / branch. Saved operands, no CC register.

(define_expand "cmpdi"
  [(set (cc0)
	(compare:DI (match_operand:DI 0 "register_operand" "")
		    (match_operand:DI 1 "nonmemory_operand" "")))]
  ""
  "
{
  riscv64_save_compare (operands[0], operands[1]);
  DONE;
}")

(define_expand "cmpsi"
  [(set (cc0)
	(compare:SI (match_operand:SI 0 "register_operand" "")
		    (match_operand:SI 1 "nonmemory_operand" "")))]
  ""
  "
{
  riscv64_save_compare (operands[0], operands[1]);
  DONE;
}")

(define_expand "beq"
  [(set (pc)
	(if_then_else (eq (match_dup 1) (match_dup 2))
		      (label_ref (match_operand 0 "" ""))
		      (pc)))]
  ""
  "
{
  operands[1] = riscv64_cmp_op0 ();
  operands[2] = riscv64_cmp_op1 ();
}")

(define_expand "bne"
  [(set (pc)
	(if_then_else (ne (match_dup 1) (match_dup 2))
		      (label_ref (match_operand 0 "" ""))
		      (pc)))]
  ""
  "
{
  operands[1] = riscv64_cmp_op0 ();
  operands[2] = riscv64_cmp_op1 ();
}")

(define_expand "blt"
  [(set (pc)
	(if_then_else (lt (match_dup 1) (match_dup 2))
		      (label_ref (match_operand 0 "" ""))
		      (pc)))]
  ""
  "
{
  operands[1] = riscv64_cmp_op0 ();
  operands[2] = riscv64_cmp_op1 ();
}")

(define_expand "bge"
  [(set (pc)
	(if_then_else (ge (match_dup 1) (match_dup 2))
		      (label_ref (match_operand 0 "" ""))
		      (pc)))]
  ""
  "
{
  operands[1] = riscv64_cmp_op0 ();
  operands[2] = riscv64_cmp_op1 ();
}")

(define_expand "bltu"
  [(set (pc)
	(if_then_else (ltu (match_dup 1) (match_dup 2))
		      (label_ref (match_operand 0 "" ""))
		      (pc)))]
  ""
  "
{
  operands[1] = riscv64_cmp_op0 ();
  operands[2] = riscv64_cmp_op1 ();
}")

(define_expand "bgeu"
  [(set (pc)
	(if_then_else (geu (match_dup 1) (match_dup 2))
		      (label_ref (match_operand 0 "" ""))
		      (pc)))]
  ""
  "
{
  operands[1] = riscv64_cmp_op0 ();
  operands[2] = riscv64_cmp_op1 ();
}")

(define_expand "bgt"
  [(set (pc)
	(if_then_else (gt (match_dup 1) (match_dup 2))
		      (label_ref (match_operand 0 "" ""))
		      (pc)))]
  ""
  "
{
  operands[1] = riscv64_cmp_op0 ();
  operands[2] = riscv64_cmp_op1 ();
}")

(define_expand "ble"
  [(set (pc)
	(if_then_else (le (match_dup 1) (match_dup 2))
		      (label_ref (match_operand 0 "" ""))
		      (pc)))]
  ""
  "
{
  operands[1] = riscv64_cmp_op0 ();
  operands[2] = riscv64_cmp_op1 ();
}")

(define_expand "bgtu"
  [(set (pc)
	(if_then_else (gtu (match_dup 1) (match_dup 2))
		      (label_ref (match_operand 0 "" ""))
		      (pc)))]
  ""
  "
{
  operands[1] = riscv64_cmp_op0 ();
  operands[2] = riscv64_cmp_op1 ();
}")

(define_expand "bleu"
  [(set (pc)
	(if_then_else (leu (match_dup 1) (match_dup 2))
		      (label_ref (match_operand 0 "" ""))
		      (pc)))]
  ""
  "
{
  operands[1] = riscv64_cmp_op0 ();
  operands[2] = riscv64_cmp_op1 ();
}")

(define_insn "*branch_di"
  [(set (pc)
	(if_then_else (match_operator 3 "comparison_operator"
			[(match_operand:DI 1 "register_operand" "r")
			 (match_operand:DI 2 "nonmemory_operand" "rJ")])
		      (label_ref (match_operand 0 "" ""))
		      (pc)))]
  ""
  "* return riscv64_output_branch (insn, operands);")

(define_insn "*branch_si"
  [(set (pc)
	(if_then_else (match_operator 3 "comparison_operator"
			[(match_operand:SI 1 "register_operand" "r")
			 (match_operand:SI 2 "nonmemory_operand" "rJ")])
		      (label_ref (match_operand 0 "" ""))
		      (pc)))]
  ""
  "* return riscv64_output_branch (insn, operands);")

(define_insn "jump"
  [(set (pc)
	(label_ref (match_operand 0 "" "")))]
  ""
  "jal\tzero, %0")

(define_insn "indirect_jump"
  [(set (pc) (match_operand:DI 0 "register_operand" "r"))]
  ""
  "jalr\tzero, %0, 0")

(define_insn "tablejump"
  [(set (pc) (match_operand:DI 0 "register_operand" "r"))
   (use (label_ref (match_operand 1 "" "")))]
  ""
  "jalr\tzero, %0, 0")

;; Calls

(define_expand "call"
  [(parallel [(call (match_operand 0 "" "")
		    (match_operand 1 "" ""))
	      (clobber (reg:DI 1))])]
  ""
  "")

(define_insn "*call"
  [(call (mem:DI (match_operand:DI 0 "general_operand" "r,s"))
	 (match_operand 1 "" ""))
   (clobber (reg:DI 1))]
  ""
  "@
   jalr\tra, %0, 0
   jal\tra, %0")

(define_expand "call_value"
  [(parallel [(set (match_operand 0 "" "")
		   (call (match_operand 1 "" "")
			 (match_operand 2 "" "")))
	      (clobber (reg:DI 1))])]
  ""
  "")

(define_insn "*call_value"
  [(set (match_operand 0 "register_operand" "=r,r")
	(call (mem:DI (match_operand:DI 1 "general_operand" "r,s"))
	      (match_operand 2 "" "")))
   (clobber (reg:DI 1))]
  ""
  "@
   jalr\tra, %1, 0
   jal\tra, %1")

(define_expand "prologue"
  [(const_int 1)]
  ""
  "
{
  riscv64_expand_prologue ();
  DONE;
}")

(define_expand "epilogue"
  [(const_int 2)]
  ""
  "
{
  riscv64_expand_epilogue ();
  DONE;
}")

(define_insn "return"
  [(return)]
  "reload_completed"
  "jalr\tzero, ra, 0")

(define_insn "*addi_sp"
  [(set (reg:DI 2)
	(plus:DI (reg:DI 2)
		 (match_operand:DI 0 "const_int_operand" "I")))]
  ""
  "addi\tsp, sp, %0")
