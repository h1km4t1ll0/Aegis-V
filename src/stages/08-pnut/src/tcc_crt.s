.global _start
.global read
.global write
.global open
.global close
.global lseek
.global brk
.global exit
_start:
	addi a0, zero, '!'-' '
	lla a1, crtmsg
	addi a2, zero, '('-' '
	addi a7, zero, '@'
	ecall
	ld a0, sp, 'A'-'A'
	addi a1, sp, '('-' '
	jal ra, main
	addi a7, zero, ']'
	ecall
read:
	addi a7, zero, '?'
	ecall
	jalr zero, ra, 'A'-'A'
write:
	addi a7, zero, '@'
	ecall
	jalr zero, ra, 'A'-'A'
open:
	mv a3, a2
	mv a2, a1
	mv a1, a0
	addi a0, zero, -'d'
	addi a7, zero, '8'
	ecall
	jalr zero, ra, 'A'-'A'
close:
	addi a7, zero, '9'
	ecall
	jalr zero, ra, 'A'-'A'
lseek:
	addi a7, zero, '>'
	ecall
	jalr zero, ra, 'A'-'A'
brk:
	addi a7, zero, 'V'+'V'+'*'
	ecall
	jalr zero, ra, 'A'-'A'
exit:
	addi a7, zero, ']'
	ecall
crtmsg:
	.ascii "tcc-crt\n"
