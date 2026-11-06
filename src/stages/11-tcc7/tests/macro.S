.macro load_42
   li a0, 40
   addi a0, a0, 2
.endm

.global macro_val
macro_val:
   load_42
   ret
