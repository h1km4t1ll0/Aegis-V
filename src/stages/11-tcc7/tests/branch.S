/* Conditional branches to labels (Tilck start.S, trap_entry.S, tcc's
   lib/atomic.S): forward and backward, named and numeric, the pseudo
   forms; t0 stays live across them. "call" returns (start.S calls C). */
.global br_count
/* br_count(n): n by a backward loop; -1 if t0 was clobbered */
br_count:
   li t0, 0x55
   li a1, 0
   beqz a0, br_done
1:
   addi a1, a1, 1
   addi a0, a0, -1
   bnez a0, 1b
br_done:
   li t1, 0x55
   bne t0, t1, br_bad
   mv a0, a1
   ret
br_bad:
   li a0, -1
   ret

.global br_cmp
/* br_cmp(a, b): bit 0 a < b, bit 1 a <= b, bit 2 a <u b, bit 3 a > b */
br_cmp:
   li a2, 0
   bge a0, a1, 2f
   ori a2, a2, 1
2:
   bgt a0, a1, 3f
   ori a2, a2, 2
3:
   bgeu a0, a1, 4f
   ori a2, a2, 4
4:
   ble a0, a1, 5f
   ori a2, a2, 8
5:
   mv a0, a2
   ret

.global br_call
/* br_call(): 41 + 1 through a call that returns */
br_call:
   addi sp, sp, -16
   sd ra, 8(sp)
   li a0, 41
   call br_inc
   ld ra, 8(sp)
   addi sp, sp, 16
   ret
br_inc:
   addi a0, a0, 1
   ret
