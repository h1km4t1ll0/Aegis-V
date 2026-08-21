/* SMP bring-up for QEMU virt (HART_COUNT>1). Other boards link stubs. */
#ifndef BOARD_CONFIG_LOADED
#error "Board config was not included. Use make BOARD=<name>."
#endif
#ifndef HART_COUNT
#define HART_COUNT 1
#endif
#ifndef HART_STACK_SIZE
#define HART_STACK_SIZE 0x1000
#endif

#if HART_COUNT > 1

.global uart_lock
.global uart_unlock
.global oft_lock
.global oft_unlock
.global oft_unlock_if_held
.global smp_release_harts
.global smp_release_once
.global hart_kick
.global smp_wait
.global secondary_start
.global secondary_idle
.global hart_current
.global slot_code_tab
.global slot_heap_tab
.global slot_heap_end_tab
.global slot_args_tab

.extern yield
.extern resume_proc_context
.extern puts

.section .text

uart_lock:
        la t0, uart_lock_word
1:
        li t1, 1
        amoswap.d t1, t1, (t0)
        bnez t1, 1b
        fence rw, rw
        ret

uart_unlock:
        fence rw, rw
        la t0, uart_lock_word
        sd zero, 0(t0)
        ret

oft_lock:
        la t0, oft_lock_word
1:
        li t1, 1
        amoswap.d t1, t1, (t0)
        bnez t1, 1b
        fence rw, rw
        csrr t0, mhartid
        slli t0, t0, 3
        la t1, oft_held
        add t1, t1, t0
        li t2, 1
        sd t2, 0(t1)
        ret

oft_unlock:
        csrr t0, mhartid
        slli t0, t0, 3
        la t1, oft_held
        add t1, t1, t0
        sd zero, 0(t1)
        fence rw, rw
        la t0, oft_lock_word
        sd zero, 0(t0)
        ret

oft_unlock_if_held:
        csrr t0, mhartid
        slli t0, t0, 3
        la t1, oft_held
        add t1, t1, t0
        ld t2, 0(t1)
        beqz t2, 1f
        j oft_unlock
1:
        ret

smp_release_harts:
        la t3, secondary_start
        li t4, 1
smp_release_loop:
        li t5, HART_COUNT
        bge t4, t5, smp_release_done
        li t0, HART_MAILBOX
        slli t1, t4, 4
        add t0, t0, t1
        amoswap.d.aqrl zero, t3, (t0)
        li t1, KERNEL_STACK_TOP
        li t2, HART_STACK_SIZE
        mul t2, t2, t4
        sub t1, t1, t2
        sd t1, 8(t0)
        fence w, rw
        li t0, CLINT_BASE
        slli t1, t4, 2
        add t0, t0, t1
        li t1, 1
        sw t1, 0(t0)
        addi t4, t4, 1
        j smp_release_loop
smp_release_done:
        ret

/* First spawn (4-way TCC) brings harts 1–3 out of boot WFI. */
smp_release_once:
        la t0, harts_released
        ld t1, 0(t0)
        bnez t1, smp_release_once_done
        li t1, 1
        sd t1, 0(t0)
        j smp_release_harts
smp_release_once_done:
        ret

/* a0 = hart id (1..3), a1 = PCB. Wakes that hart. */
hart_kick:
        slli t0, a0, 3
        la t1, hart_current
        add t0, t1, t0
        sd a1, 0(t0)
        li t0, CLINT_BASE
        slli t1, a0, 2
        add t0, t0, t1
        li t1, 1
        sw t1, 0(t0)
        ret

smp_wait:
        li t0, 1
smp_wait_loop:
        li t1, HART_COUNT
        bge t0, t1, smp_wait_done
        slli t2, t0, 3
        la t3, hart_current
        add t2, t3, t2
        ld t2, 0(t2)
        beqz t2, smp_wait_next
        ld t3, 8(t2)
        bnez t3, smp_wait_spin
smp_wait_next:
        addi t0, t0, 1
        j smp_wait_loop
smp_wait_spin:
        li t4, 256
1:
        addi t4, t4, -1
        bnez t4, 1b
        j smp_wait_loop
smp_wait_done:
        ret

secondary_start:
        csrr t0, mhartid
        li t1, CLINT_BASE
        slli t2, t0, 2
        add t1, t1, t2
        sw zero, 0(t1)
        la t0, yield
#if BOARD_TRAP_MODE == BOARD_TRAP_M
        csrw mtvec, t0
        li t0, 8
        csrs mie, t0
#else
        csrw stvec, t0
#endif
        la a0, msg_smp_up
        call puts
secondary_idle:
        csrr t0, mhartid
        li t1, CLINT_BASE
        slli t2, t0, 2
        add t1, t1, t2
        sw zero, 0(t1)
        slli t0, t0, 3
        la t1, hart_current
        add t0, t1, t0
        ld t0, 0(t0)
        beqz t0, secondary_wfi
        ld t1, 8(t0)
        li t2, 0xffff
        bne t1, t2, secondary_wfi
        li t3, 8
        csrc mstatus, t3
        j resume_proc_context
secondary_wfi:
        li t3, 8
        csrs mstatus, t3
        wfi
        csrc mstatus, t3
        j secondary_idle

.align 3
harts_released:         .dword  0
hart_current:           .space  32
uart_lock_word:         .dword  0
oft_lock_word:          .dword  0
oft_held:               .space  32
slot_code_tab:
        .dword SLOT0_CODE, SLOT1_CODE, SLOT2_CODE, SLOT3_CODE
slot_heap_tab:
        .dword SLOT0_HEAP, SLOT1_HEAP, SLOT2_HEAP, SLOT3_HEAP
slot_heap_end_tab:
        .dword SLOT0_HEAP_END, SLOT1_HEAP_END, SLOT2_HEAP_END, SLOT3_HEAP_END
slot_args_tab:
        .dword SLOT0_ARGS, SLOT1_ARGS, SLOT2_ARGS, SLOT3_ARGS
msg_smp_up:             .string "[smp] hart up\r\n"

#else

.global uart_lock
.global uart_unlock
.global oft_lock
.global oft_unlock
.global oft_unlock_if_held
.global smp_release_harts
.global smp_release_once
.global hart_kick
.global smp_wait

.section .text
uart_lock:
uart_unlock:
oft_lock:
oft_unlock:
oft_unlock_if_held:
smp_release_harts:
smp_release_once:
hart_kick:
smp_wait:
        ret

#endif
