.global _start
.global read
.global write
.global open
.global close
.global lseek
.global brk
.global exit
_start:
	addi a0, zero, 1
	lla a1, crtmsg
	addi a2, zero, 8
	addi a7, zero, 64
	ecall
	ld a0, sp, 0
	addi a1, sp, 8
	jal ra, main
	addi a7, zero, 93
	ecall
read:
	addi a7, zero, 63
	ecall
	jalr zero, ra, 0
write:
	addi a7, zero, 64
	ecall
	jalr zero, ra, 0
open:
	mv a3, a2
	mv a2, a1
	mv a1, a0
	addi a0, zero, -100
	addi a7, zero, 56
	ecall
	jalr zero, ra, 0
close:
	addi a7, zero, 57
	ecall
	jalr zero, ra, 0
lseek:
	addi a7, zero, 62
	ecall
	jalr zero, ra, 0
brk:
	addi a7, zero, 214
	ecall
	jalr zero, ra, 0
exit:
	addi a7, zero, 93
	ecall
crtmsg:
	.ascii "tcc-crt\n"
