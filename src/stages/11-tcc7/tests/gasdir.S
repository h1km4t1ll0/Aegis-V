/* GAS forms Tilck's riscv .S files use (start.S, fpu.S, trap_entry.S,
   vdso.S, asm_defs.h). */
.global gas_li_ul
gas_li_ul:
   li a0, 0x00006000UL           /* C integer suffix */
   add a0, a0, 8                 /* add with an immediate: addi */
   srl a0, a0, 1                 /* srl with an immediate: srli */
   ret

.global gas_la_addend
gas_la_addend:
   la a0, gas_al8 + 8            /* defined .global label plus addend */
   ret

/* encodings only, never run (S-mode CSRs trap in U-mode) */
.global gas_enc
gas_enc:
   csrr a0, sstatus
   csrw sscratch, t0
   csrr a1, scause
   csrr a2, stval
   csrr a3, sepc
   fscsr t0
   add a3, a3, 8

.section .text.gas
.global gas_in_text_gas
gas_in_text_gas:
   li a0, 7
   ret

.data
.global gas_odd
gas_odd:
   .byte 1
.align 3                        /* 2^3 */
.global gas_al8
gas_al8:
   .dword 0x1122334455667788
   .dword 0
.global gas_words
gas_words:
   .word 0x11223344, 5           /* 32-bit words */
   .byte 9

.comm gas_common, 100, 8192
