.global _start
_start:
	addi a0, zero, 1
	lla a1, msg
	addi a2, zero, 3
	addi a7, zero, '@'
	ecall
	addi a0, zero, 0
	addi a7, zero, ']'
	ecall
msg:
	.ascii "hi\n"
