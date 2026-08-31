.global _start
.global kmain
.global trap_c
.global trap_entry
.global mret_user
.global read_mepc
.global write_mepc
.global read_mtval
.global kphys_ramfs
.global kphys_user
.global kphys_usp
.global kphys_dram
.global kphys_kalloc
.global write_satp
.global jump_k
_start:
	lla sp, kstack
	lla t0, trap_entry
	.long 0x30529073
	addi t0, zero, -1
	.long 0x3B029073
	addi t0, zero, 15
	.long 0x3A029073
	jal ra, kmain
hang:
	jal zero, hang
.align 4
trap_entry:
	lla t6, trap_save
	sd t6, ra, 8
	sd t6, sp, 16
	sd t6, gp, 24
	sd t6, tp, 32
	sd t6, t0, 40
	sd t6, t1, 48
	sd t6, t2, 56
	sd t6, s0, 64
	sd t6, s1, 72
	sd t6, a0, 80
	sd t6, a1, 88
	sd t6, a2, 96
	sd t6, a3, 104
	sd t6, a4, 112
	sd t6, a5, 120
	sd t6, a6, 128
	sd t6, a7, 136
	sd t6, s2, 144
	sd t6, s3, 152
	sd t6, s4, 160
	sd t6, s5, 168
	sd t6, s6, 176
	sd t6, s7, 184
	sd t6, s8, 192
	sd t6, s9, 200
	sd t6, s10, 208
	sd t6, s11, 216
	sd t6, t3, 224
	sd t6, t4, 232
	sd t6, t5, 240
	lla sp, trap_kstack
	lla a1, trap_save
	.long 0x342022F3
	mv a0, t0
	jal ra, trap_c
	lla t6, trap_save
	ld ra, t6, 8
	ld sp, t6, 16
	ld gp, t6, 24
	ld tp, t6, 32
	ld t0, t6, 40
	ld t1, t6, 48
	ld t2, t6, 56
	ld s0, t6, 64
	ld s1, t6, 72
	ld a0, t6, 80
	ld a1, t6, 88
	ld a2, t6, 96
	ld a3, t6, 104
	ld a4, t6, 112
	ld a5, t6, 120
	ld a6, t6, 128
	ld a7, t6, 136
	ld s2, t6, 144
	ld s3, t6, 152
	ld s4, t6, 160
	ld s5, t6, 168
	ld s6, t6, 176
	ld s7, t6, 184
	ld s8, t6, 192
	ld s9, t6, 200
	ld s10, t6, 208
	ld s11, t6, 216
	ld t3, t6, 224
	ld t4, t6, 232
	ld t5, t6, 240
	.long 0x30200073
mret_user:
	lla t2, saved_kra
	sd t2, ra, 0
	lla t2, saved_ksp
	sd t2, sp, 0
	mv t0, a0
	.long 0x34129073
	.long 0x300022F3
	addi t1, zero, 3
	slli t1, t1, 11
	xori t1, t1, -1
	and t0, t0, t1
	addi t1, zero, 3
	slli t1, t1, 13
	or t0, t0, t1
	.long 0x30029073
	mv sp, a1
	.long 0x30200073
jump_k:
	lla t0, saved_kra
	ld ra, t0, 0
	lla t0, saved_ksp
	ld sp, t0, 0
	jalr zero, ra, 0
read_mepc:
	.long 0x341022F3
	mv a0, t0
	jalr zero, ra, 0
read_mtval:
	.long 0x343022F3
	mv a0, t0
	jalr zero, ra, 0
write_mepc:
	mv t0, a0
	.long 0x34129073
	jalr zero, ra, 0
write_satp:
	mv t0, a0
	srli t0, t0, 12
	addi t1, zero, 8
	slli t1, t1, 60
	or t0, t0, t1
	.long 0x18029073
	.long 0x12000073
	jalr zero, ra, 0
kphys_dram:
	lla a0, _start
	jalr zero, ra, 0
kphys_ramfs:
	lla t0, _start
	addi t1, zero, 1
	slli t1, t1, 21
	add a0, t0, t1
	jalr zero, ra, 0
kphys_user:
	lla t0, _start
	addi t1, zero, 1
	slli t1, t1, 25
	add a0, t0, t1
	jalr zero, ra, 0
kphys_usp:
	lla t0, _start
	addi t1, zero, 1
	slli t1, t1, 30
	add a0, t0, t1
	jalr zero, ra, 0
kphys_kalloc:
	lla t0, _start
	addi t1, zero, 1
	slli t1, t1, 29
	add a0, t0, t1
	jalr zero, ra, 0
.data
.align 16
saved_kra:
	.space 8
saved_ksp:
	.space 8
trap_save:
	.space 256
trap_kstack_space:
	.space 8192
trap_kstack:
kstack_space:
	.space 65536
kstack:
