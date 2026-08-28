.global _start
.global main
.global exit
.global ecall6
_start:
	ld a0, sp, 0
	addi a1, sp, 8
	jal ra, main
	jal ra, exit
ecall6:
	mv a7, a0
	mv a0, a1
	mv a1, a2
	mv a2, a3
	mv a3, a4
	mv a4, a5
	mv a5, a6
	ecall
	jalr zero, ra, 0
