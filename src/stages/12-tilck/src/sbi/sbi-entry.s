/* Aegis-V SBI shim: M-mode entry and trap vector (see README.md).
   QEMU virt jumps to 0x80000000 with a0 = hartid, a1 = DTB address.
   tcc has no linker script: a custom "a" section is laid out before
   .data.ro and .text, so in the raw image this code is the first byte.
   CSR numbers: mie 0x304, mtvec 0x305, mscratch 0x340, mepc 0x341. */
.section .sbi.entry, "a"
.global _start
.global sbi_enter_s
.global sbi_trap_entry
_start:
	csrw 0x304, zero
	la t0, __bss_start
	la t1, __bss_stop
	bge t0, t1, 2f
1:
	sd zero, 0(t0)
	addi t0, t0, 8
	blt t0, t1, 1b
2:
	la sp, sbi_stack + 16384
	csrw 0x340, sp
	la t0, sbi_trap_entry
	csrw 0x305, t0
	call sbi_main
3:
	wfi
	j 3b

/* sbi_enter_s(hartid, dtb, entry): mstatus.MPP is S (set by sbi_main) */
sbi_enter_s:
	csrw 0x341, a2
	mret

/* Trap from S (ecall, machine timer): swap to the M stack in mscratch,
   save x1..x31 as regs[1..31] (regs[2] = the S sp), call sbi_trap(regs),
   restore, swap back. */
.align 2
sbi_trap_entry:
	csrrw sp, 0x340, sp
	addi sp, sp, -256
	sd ra, 8(sp)
	sd gp, 24(sp)
	sd tp, 32(sp)
	sd t0, 40(sp)
	sd t1, 48(sp)
	sd t2, 56(sp)
	sd s0, 64(sp)
	sd s1, 72(sp)
	sd a0, 80(sp)
	sd a1, 88(sp)
	sd a2, 96(sp)
	sd a3, 104(sp)
	sd a4, 112(sp)
	sd a5, 120(sp)
	sd a6, 128(sp)
	sd a7, 136(sp)
	sd s2, 144(sp)
	sd s3, 152(sp)
	sd s4, 160(sp)
	sd s5, 168(sp)
	sd s6, 176(sp)
	sd s7, 184(sp)
	sd s8, 192(sp)
	sd s9, 200(sp)
	sd s10, 208(sp)
	sd s11, 216(sp)
	sd t3, 224(sp)
	sd t4, 232(sp)
	sd t5, 240(sp)
	sd t6, 248(sp)
	csrr t0, 0x340
	sd t0, 16(sp)
	mv a0, sp
	call sbi_trap
	ld ra, 8(sp)
	ld gp, 24(sp)
	ld tp, 32(sp)
	ld t0, 40(sp)
	ld t1, 48(sp)
	ld t2, 56(sp)
	ld s0, 64(sp)
	ld s1, 72(sp)
	ld a0, 80(sp)
	ld a1, 88(sp)
	ld a2, 96(sp)
	ld a3, 104(sp)
	ld a4, 112(sp)
	ld a5, 120(sp)
	ld a6, 128(sp)
	ld a7, 136(sp)
	ld s2, 144(sp)
	ld s3, 152(sp)
	ld s4, 160(sp)
	ld s5, 168(sp)
	ld s6, 176(sp)
	ld s7, 184(sp)
	ld s8, 192(sp)
	ld s9, 200(sp)
	ld s10, 208(sp)
	ld s11, 216(sp)
	ld t3, 224(sp)
	ld t4, 232(sp)
	ld t5, 240(sp)
	ld t6, 248(sp)
	addi sp, sp, 256
	csrrw sp, 0x340, sp
	mret
