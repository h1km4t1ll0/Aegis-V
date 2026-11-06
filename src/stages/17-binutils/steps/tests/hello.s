# Rung binutils (17-binutils README): written by hand, Linux riscv64
# system calls only: write(1, msg, 16), exit(0). norelax: no gp here, and
# ld may relax `la` to a gp-relative addi.
	.option norelax
	.section .rodata
msg:
	.ascii "binutils asm ok\n"

	.text
	.globl _start
_start:
	li a0, 1
	la a1, msg
	li a2, 16
	li a7, 64
	ecall
	li a0, 0
	li a7, 93
	ecall
