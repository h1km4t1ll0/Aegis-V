.section .text,"ax",@progbits
.global _start, probe_low, probe_high, probe_fix
_start:
    ret
probe_low:
    nop
probe_high:
    nop
probe_fix:
    ret
.section __ex_table,"a",@progbits
.balign 4
.long probe_high - .
.long probe_fix - .
.short 1
.short 0
.long probe_low - .
.long probe_fix - .
.short 2
.short 0
.section .data,"aw",@progbits
.balign 4
.global main_extable_sort_needed
.type main_extable_sort_needed,@object
main_extable_sort_needed:
.long 1
.size main_extable_sort_needed,4
