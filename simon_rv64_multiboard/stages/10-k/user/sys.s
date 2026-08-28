.global _start
.global go
.global ecall3
_start:
	jal ra, go
ecall3:
	mv a7, a0
	mv a0, a1
	mv a1, a2
	mv a2, a3
	ecall
	jalr zero, ra, 0
