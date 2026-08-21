/* Board-specific constants are injected from boards/<BOARD>.h via config.mk.
 * Do not edit source files to port to a new board: create a new board config instead.
 */
#ifndef BOARD_CONFIG_LOADED
#error "Board config was not included. Use make BOARD=<name>."
#endif
#define BUFFER_SIZE             256
#define ARGV_MAX                16
#define ARGV_STRINGS_OFF        0x90
#define ARGS_BLOCK_SIZE         0x400

#define MAGIC_ADDRESS           0x100000
#define SHUTDOWN_SIG            0x5555
#define REBOOT_SIG              0x7777

#define PROC_STATE_UNUSED       0x0000
#define PROC_STATE_RUNNING      0xffff

#ifndef USER_CODE_BASE
#define USER_CODE_BASE 0
#endif
#ifndef USER_CODE_END
#define USER_CODE_END USER_CODE_BASE
#endif
#ifndef SLOT_CODE_SIZE
#define SLOT_CODE_SIZE 0x800000
#endif
#ifndef FILE_HOLE_GUARD
#define FILE_HOLE_GUARD 0x800000
#endif
#ifndef OPEN_FILES_BASE
#define OPEN_FILES_BASE OPEN_FILES_BASE
#endif
#ifndef OPEN_FILES_END
#define OPEN_FILES_END OPEN_FILES_END
#endif
#define OPEN_FILE_SIZE          0x20
#ifndef OPEN_FILE_MAX_FD
#define OPEN_FILE_MAX_FD        64
#endif
#define FILE_NAME_SLOT          0x400
#ifndef ARGS_BLOCK_BASE
#define ARGS_BLOCK_BASE ARGS_BLOCK_BASE
#endif

#define SYS_GETCWD              17
#define SYS_IOCTL               29
#define SYS_FACCESSAT           48
#define SYS_OPENAT              56
#define SYS_CLOSE               57
#define SYS_LSEEK               62
#define SYS_READ                63
#define SYS_WRITE               64
#define SYS_EXIT                93
#define SYS_EXIT_OLD            80
#define SYS_CLOCK_GETTIME       113
#define SYS_UNAME               160
#define SYS_GETTIMEOFDAY        169
#define SYS_BRK                 214

#ifndef MCAUSE_ECALL_U
#define MCAUSE_ECALL_U 8
#endif
#ifndef MCAUSE_ECALL_M
#define MCAUSE_ECALL_M 9
#endif
#ifndef MCAUSE_TIMER_M
#define MCAUSE_TIMER_M 0x8000000000000005
#endif
#ifndef MCAUSE_MSIP_M
#define MCAUSE_MSIP_M 0x8000000000000003
#endif
#ifndef HART_COUNT
#define HART_COUNT 1
#endif
#ifndef HART_STACK_SIZE
#define HART_STACK_SIZE 0x1000
#endif

.macro LOAD_CUR_PROC rd
#if HART_COUNT > 1
        csrr \rd, mhartid
        slli \rd, \rd, 3
        la t6, hart_current
        add \rd, t6, \rd
        ld \rd, 0(\rd)
#else
        la \rd, current_proc
        ld \rd, 0(\rd)
#endif
.endm

.macro STORE_CUR_PROC rs
#if HART_COUNT > 1
        csrr t6, mhartid
        slli t6, t6, 3
        la t5, hart_current
        add t6, t5, t6
        sd \rs, 0(t6)
#else
        la t6, current_proc
        sd \rs, 0(t6)
#endif
.endm

.macro SET_KERNEL_SP
#if HART_COUNT > 1
        csrr sp, mhartid
        li t6, HART_STACK_SIZE
        mul sp, sp, t6
        li t6, KERNEL_STACK_TOP
        sub sp, t6, sp
#else
        li sp, KERNEL_STACK_TOP
#endif
.endm

#define NEWLINE                 0xA
#define HASH                    0x23
#define SEMICOLON               0x3B
#define F_LOWERCASE             0x66
#define F_UPPERCASE             0x46
#define A_LOWERCASE             0x61
#define A_UPPERCASE             0x41
#define DIGIT_9                 0x39
#define DIGIT_0                 0x30

.section .text
.global _start

/* -----------------------------------------------------------------------
    Memory Map (RISC-V 64-bit, QEMU virt, RAM 0x80000000 - HEAP_END)

    0x80000000 - 0x8004ffff : boot (stage 1) + payload-исходники (hex0/hex1/M0)
                              boot читает исходники отсюда и компилирует simon
                              в зону STAGE2_BASE (см. PAYLOAD_OFFSET в boot.S)
    PROC_BASE - PROC_END : process descriptors (max 16, 4 KB each)
                                offset  len     description
                                0x000   0x008   PID
                                0x008   0x008   process state (0=UNUSED, 0xFFFF=RUNNING)
                                0x010   0x008   sp
                                0x018   0x008   ra
                                0x020   0x060   s0-s11
                                0x080   0x040   a0-a7
                                0x0c0   0x008   sepc (saved PC for trap return)
                                0x0c8   0x038   reserved
                                0x100   0x038   t0-t6 (saved on context switch)
                                0x0f0   0x008   name
                                0x0f8   0xf00   process stack
    FILE_NAME_BASE - FILE_NAME_END : file names (max 64, 1 KB each)
    STAGE2_BASE - 0x1208ffff : simon (stage 2) - boot компилирует сюда (64 KB)
                              реальный размер simon.bin ~10 KB, остаток - запас
    KERNEL_STACK_TOP - 0x120900ff : kernel stack zone (sp в trap handler = KERNEL_STACK_TOP,
                              растёт вниз; не пересекается с simon-кодом ниже)
    OPEN_FILES_BASE - 0x120902ff : open files table (16 x 32 bytes)
                                offset  len     description
                                0x00    0x08    global_file_index
                                0x08    0x08    current_offset
                                0x10    0x01    flags (bit 0 = used, bit 1 = writable)
                                0x11    0x0f    padding
    OPEN_FILES_END - 0x12097fff : reserved/unused
    ARGS_BLOCK_BASE - 0x120980ff : process args block (256 B)
                                0x00    0x08    argc
                                0x08    0x08    argv[0] ptr
                                0x10    0x08    argv[1] ptr
                                ...
                                0x40    0x08    argv[7] ptr
                                0x48    0x08    NULL terminator
                                0x50-0xff       argv strings
    0x12098100 - 0x1209ffff : reserved/unused
    FD_BASE - 0x121003ff : file descriptors (max 64, 16 B each)
                                offset  len     description
                                0x00    0x08    file address
                                0x08    0x08    file length
    FILE_DATA_BASE - 0x15ffffff : files data (создаваемые/компилируемые файлы)
    HEAP_BASE - 0x19ffffff : user heap (sys_brk), 64 MB
----------------------------------------------------------------------- */

_start:

    la t0, script_ptr
    sd a0, 0(t0)

    # После stage1 a0 должен указывать на files.pl, но на реальной плате
    # безопаснее проверить и при необходимости найти блок "src <file>" сканированием.
    call find_script_block
    call ddr_warmup

    la t0, file_ptr
    li t1, FILE_DATA_BASE
    sd t1, 0(t0)

    auipc sp, 0
    li t0, 0x100000
    add sp, sp, t0

    # K1-X/Banana Pi: U-Boot starts watchdogs before go().
    # Linux driver for this SoC disables/resets the watchdog through
    # WFAR/WSAR unlock + WCR/WMER writes. Do the same immediately,
    # then keep kicking while sitting in UART polling loops.
#if BOARD_WDT_ENABLE
    call wdt_init
#endif

    la t0, next_file_data
    li t1, FD_BASE
    sd t1, 0(t0)

    la t0, heap_ptr
    li t1, HEAP_BASE
    sd t1, 0(t0)

    li t0, OPEN_FILES_BASE
    li t1, OPEN_FILES_END
init_oft_loop:
    bge t0, t1, init_oft_done
    sd zero, 0(t0)
    addi t0, t0, 8
    j init_oft_loop
init_oft_done:

    la t0, yield
#if BOARD_TRAP_MODE == BOARD_TRAP_M
    csrw mtvec, t0
#else
    csrw stvec, t0
#endif

    # Banana Pi K1-X запускает payload через OpenSBI/U-Boot уже в S-mode.
    # Старый QEMU CLINT/machine-timer (0x200BFF8/0x2004000, mie/mstatus)
    # здесь недоступен, поэтому таймер выключен, а SiSH читает UART polling'ом.

    la a0, idle_proc
    la a1, ps_name_idle
    call create_process

    la a0, sish
    la a1, ps_name_sish
    call create_process

loop:
    j find_next

idle_proc:
    la a0, msg_kernel_panic_4
    call kernel_panic

# =====================================================
# find_script_block:
#   Ищет embedded-файлы в загруженном образе по сигнатуре "src <name>".
find_script_block:
    la t0, script_ptr
    ld t1, 0(t0)

    lb t2, 0(t1)
    li t3, 0x73              # 's'
    bne t2, t3, fs_scan_start
    lb t2, 1(t1)
    li t3, 0x72              # 'r'
    bne t2, t3, fs_scan_start
    lb t2, 2(t1)
    li t3, 0x63              # 'c'
    bne t2, t3, fs_scan_start
    lb t2, 3(t1)
    li t3, 0x20              # ' '
    beq t2, t3, fs_done

fs_scan_start:
    li t1, IMAGE_SCAN_BASE
    li t6, IMAGE_SCAN_END        # сканируем первые 9 МБ загруженного image
fs_scan_loop:
    bgeu t1, t6, fs_done
    lb t2, 0(t1)
    li t3, 0x73
    bne t2, t3, fs_next
    lb t2, 1(t1)
    li t3, 0x72
    bne t2, t3, fs_next
    lb t2, 2(t1)
    li t3, 0x63
    bne t2, t3, fs_next
    lb t2, 3(t1)
    li t3, 0x20
    bne t2, t3, fs_next

    lb t2, 4(t1)
    beqz t2, fs_next
    li t3, 0x0a
    beq t2, t3, fs_next

    addi t4, t1, 4
    li t5, 0
    li s6, 0
fs_name_check:
    li s7, 128
    bgeu t5, s7, fs_next
    lb t2, 0(t4)
    beqz t2, fs_next
    li t3, 0x2e              # '.'
    beq t2, t3, fs_mark_dot
    li t3, 0x0a
    beq t2, t3, fs_name_newline
    addi t4, t4, 1
    addi t5, t5, 1
    j fs_name_check
fs_mark_dot:
    li s6, 1
    addi t4, t4, 1
    addi t5, t5, 1
    j fs_name_check
fs_name_newline:
    beqz s6, fs_next
    la t0, script_ptr
    sd t1, 0(t0)
    j fs_done
fs_next:
    addi t1, t1, 1
    j fs_scan_loop
fs_done:
    ret

# Прогрев и очистка рабочей DDR-области Simon.
ddr_warmup:
    li t0, WARMUP1_START
    li t1, WARMUP1_END
    li t2, 0x1122334455667788
1:
    bgeu t0, t1, 2f
    sd t2, 0(t0)
    ld t3, 0(t0)
    sd zero, 0(t0)
    addi t0, t0, 8
    bltu t0, t1, 1b
2:
#if WARMUP2_ENABLE
    li t0, WARMUP2_START
    li t1, WARMUP2_END
3:
    bgeu t0, t1, 4f
    sd t2, 0(t0)
    ld t3, 0(t0)
    sd zero, 0(t0)
    addi t0, t0, 8
    bltu t0, t1, 3b
4:
#endif
    ret

# =====================================================
# Banana Pi / SpacemiT K1-X watchdog helper.
# Registers are the ky,x1-wdt / x1_wdt layout used by Linux:
#   unlock: WFAR=0xbaba, WSAR=0xeb10
#   WCR=1 resets counter, WMER=0 disables SoC WDT.
# PMIC watchdog must still be stopped from U-Boot if it is active.

wdt_unlock:
    li t0, SOC_WDT_BASE
    li t1, 0xbaba
    sw t1, WDT_WFAR(t0)
    li t1, 0xeb10
    sw t1, WDT_WSAR(t0)
    ret

wdt_kick:
    addi sp, sp, -24
    sd ra, 0(sp)
    sd t0, 8(sp)
    sd t1, 16(sp)

    call wdt_unlock
    li t0, SOC_WDT_BASE
    li t1, 1
    sw t1, WDT_WCR(t0)

    ld ra, 0(sp)
    ld t0, 8(sp)
    ld t1, 16(sp)
    addi sp, sp, 24
    ret

wdt_disable_soc:
    addi sp, sp, -24
    sd ra, 0(sp)
    sd t0, 8(sp)
    sd t1, 16(sp)

    call wdt_unlock
    li t0, SOC_WDT_BASE
    li t1, 1
    sw t1, WDT_WCR(t0)      # reset counter once before disabling

    call wdt_unlock
    li t0, SOC_WDT_BASE
    sw zero, WDT_WMER(t0)   # disable SoC WDT

    call wdt_unlock
    li t0, SOC_WDT_BASE
    sw zero, WDT_WSR(t0)    # clear status

    ld ra, 0(sp)
    ld t0, 8(sp)
    ld t1, 16(sp)
    addi sp, sp, 24
    ret

wdt_init:
    addi sp, sp, -16
    sd ra, 0(sp)
    sd t0, 8(sp)

    call wdt_disable_soc
    la t0, wdt_poll_counter
    li t1, 1000000
    sd t1, 0(t0)

    ld ra, 0(sp)
    ld t0, 8(sp)
    addi sp, sp, 16
    ret

wdt_tick:
    addi sp, sp, -32
    sd ra, 0(sp)
    sd t0, 8(sp)
    sd t1, 16(sp)
    sd t2, 24(sp)

    la t0, wdt_poll_counter
    ld t1, 0(t0)
    addi t1, t1, -1
    bgtz t1, 1f
    li t1, 1000000
    sd t1, 0(t0)
    call wdt_kick
    j 2f
1:
    sd t1, 0(t0)
2:
    ld ra, 0(sp)
    ld t0, 8(sp)
    ld t1, 16(sp)
    ld t2, 24(sp)
    addi sp, sp, 32
    ret

# =====================================================

sish:
    la t0, sish_parsed
    lb t2, 0(t0)
    bnez t2, sish_init

    la a0, greetings
    call puts

    la t0, script_ptr
    ld t1, 0(t0)
sish_loop:
    li t0, 0
    lb t0, 0(t1)
    beqz t0, sish_parse_done

    mv a0, t1
    la a1, str_src
    call cmdcmp
    bnez a0, sish_init

    addi t1, t1, 4
    call free_slot_file_name
    li t2, 0x0A
copy_name_loop:
    lb t3, 0(t1)
    beq t3, t2, skip_file_loop_head

    sb t3, 0(a0)

    addi a0, a0, 1
    addi t1, t1, 1
    j copy_name_loop
skip_file_loop_head:
    addi t1, t1, 1
    mv t4, t1
    li t5, 0
skip_file_loop:
    lb t3, 0(t1)
    beqz t3, skip_file_loop_done

    addi t5, t5, 1
    addi t1, t1, 1
    j skip_file_loop
skip_file_loop_done:
    li s1, 0x10
    mul s1, s1, a1
    li s2, FD_BASE
    add s2, s2, s1

    sd t4, 0(s2)
    sd t5, 8(s2)

    addi t1, t1, 1
    j sish_loop

sish_parse_done:
    la t0, sish_parsed
    li t1, 1
    sb t1, 0(t0)
    j sish_init

sish_init:

    la a0, cmd_prefix
    call puts

input_loop:
    # Без CLINT-таймера SiSH сам polling'ом читает UART.
    li t0, UART0_BASE
input_poll_uart:
#if BOARD_WDT_ENABLE
    call wdt_tick
#endif
    li t0, UART0_BASE
    lb t1, UART_LSR(t0)
    andi t1, t1, LSR_RX_READY
    beqz t1, input_poll_uart

    lb a0, 0(t0)

    li t2, 0x08
    beq a0, t2, input_backspace
    li t2, 0x7f
    beq a0, t2, input_backspace

    li t2, '\n'
    beq a0, t2, input_finish_line
    li t2, '\r'
    beq a0, t2, input_finish_line

    la t2, buffer_index
    lb t3, 0(t2)
    li t4, BUFFER_SIZE - 1
    bgeu t3, t4, input_loop

    la t4, buffer
    add t4, t4, t3
    sb a0, 0(t4)
    addi t3, t3, 1
    sb t3, 0(t2)

input_echo_wait:
    lb t5, UART_LSR(t0)
    andi t5, t5, LSR_TX_IDLE
    beqz t5, input_echo_wait
    sb a0, 0(t0)
    j input_loop

input_backspace:
    la t2, buffer_index
    lb t3, 0(t2)
    beqz t3, input_loop
    addi t3, t3, -1
    sb t3, 0(t2)
    la t4, buffer
    add t4, t4, t3
    sb zero, 0(t4)
    li a0, 0x08
    call putc
    li a0, ' '
    call putc
    li a0, 0x08
    call putc
    j input_loop

input_finish_line:
    la t2, buffer_index
    lb t3, 0(t2)
    la t4, buffer
    add t4, t4, t3
    sb zero, 0(t4)
    li t1, 1
    la t2, buffer_ready
    sb t1, 0(t2)
    j output

output:
    la a0, sym_newline
    call puts

    # Empty line: just redraw prompt, do not print fake echo/unknown command.
    la t0, buffer
    lb t1, 0(t0)
    beqz t1, output_done

    la a0, buffer
    la a1, shutdown_cmd
    call cmdcmp
    beqz a0, shutdown

    la a0, buffer 
    la a1, reboot_cmd
    call cmdcmp
    beqz a0, reboot

    la a0, buffer
    la a1, debug_cmd
    call cmdcmp
    beqz a0, debug

    la a0, buffer
    la a1, ls_cmd
    call cmdcmp
    beqz a0, ls

    la a0, buffer
    la a1, hex0_cmd
    call cmdcmp
    beqz a0, hex0

    la a0, buffer
    la a1, do_cmd
    call cmdcmp
    beqz a0, do

#if HART_COUNT > 1
    la a0, buffer
    la a1, spawn_cmd
    call cmdcmp
    beqz a0, spawn

    la a0, buffer
    la a1, wait_cmd
    call cmdcmp
    beqz a0, wait
#endif

    la a0, buffer
    la a1, view_cmd
    call cmdcmp
    beqz a0, view
    
    la a0, unknown_cmd
    call puts
    la a0, buffer
    call puts

output_done:
    la a0, buffer
    li a1, 0
    li a2, BUFFER_SIZE
    call memset

    la t0, buffer_ready
    li t1, 0
    sb t1, 0(t0)

    la t0, buffer_index
    li t1, 0
    sb t1, 0(t0)

    j sish_init

# ================= РЕАЛИЗАЦИЯ КОМАНД =================

shutdown:
    # Banana Pi/OpenSBI: SBI System Reset extension.
    # a0 = reset_type 0 => shutdown, a1 = reason 0.
    li a7, 0x53525354
    li a6, 0
    li a0, 0
    li a1, 0
    ecall
    j shutdown

reboot:
    # Banana Pi/OpenSBI: cold reboot.
    li a7, 0x53525354
    li a6, 0
    li a0, 1
    li a1, 0
    ecall
    j reboot 

debug:
    j debug

ls:
    li t0, FILE_NAME_BASE
    li t1, FILE_NAME_END
    li t2, FILE_NAME_SLOT
ls_loop:
    bge t0, t1, ls_done

    lb a0, 0(t0)
    beqz a0, ls_loop_skip

    mv a0, t0
    call puts 
    la a0, sym_newline
    call puts
ls_loop_skip:
    add t0, t0, t2
    j ls_loop
ls_done:
    j output_done

#if USER_CODE_BASE
/* If file_ptr would land in the user-code window, jump past it.
 * Save t0/t1: sys_write_file still needs them after the bump. */
file_bump_skip:
    addi sp, sp, -16
    sd t0, 0(sp)
    sd t1, 8(sp)
    li t0, USER_CODE_BASE
    li t1, FILE_HOLE_GUARD
    sub t0, t0, t1
    bltu a0, t0, file_bump_skip_done
    li t0, USER_CODE_END
    bgeu a0, t0, file_bump_skip_done
    mv a0, t0
file_bump_skip_done:
    ld t0, 0(sp)
    ld t1, 8(sp)
    addi sp, sp, 16
    ret

/* If the next write (a3+t5) sits in the user-code hole, move the file
 * past USER_CODE_END. a3 = data base, t5 = offset, t6 = FD slot. */
file_avoid_hole:
    addi sp, sp, -48
    sd ra, 0(sp)
    sd t0, 8(sp)
    sd t1, 16(sp)
    sd a0, 24(sp)
    sd a1, 32(sp)
    sd a2, 40(sp)
    add t0, a3, t5
    li t1, USER_CODE_BASE
    li a0, FILE_HOLE_GUARD
    sub t1, t1, a0
    bltu t0, t1, file_avoid_hole_done
    li t1, USER_CODE_END
    bgeu t0, t1, file_avoid_hole_done
    la a0, file_ptr
    ld a1, 0(a0)
    addi a1, a1, 3
    andi a1, a1, -4
    mv a0, a1
    call file_bump_skip
    mv a1, a0
    beq a1, a3, file_avoid_hole_done
    mv a0, a3
    mv a2, t5
    mv a3, a1
    call memcpy
    sd a3, 0(t6)
    add t0, a3, t5
    la t1, file_ptr
    sd t0, 0(t1)
file_avoid_hole_done:
    ld ra, 0(sp)
    ld t0, 8(sp)
    ld t1, 16(sp)
    ld a0, 24(sp)
    ld a1, 32(sp)
    ld a2, 40(sp)
    addi sp, sp, 48
    ret
#endif

hex0:
    la s1, buffer
    addi s1, s1, 5

    li t0, FILE_NAME_BASE
    li t1, FILE_NAME_END
    li t2, FILE_NAME_SLOT
    li t3, 0

hex0_find_file:
    bge t0, t1, hex0_done

    lb a0, 0(s1)
    beqz a0, hex0_done

    mv a0, t0
    mv a1, s1
    call strcmp
    beqz a0, hex0_file_founded

    addi t3, t3, 1
    add t0, t0, t2
    j hex0_find_file
hex0_file_founded:
    mv t1, t3
    li t2, FD_BASE
    li t3, 0x10
    mul t1, t1, t3
    add t1, t1, t2

    # Save source descriptor address across helper calls.
    # t-registers/a-registers are caller-saved and can be clobbered.
    mv s11, t1

    la a0, hex0_build
    call puts
    mv a0, t0
    call puts

    call free_slot_file_name
    # Save output file slot and name address across strlen/memcpy/puts.
    # Without this, test.bin descriptor can be written to a wrong slot,
    # and `do test.bin` may jump into the original hex0 text block.
    mv s9, a1
    mv s10, a0
    mv t2, s9
    mv t3, s10

    # Ищем позицию последней '.' в имени t0 (имя файла-источника)
    # a0 - позиция последней точки (-1 если не найдена)
    # t4 - текущий индекс
    # s6 - текущий байт
    mv a0, t0
    call strlen
    mv t4, a0           # t4 = длина имени-источника

    li a0, -1           # позиция последней точки (-1 = нет)
    li s7, 0            # текущий индекс
hex0_find_dot:
    bge s7, t4, hex0_find_dot_done

    add s6, t0, s7
    lb s6, 0(s6)
    li s8, 0x2e         # '.'
    bne s6, s8, hex0_find_dot_skip
    mv a0, s7 
hex0_find_dot_skip:
    addi s7, s7, 1
    j hex0_find_dot
hex0_find_dot_done:
    li s7, -1
    beq a0, s7, hex0_copy_full
    mv t4, a0  
hex0_copy_full:
    mv a2, t4
    mv a1, s10
    mv a0, t0
    call memcpy

    add t3, s10, t4
    li t4, 0x2e
    sb t4, 0(t3)
    li t4, 0x62
    sb t4, 1(t3)
    li t4, 0x69
    sb t4, 2(t3)
    li t4, 0x6e
    sb t4, 3(t3)
    sb zero, 4(t3)            # terminate new filename: test.bin\0

    ld t4, 0(s11)
    ld t5, 8(s11)

    la t0, file_ptr
    ld t1, 0(t0)
#if USER_CODE_BASE
    mv a0, t1
    call file_bump_skip
    mv t1, a0
    la t0, file_ptr
    sd t1, 0(t0)
#endif

    li t0, 0x10
    mul t2, s9, t0           # descriptor index must match output name slot
    li t0, FD_BASE
    add t2, t2, t0

    # t1 - file pointer
    # t2 - address to new descriptor
    # t3 - address to new name
    # t4 - address to src file
    # t5 - src file length

    sd t1, 0(t2)

    mv s1, t4
    li s3, 0
    mv s5, t1
    li t6, 0

    # s1 - source address
    # s2 - read byte
    # s3 - digit read flag
    # s4 - digit buffer
    # s5 - write address
    read_loop:
            lb s2, (s1)
            addi s1, s1, 1
            beqz s2, read_loop_exit

            li s6, HASH
            beq s2, s6, skip_comment
            li s6, SEMICOLON
            beq s2, s6, skip_comment
                
            li s6, (F_LOWERCASE + 1)
            bge s2, s6, read_loop_cont

            li s6, A_LOWERCASE
            bge s2, s6, handle_lower

            li s6, (F_UPPERCASE + 1)
            bge s2, s6, read_loop_cont

            li s6, A_UPPERCASE
            bge s2, s6, handle_upper

            li s6, (DIGIT_9 + 1)
            bge s2, s6, read_loop_cont

            li s6, DIGIT_0
            bge s2, s6, handle_digit

            j read_loop_cont

            skip_comment:                
                    li s6, NEWLINE
            skip_comment_loop:
                    lb s2, (s1)
                    addi s1, s1, 1
                    beq s2, s6, skip_comment_loop_end
                    j skip_comment_loop
            skip_comment_loop_end:
                    j read_loop_cont                         
                
            handle_digit:
                    addi s2, s2, -(DIGIT_0)
                    j general_handle                        
            handle_lower:
                    addi s2, s2, -(A_LOWERCASE - 10)
                    j general_handle
            handle_upper:
                    addi s2, s2, -(A_UPPERCASE - 10)

            general_handle:
                    slli s4, s4, 4
                    andi s4, s4, 0xFF
                    add s4, s4, s2
                    beqz s3, set_flag
                    sb s4, (s5)
                    addi s5, s5, 1
                    addi t6, t6, 1       # count decoded BYTES, not hex digits

                    set_flag:
                    not s3, s3

            read_loop_cont:
                j read_loop
        read_loop_exit:
                beqz s3, read_loop_end
                andi s4, s4, 0xF
                sb s4, (s5)
                addi s5, s5, 1
                addi t6, t6, 1
        read_loop_end:
            sd t6, 8(t2)

            # Advance file_ptr for the next generated file, 16-byte aligned.
            addi s5, s5, 15
            li t0, -16
            and s5, s5, t0
#if USER_CODE_BASE
            mv a0, s5
            call file_bump_skip
            mv s5, a0
#endif
            la t0, file_ptr
            sd s5, 0(t0)

            la a0, sym_newline
            call puts
            la a0, hex0_builded
            call puts
    
hex0_done:
    j output_done




do:
    # v6: every launched user program gets a clean heap base.
    # QEMU version effectively starts tiny tools in a fresh environment;
    # on Banana we run them one-by-one from the shell, so leaving brk from
    # the previous compiler stage can make M0 walk stale heap/list data.
    la t0, heap_ptr
    li t1, HEAP_BASE
    sd t1, 0(t0)

    la t0, progress_write_bytes
    sd zero, 0(t0)
    la t0, progress_last_write_kb
    sd zero, 0(t0)
    la t0, progress_last_heap_kb
    sd zero, 0(t0)
    la t0, progress_last_pct
    li t1, -1
    sd t1, 0(t0)
    la t0, progress_last_label
    sd zero, 0(t0)

    # Clear the whole argv block before parsing this command.
    li a0, ARGS_BLOCK_BASE
    li a1, 0
    li a2, ARGS_BLOCK_SIZE
    call memset

    la s1, buffer
    addi s1, s1, 3

    li s2, ARGS_BLOCK_BASE
    addi s2, s2, ARGV_STRINGS_OFF
    li s3, ARGS_BLOCK_BASE
    addi s3, s3, 0x08
    li s4, 0

    # hex2-1 needs argv[0..9] (bin -A riscv64 --little-endian -B 0x82000000 -f in -o out);
    # strings live at +0x90 so they do not overlap 16 argv pointers.
do_parse_loop:
    li t0, ARGV_MAX
    bge s4, t0, do_parse_done

    lb t0, 0(s1)
    beqz t0, do_parse_done

    sd s2, 0(s3)
    addi s3, s3, 8

do_copy_word:
    lb t0, 0(s1)
    beqz t0, do_word_end
    li t1, ' '
    beq t0, t1, do_word_end

    sb t0, 0(s2)
    addi s1, s1, 1
    addi s2, s2, 1
    j do_copy_word

do_word_end:
    sb zero, 0(s2)
    addi s2, s2, 1
    addi s4, s4, 1

    lb t0, 0(s1)
    beqz t0, do_parse_done
    addi s1, s1, 1
    j do_parse_loop

do_parse_done:
    beqz s4, do_done

    li t0, ARGS_BLOCK_BASE
    sd s4, 0(t0)
    sd zero, 0(s3)

    li t0, ARGS_BLOCK_BASE
    ld a0, 0x08(t0)
#if HART_COUNT > 1
    call oft_lock
#endif
    call find_file_by_name

    li t1, -1
    beq a0, t1, do_file_miss

    li t1, 0x10
    mul a0, a0, t1
    li t1, FD_BASE
    add a0, a0, t1
#if USER_CODE_BASE
    # hex2 emits absolute &label against USER_CODE_BASE. Copy the binary
    # there so pointers in cc.bin (and later C programs) match RAM.
    ld t2, 0(a0)
    ld a2, 8(a0)
    li t3, SLOT_CODE_SIZE
    bltu a2, t3, do_memcpy_len_ok
    mv a2, t3
do_memcpy_len_ok:
    mv s11, a2
    mv a0, t2
    li a1, USER_CODE_BASE
    call memcpy
    li a0, USER_CODE_BASE
    add a0, a0, s11
    li t0, USER_CODE_BASE
    li t1, SLOT_CODE_SIZE
    add t0, t0, t1
    bgeu a0, t0, do_code_flush
    sub a2, t0, a0
    li t1, 0x2000
    bltu a2, t1, do_zero_tail_ok
    mv a2, t1
do_zero_tail_ok:
    li a1, 0
    call memset
do_code_flush:
    li t0, USER_CODE_BASE
    lw t1, 0(t0)
    sw t1, 0(t0)
    fence rw, rw
    fence.i
    li a0, USER_CODE_BASE
#else
    ld a0, 0(a0)
#endif
#if HART_COUNT > 1
    call oft_unlock
#endif

    li t0, ARGS_BLOCK_BASE
    ld a1, 0x08(t0)

    # Banana Pi / real RISC-V: files are generated by stores and then
    # immediately executed with `do`.  The instruction cache is not required
    # to see those stores until FENCE.I, so without this hex2/M0 outputs can
    # trap on stale/illegal instructions.
    fence.i

    li a2, ARGS_BLOCK_BASE

    la t0, user_running
    li t1, 1
    sd t1, 0(t0)

    call create_user_process
    la t0, user_proc
    sd a0, 0(t0)
    mv s0, a0
#if HART_COUNT > 1
    li t1, HEAP_BASE
    sd t1, 0xd0(s0)
    sd t1, 0xd8(s0)
    li t1, HEAP_END
    sd t1, 0xe0(s0)
    li t1, USER_CODE_BASE
    sd t1, 0xe8(s0)
#endif

    # Без таймера новый процесс сам не запустится.
    la a0, buffer
    li a1, 0
    li a2, BUFFER_SIZE
    call memset
    la t0, buffer_ready
    sb zero, 0(t0)
    la t0, buffer_index
    sb zero, 0(t0)

    STORE_CUR_PROC s0
    mv t0, s0
    j resume_proc_context

do_file_miss:
#if HART_COUNT > 1
    call oft_unlock
#endif
do_done:
    j output_done

#if HART_COUNT > 1
spawn:
    li s0, 1
spawn_find:
    li t0, HART_COUNT
    bge s0, t0, spawn_full
    slli t1, s0, 3
    la t2, hart_current
    add t1, t2, t1
    ld t1, 0(t1)
    beqz t1, spawn_got
    ld t2, 8(t1)
    beqz t2, spawn_got
    addi s0, s0, 1
    j spawn_find
spawn_full:
    la a0, msg_spawn_full
    call puts
    j output_done
spawn_got:
    slli t0, s0, 3
    la t1, slot_args_tab
    add t1, t1, t0
    ld s5, 0(t1)
    mv a0, s5
    li a1, 0
    li a2, ARGS_BLOCK_SIZE
    call memset
    la s1, buffer
    addi s1, s1, 6
    mv s2, s5
    addi s2, s2, ARGV_STRINGS_OFF
    mv s3, s5
    addi s3, s3, 8
    li s4, 0
spawn_parse_loop:
    li t0, ARGV_MAX
    bge s4, t0, spawn_parse_done
    lb t0, 0(s1)
    beqz t0, spawn_parse_done
    sd s2, 0(s3)
    addi s3, s3, 8
spawn_copy_word:
    lb t0, 0(s1)
    beqz t0, spawn_word_end
    li t1, ' '
    beq t0, t1, spawn_word_end
    sb t0, 0(s2)
    addi s1, s1, 1
    addi s2, s2, 1
    j spawn_copy_word
spawn_word_end:
    sb zero, 0(s2)
    addi s2, s2, 1
    addi s4, s4, 1
    lb t0, 0(s1)
    beqz t0, spawn_parse_done
    addi s1, s1, 1
    j spawn_parse_loop
spawn_parse_done:
    beqz s4, do_done
    sd s4, 0(s5)
    sd zero, 0(s3)
    ld a0, 8(s5)
    call oft_lock
    call find_file_by_name
    li t1, -1
    beq a0, t1, spawn_file_miss
    li t1, 0x10
    mul a0, a0, t1
    li t1, FD_BASE
    add a0, a0, t1
    ld t2, 0(a0)
    ld a2, 8(a0)
    li t3, SLOT_CODE_SIZE
    bltu a2, t3, 1f
    mv a2, t3
1:
    mv a0, t2
    slli t0, s0, 3
    la t1, slot_code_tab
    add t1, t1, t0
    ld a1, 0(t1)
    addi sp, sp, -16
    sd a1, 0(sp)
    call memcpy
    ld a0, 0(sp)
    addi sp, sp, 16
    call oft_unlock
    fence rw, rw
    fence.i
    sfence.vma
    fence.i
    ld a1, 8(s5)
    mv a2, s5
    call create_user_process
    slli t0, s0, 3
    la t1, slot_heap_tab
    add t1, t1, t0
    ld t1, 0(t1)
    sd t1, 0xd0(a0)
    sd t1, 0xd8(a0)
    la t1, slot_heap_end_tab
    add t1, t1, t0
    ld t1, 0(t1)
    sd t1, 0xe0(a0)
    la t1, slot_code_tab
    add t1, t1, t0
    ld t1, 0(t1)
    sd t1, 0xe8(a0)
    mv a1, a0
    mv a0, s0
    call smp_release_once
    call hart_kick
    j output_done
spawn_file_miss:
    call oft_unlock
    j do_done
wait:
    call smp_wait
    j output_done
#endif

view:
    la s1, buffer
    addi s1, s1, 5

    li t0, FILE_NAME_BASE
    li t1, FILE_NAME_END
    li t2, FILE_NAME_SLOT
    li t3, 0
view_find_file:
    bge t0, t1, view_done

    mv a0, t0
    mv a1, s1
    call strcmp
    beqz a0, view_file_founded

    addi t3, t3, 1
    add t0, t0, t2
    j view_find_file

view_file_founded:
    mv t1, t3
    li t2, FD_BASE
    li t3, 0x10
    mul t1, t1, t3
    add t1, t1, t2

    ld t4, 0(t1)
    ld t5, 8(t1)

    la a0, sym_newline
    call puts

    mv s2, t4
    mv s3, t5
view_print_loop:
    beqz s3, view_done

    lb a0, 0(s2)

    li t0, 0x0A
    bne a0, t0, view_print_byte
    li a0, 0x0D
    call putc
    li a0, 0x0A

view_print_byte:
    call putc

    addi s2, s2, 1
    addi s3, s3, -1
    j view_print_loop

view_done:
    j output_done

# =====================================================

# ====================== УТИЛИТЫ ======================
.align 4
.global yield
yield:
#if BOARD_TRAP_MODE == BOARD_TRAP_M
    csrw mscratch, t0
#else
    csrw sscratch, t0
#endif
#if HART_COUNT > 1 && BOARD_TRAP_MODE == BOARD_TRAP_M
    csrr t0, mcause
    li t1, MCAUSE_MSIP_M
    bne t0, t1, yield_not_msip
    csrr t0, mhartid
    li t1, CLINT_BASE
    slli t2, t0, 2
    add t1, t1, t2
    sw zero, 0(t1)
    bnez t0, secondary_idle
    csrr t0, mscratch
    mret
yield_not_msip:
    csrr t0, mscratch
#endif
    LOAD_CUR_PROC t0
    beqz t0, yield_no_save

    sd sp, 0x10(t0)
#if BOARD_TRAP_MODE == BOARD_TRAP_M
    csrr t1, mepc
#else
    csrr t1, sepc
#endif
    sd t1, 0xc0(t0)
    sd ra, 0x18(t0)
    sd s0, 0x20(t0); sd s1, 0x28(t0); sd s2, 0x30(t0)
    sd s3, 0x38(t0); sd s4, 0x40(t0); sd s5, 0x48(t0)
    sd s6, 0x50(t0); sd s7, 0x58(t0); sd s8, 0x60(t0)
    sd s9, 0x68(t0); sd s10, 0x70(t0); sd s11, 0x78(t0)
    sd a0, 0x80(t0); sd a1, 0x88(t0); sd a2, 0x90(t0)
    sd a3, 0x98(t0); sd a4, 0xa0(t0); sd a5, 0xa8(t0)
    sd a6, 0xb0(t0); sd a7, 0xb8(t0)
    sd t1, 0x108(t0); sd t2, 0x110(t0); sd t3, 0x118(t0)
    sd t4, 0x120(t0); sd t5, 0x128(t0); sd t6, 0x130(t0)
#if BOARD_TRAP_MODE == BOARD_TRAP_M
    csrr t1, mscratch
#else
    csrr t1, sscratch
#endif
    sd t1, 0x100(t0)

yield_no_save:
#if BOARD_TRAP_MODE == BOARD_TRAP_M
    csrr t6, mcause
#else
    csrr t6, scause
#endif
    li t5, MCAUSE_ECALL_U
    beq t6, t5, syscall_entry
    li t5, MCAUSE_ECALL_M
    beq t6, t5, syscall_entry

    li t5, MCAUSE_TIMER_M
    beq t6, t5, yield_timer

    j trap_panic

yield_timer:
    li t0, UART0_BASE
    lb t1, UART_LSR(t0)
    andi t1, t1, LSR_RX_READY
    beqz t1, yield_processes

    lb a0, 0(t0)

    li t2, '\n'
    beq a0, t2, yield_prepare_input

    li t2, '\r'
    beq a0, t2, yield_prepare_input

yield_regular_char:
    la t2, buffer_index
    lb t3, 0(t2)
    la t2, buffer
    add t2, t2, t3

    sb a0, 0(t2)
    addi t3, t3, 1
    la t2, buffer_index
    sb t3, 0(t2)

    sb a0, 0(t0)
    j yield_processes
yield_prepare_input:
    la t2, buffer_index
    lb t3, 0(t2)
    la t2, buffer
    add t2, t2, t3

    sb zero, 0(t2)
    addi t3, t3, 1
    la t2, buffer_index
    sb t3, 0(t2)

    li t2, 1
    la t3, buffer_ready
    sb t2, 0(t3)

yield_processes:
    la t0, user_running
    ld t0, 0(t0)
    beqz t0, find_next

    la t0, user_proc
    ld t0, 0(t0)
    beqz t0, find_next

    ld t1, 0x8(t0)
    beqz t1, find_next

    STORE_CUR_PROC t0

    # Banana Pi K1-X: QEMU CLINT timer rearm отключён.

    j resume_proc_context

syscall_entry:
    LOAD_CUR_PROC t0
    beqz t0, syscall_no_proc

    sd sp, 0x10(t0)
#if BOARD_TRAP_MODE == BOARD_TRAP_M
    csrr t1, mepc
#else
    csrr t1, sepc
#endif
    addi t1, t1, 4
    sd t1, 0xc0(t0)

    sd ra, 0x18(t0)

    sd s0, 0x20(t0); sd s1, 0x28(t0); sd s2, 0x30(t0)
    sd s3, 0x38(t0); sd s4, 0x40(t0); sd s5, 0x48(t0)
    sd s6, 0x50(t0); sd s7, 0x58(t0); sd s8, 0x60(t0)
    sd s9, 0x68(t0); sd s10, 0x70(t0); sd s11, 0x78(t0)

    sd a0, 0x80(t0); sd a1, 0x88(t0); sd a2, 0x90(t0)
    sd a3, 0x98(t0); sd a4, 0xa0(t0); sd a5, 0xa8(t0)
    sd a6, 0xb0(t0); sd a7, 0xb8(t0)

    SET_KERNEL_SP

    # === Диспетчер ===
    li t1, SYS_EXIT
    beq a7, t1, sys_exit
    li t1, SYS_EXIT_OLD
    beq a7, t1, sys_exit

    li t1, SYS_WRITE
    beq a7, t1, sys_write

    li t1, SYS_READ
    beq a7, t1, sys_read

    li t1, SYS_OPENAT
    beq a7, t1, sys_openat

    li t1, SYS_CLOSE
    beq a7, t1, sys_close

    li t1, SYS_LSEEK
    beq a7, t1, sys_lseek

    li t1, SYS_BRK
    beq a7, t1, sys_brk

    li t1, SYS_CLOCK_GETTIME
    beq a7, t1, sys_clock_gettime

    li t1, SYS_GETTIMEOFDAY
    beq a7, t1, sys_gettimeofday

    li t1, SYS_GETCWD
    beq a7, t1, sys_getcwd

    li t1, SYS_UNAME
    beq a7, t1, sys_uname

    li t1, SYS_FACCESSAT
    beq a7, t1, sys_faccessat

    li t1, SYS_IOCTL
    beq a7, t1, syscall_return_minus_one

    j syscall_return_minus_one

syscall_no_proc:
    la a0, msg_syscall_no_proc
    j kernel_panic

syscall_return_zero:
    LOAD_CUR_PROC t0
    sd zero, 0x80(t0)
    j syscall_resume_current

syscall_return_minus_one:
    LOAD_CUR_PROC t0
    li t1, -1
    sd t1, 0x80(t0)
    j syscall_resume_current

syscall_resume_current:
#if HART_COUNT > 1
    call oft_unlock_if_held
#endif
    LOAD_CUR_PROC t0

    # Banana Pi K1-X: QEMU CLINT timer rearm отключён.

    # v7: возвращая управление после ecall, надо восстановить ВСЕ user-регистры,
    # включая t0-t6.  Раньше мы сохраняли t-регистры в yield, но syscall-return
    # их не восстанавливал.  Простые hex0/hex1/hex2 это терпели, а M0 держит
    # временные указатели в t-регистрах через read/write/open/lseek и мог уходить
    # в бесконечный цикл или читать с нулевого адреса.

#if BOARD_TRAP_MODE == BOARD_TRAP_M
    # M-mode boards, e.g. Lichee full-flash: keep launched tools in M-mode.
    # Without PMP/delegation, ECALL from U-mode would not return to our handler.
    li t1, 0x1800
    csrs mstatus, t1
#else
    # S-mode boards, e.g. Banana Pi through OpenSBI/U-Boot.
    # 0xc8 == 1 for user binary => SPP=0 => return to U-mode.
    ld t1, 0xc8(t0)
    bnez t1, syscall_resume_to_user
    li t1, 0x100
    csrs sstatus, t1
    j syscall_resume_restore_regs
syscall_resume_to_user:
    li t1, 0x100
    csrc sstatus, t1
#endif

syscall_resume_restore_regs:
    ld t1, 0xc0(t0)
#if BOARD_TRAP_MODE == BOARD_TRAP_M
    csrw mepc, t1
#else
    csrw sepc, t1
#endif

    ld sp, 0x10(t0)
    ld ra, 0x18(t0)
    ld s0, 0x20(t0); ld s1, 0x28(t0); ld s2, 0x30(t0)
    ld s3, 0x38(t0); ld s4, 0x40(t0); ld s5, 0x48(t0)
    ld s6, 0x50(t0); ld s7, 0x58(t0); ld s8, 0x60(t0)
    ld s9, 0x68(t0); ld s10, 0x70(t0); ld s11, 0x78(t0)

    ld a0, 0x80(t0); ld a1, 0x88(t0); ld a2, 0x90(t0)
    ld a3, 0x98(t0); ld a4, 0xa0(t0); ld a5, 0xa8(t0)
    ld a6, 0xb0(t0); ld a7, 0xb8(t0)

    # Restore t0 last via scratch CSR, because t0 is still the process descriptor.
    ld t1, 0x100(t0)
#if BOARD_TRAP_MODE == BOARD_TRAP_M
    csrw mscratch, t1
#else
    csrw sscratch, t1
#endif
    ld t1, 0x108(t0); ld t2, 0x110(t0); ld t3, 0x118(t0)
    ld t4, 0x120(t0); ld t5, 0x128(t0); ld t6, 0x130(t0)
#if BOARD_TRAP_MODE == BOARD_TRAP_M
    csrr t0, mscratch
    mret
#else
    csrr t0, sscratch
    sret
#endif

/*
    Системный вызов exit (93)

    Аргументы:
    a0 - код возврата (игнорируем)

    Помечает процесс как UNUSED и снимает флаг user_running.
*/
sys_exit:
    LOAD_CUR_PROC t0
    sd zero, 0x8(t0)

#if HART_COUNT > 1
    csrr t2, mhartid
    bnez t2, sys_exit_worker
#endif

    la t1, user_running
    sd zero, 0(t1)

#if HART_COUNT > 1
    call oft_unlock_if_held
    li t0, 1
sys_exit_check_workers:
    li t1, HART_COUNT
    bge t0, t1, sys_exit_wipe
    slli t2, t0, 3
    la t3, hart_current
    add t2, t3, t2
    ld t2, 0(t2)
    beqz t2, sys_exit_check_next
    ld t3, 8(t2)
    bnez t3, sys_exit_skip_wipe
sys_exit_check_next:
    addi t0, t0, 1
    j sys_exit_check_workers
sys_exit_skip_wipe:
    li t6, 0
    STORE_CUR_PROC t6
    j output_done
sys_exit_wipe:
#endif

    li t1, OPEN_FILES_BASE
    li t2, OPEN_FILES_END
sys_exit_close_loop:
    bge t1, t2, sys_exit_close_done
    sd zero, 0(t1)
    sd zero, 8(t1)
    sd zero, 16(t1)
    sd zero, 24(t1)
    addi t1, t1, OPEN_FILE_SIZE
    j sys_exit_close_loop
sys_exit_close_done:

    li t6, 0
    STORE_CUR_PROC t6

    # Banana Pi port: no CLINT timer, so the interactive `do` command
    # gives control to the user process manually.  After user exit we must
    # return directly to SiSH, not to scheduler/find_next, otherwise SiSH is
    # restarted from its entry point and embedded files can be parsed again.
    j output_done

#if HART_COUNT > 1
sys_exit_worker:
    call oft_unlock_if_held
    li t6, 0
    STORE_CUR_PROC t6
    j secondary_idle
#endif

/*
    Системный вызов write (64)

    Аргументы:
    a0 - файловый дескриптор (1/2 - stdout, >=3 - файл)
    a1 - адрес буфера
    a2 - количество байт

    Вывод:
    a0 - количество записанных байт
*/
sys_write:
    LOAD_CUR_PROC t0
    ld t1, 0x80(t0)
    ld t2, 0x88(t0)
    ld t3, 0x90(t0)

    li t4, 1
    beq t1, t4, sys_write_stdout
    li t4, 2
    beq t1, t4, sys_write_stdout

    li t4, 3
    bge t1, t4, sys_write_file

    j syscall_return_minus_one

sys_write_stdout:
    li t4, UART0_BASE
    li t6, 0
sys_write_loop:
    beqz t3, sys_write_done

    lb t5, 0(t2)
    li a0, 0x0A
    bne t5, a0, sys_write_putc
    li a0, 0x0D
sys_write_wait_cr:
    lb a3, UART_LSR(t4)
    andi a3, a3, LSR_TX_IDLE
    beqz a3, sys_write_wait_cr
    sb a0, 0(t4)

sys_write_putc:
sys_write_wait_tx:
    lb a3, UART_LSR(t4)
    andi a3, a3, LSR_TX_IDLE
    beqz a3, sys_write_wait_tx
    sb t5, 0(t4)

    addi t2, t2, 1
    addi t3, t3, -1
    addi t6, t6, 1
    j sys_write_loop

sys_write_done:
    sd t6, 0x80(t0)
    j syscall_resume_current

/*
    Запись в открытый файл (вызывается из sys_write при fd >= 3)

    Аргументы:
    t1 - fd, t2 - буфер, t3 - количество байт
*/
sys_write_file:
#if HART_COUNT > 1
    addi sp, sp, -32
    sd t1, 0(sp)
    sd t2, 8(sp)
    sd t3, 16(sp)
    call oft_lock
    ld t1, 0(sp)
    ld t2, 8(sp)
    ld t3, 16(sp)
    addi sp, sp, 32
#endif
    addi t1, t1, -3
    li t4, OPEN_FILE_SIZE
    mul t1, t1, t4
    li t4, OPEN_FILES_BASE
    add t1, t1, t4

    lb t4, 0x10(t1)
    andi t5, t4, 0x01
    beqz t5, syscall_return_minus_one
    andi t5, t4, 0x02
    beqz t5, syscall_return_minus_one

    ld t4, 0x00(t1)
    ld t5, 0x08(t1)

    li t6, 0x10
    mul t6, t4, t6
    li a3, FD_BASE
    add t6, t6, a3
    ld a3, 0(t6)
    ld a4, 8(t6)

    li a5, 0
sys_write_file_loop:
    beq a5, t3, sys_write_file_done

    lb a6, 0(t2)
    add a7, a3, t5
#if USER_CODE_BASE
    call file_avoid_hole
    add a7, a3, t5
#endif
    sb a6, 0(a7)

    addi t5, t5, 1

    ble t5, a4, sys_write_file_no_grow
    mv a4, t5
    la a6, file_ptr
    ld a7, 0(a6)
    addi a7, a7, 1
#if USER_CODE_BASE
    mv a0, a7
    call file_bump_skip
    mv a7, a0
#endif
    sd a7, 0(a6)
sys_write_file_no_grow:

    addi t2, t2, 1
    addi a5, a5, 1
    j sys_write_file_loop

sys_write_file_done:
    sd t5, 0x08(t1)
    sd a4, 8(t6)

    la t0, progress_write_bytes
    ld t1, 0(t0)
    add t1, t1, a5
    sd t1, 0(t0)

    LOAD_CUR_PROC t0
    sd a5, 0x80(t0)

    mv a0, t1
    srli a0, a0, 10
    call progress_write_kb
    j syscall_resume_current

/*
    Системный вызов openat (56)

    Аргументы:
    a1 - имя файла (путь)
    a2 - флаги (0 = O_RDONLY, 0x241 = O_CREAT|O_WRONLY|O_TRUNC)

    Вывод:
    a0 - файловый дескриптор (>= 3) или -1 при ошибке
*/
sys_openat:
#if HART_COUNT > 1
    call oft_lock
#endif
    LOAD_CUR_PROC t0

    ld a0, 0x88(t0)
    ld t1, 0x90(t0)

    beqz t1, sys_openat_rdonly

    andi t2, t1, 0x40
    bnez t2, sys_openat_creat

    j syscall_return_minus_one

sys_openat_rdonly:
    call find_file_by_name
    li t1, -1
    beq a0, t1, syscall_return_minus_one

    addi sp, sp, -8
    sd a0, 0(sp)

    call alloc_open_file_slot
    li t1, -1
    beq a0, t1, syscall_openat_fail

    li t1, OPEN_FILE_SIZE
    mul t3, a0, t1
    li t1, OPEN_FILES_BASE
    add t3, t3, t1

    ld t2, 0(sp)
    addi sp, sp, 8

    sd t2, 0x00(t3)
    sd zero, 0x08(t3)
    li t1, 0x01
    sb t1, 0x10(t3)

    addi a0, a0, 3

    LOAD_CUR_PROC t0
    sd a0, 0x80(t0)
    j syscall_resume_current

sys_openat_creat:
    addi sp, sp, -32
    sd a0, 0(sp)

    call find_file_by_name
    li t1, -1
    bne a0, t1, sys_openat_creat_existing

    call free_slot_file_name
    li t1, -1
    beq a1, t1, sys_openat_creat_fail
    sd a1, 8(sp)

    mv t0, a0
    ld t1, 0(sp)
sys_openat_copy_name:
    lb t2, 0(t1)
    sb t2, 0(t0)
    beqz t2, sys_openat_copy_name_done
    addi t1, t1, 1
    addi t0, t0, 1
    j sys_openat_copy_name
sys_openat_copy_name_done:

    la t0, file_ptr
    ld t1, 0(t0)
    addi t1, t1, 3
    andi t1, t1, -4
#if USER_CODE_BASE
    mv a0, t1
    call file_bump_skip
    mv t1, a0
    la t0, file_ptr
#endif
    sd t1, 0(t0)

    ld t2, 8(sp)
    li t3, 0x10
    mul t3, t2, t3
    li t4, FD_BASE
    add t3, t3, t4

    sd t1, 0(t3)
    sd zero, 8(t3)

    j sys_openat_creat_open

sys_openat_creat_existing:
    sd a0, 8(sp)

    li t3, 0x10
    mul t3, a0, t3
    li t4, FD_BASE
    add t3, t3, t4
    sd zero, 8(t3)

sys_openat_creat_open:
    call alloc_open_file_slot
    li t1, -1
    beq a0, t1, sys_openat_creat_fail

    li t1, OPEN_FILE_SIZE
    mul t3, a0, t1
    li t1, OPEN_FILES_BASE
    add t3, t3, t1

    ld t2, 8(sp)
    sd t2, 0x00(t3)
    sd zero, 0x08(t3)
    li t1, 0x03
    sb t1, 0x10(t3)

    addi a0, a0, 3

    addi sp, sp, 32

    LOAD_CUR_PROC t0
    sd a0, 0x80(t0)
    j syscall_resume_current

sys_openat_creat_fail:
    addi sp, sp, 32
    j syscall_return_minus_one

syscall_openat_fail:
    addi sp, sp, 8
    j syscall_return_minus_one

/*
    Системный вызов close (57)

    Аргументы:
    a0 - файловый дескриптор

    Вывод:
    a0 - 0 при успехе, -1 при ошибке
*/
sys_close:
#if HART_COUNT > 1
    call oft_lock
#endif
    LOAD_CUR_PROC t0
    ld t1, 0x80(t0)

    li t2, 3
    blt t1, t2, syscall_return_zero

    addi t1, t1, -3
    li t2, OPEN_FILE_MAX_FD
    bge t1, t2, syscall_return_minus_one

    li t2, OPEN_FILE_SIZE
    mul t3, t1, t2
    li t2, OPEN_FILES_BASE
    add t3, t3, t2

    lb t4, 0x10(t3)
    andi t4, t4, 0x01
    beqz t4, syscall_return_minus_one

    sd zero, 0(t3)
    sd zero, 8(t3)
    sd zero, 16(t3)
    sd zero, 24(t3)
    j syscall_return_zero

/*
    Системный вызов read (63)

    Аргументы:
    a0 - файловый дескриптор
    a1 - адрес буфера
    a2 - количество байт

    Вывод:
    a0 - количество прочитанных байт (0 = конец файла)
*/
sys_read:
#if HART_COUNT > 1
    call oft_lock
#endif
    LOAD_CUR_PROC t0
    ld t1, 0x80(t0)
    ld t2, 0x88(t0)
    ld t3, 0x90(t0)

    li t4, 3
    blt t1, t4, syscall_return_zero

    addi t1, t1, -3
    li t4, OPEN_FILE_SIZE
    mul t1, t1, t4
    li t4, OPEN_FILES_BASE
    add t1, t1, t4

    lb t4, 0x10(t1)
    andi t4, t4, 0x01
    beqz t4, syscall_return_minus_one

    ld t4, 0x00(t1)
    ld t5, 0x08(t1)

    li t6, 0x10
    mul t4, t4, t6
    li t6, FD_BASE
    add t4, t4, t6
    ld a3, 0x00(t4)
    ld a4, 0x08(t4)

    sub a5, a4, t5
    blez a5, sys_read_eof

    blt t3, a5, sys_read_count_ok
    mv t3, a5
sys_read_count_ok:
    add a3, a3, t5
    mv a4, t2
    mv a5, t3
    li a6, 0
sys_read_copy_loop:
    beq a6, a5, sys_read_copy_done
    lb a7, 0(a3)
    sb a7, 0(a4)
    addi a3, a3, 1
    addi a4, a4, 1
    addi a6, a6, 1
    j sys_read_copy_loop
sys_read_copy_done:

    add t5, t5, t3
    sd t5, 0x08(t1)

    sd t3, 0x80(t0)

    mv a0, t5
    ld a1, 0x08(t4)
    la a2, msg_bar_tokenize
    call progress_bar
    j syscall_resume_current

sys_read_eof:
    sd zero, 0x80(t0)
    j syscall_resume_current

/*
    Системный вызов lseek (62)

    Аргументы:
    a0 - файловый дескриптор
    a1 - смещение
    a2 - откуда (0 = SET, 1 = CUR, 2 = END)

    Вывод:
    a0 - новое смещение
*/
sys_lseek:
#if HART_COUNT > 1
    call oft_lock
#endif
    LOAD_CUR_PROC t0
    ld t1, 0x80(t0)
    ld t2, 0x88(t0)
    ld t3, 0x90(t0)

    li t4, 3
    blt t1, t4, syscall_return_minus_one

    addi t1, t1, -3
    li t4, OPEN_FILE_SIZE
    mul t1, t1, t4
    li t4, OPEN_FILES_BASE
    add t1, t1, t4

    lb t4, 0x10(t1)
    andi t4, t4, 0x01
    beqz t4, syscall_return_minus_one

    beqz t3, sys_lseek_set

    li t4, 1
    beq t3, t4, sys_lseek_cur

    li t4, 2
    beq t3, t4, sys_lseek_end

    j syscall_return_minus_one

sys_lseek_set:
    sd t2, 0x08(t1)
    sd t2, 0x80(t0)
    j syscall_resume_current

sys_lseek_cur:
    ld t4, 0x08(t1)
    add t2, t2, t4
    sd t2, 0x08(t1)
    sd t2, 0x80(t0)
    j syscall_resume_current

sys_lseek_end:
    ld t4, 0x00(t1)
    li t5, 0x10
    mul t4, t4, t5
    li t5, FD_BASE
    add t4, t4, t5
    ld t5, 0x08(t4)
    add t2, t2, t5
    sd t2, 0x08(t1)
    sd t2, 0x80(t0)
    j syscall_resume_current

/*
    Системный вызов brk (214)

    Аргументы:
    a0 - новое значение brk, или 0 для запроса текущего

    Вывод:
    a0 - текущее (или новое) значение конца кучи
*/
sys_brk:
    LOAD_CUR_PROC t0
    ld t1, 0x80(t0)

#if HART_COUNT > 1
    ld t3, 0xd0(t0)
    beqz t1, sys_brk_return_current
    ld t4, 0xd8(t0)
    bltu t1, t4, sys_brk_return_current
    ld t4, 0xe0(t0)
    bgeu t1, t4, sys_brk_return_current
    mv t2, t0
#else
    la t2, heap_ptr
    ld t3, 0(t2)

    beqz t1, sys_brk_return_current

    li t4, HEAP_BASE
    bltu t1, t4, sys_brk_return_current
    li t4, HEAP_END
    bgeu t1, t4, sys_brk_return_current
#endif

    # v8: emulate fresh zeroed heap for each user program.
    # M0 allocates structs with malloc and leaves fields like TYPE/EXPRESSION
    # implicitly zero.  On QEMU/fresh RAM this worked, but on Banana the heap
    # is reused between do-runs after heap_ptr reset, so stale bytes make M0
    # loop forever.  Clear newly allocated bytes on brk growth.
#ifndef BOARD_BRK_ZERO
#define BOARD_BRK_ZERO 1
#endif
#if BOARD_BRK_ZERO
    mv a5, t3              # old brk
    mv a6, t1              # requested new brk
    li a7, ~7
    and a4, a5, a7
    beq a4, a5, sys_brk_zero_sd
sys_brk_zero_head:
    bgeu a5, a6, sys_brk_zero_new_done
    andi a4, a5, 7
    beqz a4, sys_brk_zero_sd
    sb zero, 0(a5)
    addi a5, a5, 1
    j sys_brk_zero_head
sys_brk_zero_sd:
    addi a4, a5, 8
    bgtu a4, a6, sys_brk_zero_tail
    sd zero, 0(a5)
    addi a5, a5, 8
    j sys_brk_zero_sd
sys_brk_zero_tail:
    bgeu a5, a6, sys_brk_zero_new_done
    sb zero, 0(a5)
    addi a5, a5, 1
    j sys_brk_zero_tail
sys_brk_zero_new_done:
#endif

#if HART_COUNT > 1
    sd t1, 0xd0(t2)
#else
    sd t1, 0(t2)
#endif
    sd t1, 0x80(t0)

    la t0, progress_last_label
    ld t2, 0(t0)
    la t3, msg_bar_compile
    beq t2, t3, sys_brk_progress
    la t3, msg_bar_tokenize
    bne t2, t3, sys_brk_skip_progress
    la t0, progress_last_pct
    ld t2, 0(t0)
    li t3, 100
    blt t2, t3, sys_brk_skip_progress
sys_brk_progress:
    li t2, HEAP_BASE
    sub a0, t1, t2
    srli a0, a0, 10
    call progress_heap_kb
sys_brk_skip_progress:
    j syscall_resume_current

sys_brk_return_current:
    sd t3, 0x80(t0)
    j syscall_resume_current

/*
    clock_gettime (113): write {0,0} timespec, success.
    gettimeofday (169): write {0,0} timeval, success.
    getcwd (17): write "/", return length including NUL.
    uname (160): fill Linux/riscv64 utsname.
*/
sys_clock_gettime:
    LOAD_CUR_PROC t0
    ld t1, 0x88(t0)
    beqz t1, syscall_return_minus_one
    sd zero, 0(t1)
    sd zero, 8(t1)
    j syscall_return_zero

sys_gettimeofday:
    LOAD_CUR_PROC t0
    ld t1, 0x80(t0)
    beqz t1, syscall_return_minus_one
    sd zero, 0(t1)
    sd zero, 8(t1)
    j syscall_return_zero

sys_getcwd:
    LOAD_CUR_PROC t0
    ld t1, 0x80(t0)
    ld t2, 0x88(t0)
    li t3, 2
    bltu t2, t3, syscall_return_minus_one
    beqz t1, syscall_return_minus_one
    li t3, '/'
    sb t3, 0(t1)
    sb zero, 1(t1)
    li t3, 2
    sd t3, 0x80(t0)
    j syscall_resume_current

sys_uname:
    LOAD_CUR_PROC t0
    ld t1, 0x80(t0)
    beqz t1, syscall_return_minus_one
    addi sp, sp, -16
    sd ra, 0(sp)
    sd t1, 8(sp)
    li t2, 390
    mv t3, t1
sys_uname_zero:
    beqz t2, sys_uname_fill
    sb zero, 0(t3)
    addi t3, t3, 1
    addi t2, t2, -1
    j sys_uname_zero
sys_uname_fill:
    ld t1, 8(sp)
    la t2, uts_sysname
    mv t3, t1
    call strcpy_raw
    ld t1, 8(sp)
    la t2, uts_nodename
    addi t3, t1, 65
    call strcpy_raw
    ld t1, 8(sp)
    la t2, uts_release
    addi t3, t1, 130
    call strcpy_raw
    ld t1, 8(sp)
    la t2, uts_version
    addi t3, t1, 195
    call strcpy_raw
    ld t1, 8(sp)
    la t2, uts_machine
    addi t3, t1, 260
    call strcpy_raw
    ld ra, 0(sp)
    addi sp, sp, 16
    j syscall_return_zero

sys_faccessat:
#if HART_COUNT > 1
    call oft_lock
#endif
    LOAD_CUR_PROC t0
    ld a0, 0x88(t0)
    call find_file_by_name
    li t1, -1
    beq a0, t1, syscall_return_minus_one
    j syscall_return_zero

/* t2 = src, t3 = dst. clobbers t4. */
strcpy_raw:
    lb t4, 0(t2)
    sb t4, 0(t3)
    beqz t4, strcpy_raw_done
    addi t2, t2, 1
    addi t3, t3, 1
    j strcpy_raw
strcpy_raw_done:
    ret

/*
    Поиск файла по имени в таблице имён

    Аргументы:
    a0 - адрес строки имени

    Вывод:
    a0 - индекс файла (0..63) или -1 если не найдено
*/
find_file_by_name:
    addi sp, sp, -32
    sd ra, 0(sp)
    sd s0, 8(sp)
    sd s1, 16(sp)
    sd s2, 24(sp)

    mv s0, a0
    li s1, FILE_NAME_BASE
    li s2, 0
find_file_loop:
    li t0, FILE_NAME_END
    bgeu s1, t0, find_file_not_found

    mv a0, s1
    mv a1, s0
    call path_strcmp
    beqz a0, find_file_found

    addi s2, s2, 1
    li t0, FILE_NAME_SLOT
    add s1, s1, t0
    j find_file_loop

find_file_found:
    mv a0, s2
    j find_file_return

find_file_not_found:
    li a0, -1

find_file_return:
    ld ra, 0(sp)
    ld s0, 8(sp)
    ld s1, 16(sp)
    ld s2, 24(sp)
    addi sp, sp, 32
    ret

alloc_open_file_slot:
    li t0, OPEN_FILES_BASE
    li t1, 0
alloc_open_file_loop:
    li t2, OPEN_FILE_MAX_FD
    bge t1, t2, alloc_open_file_full

    lb t3, 0x10(t0)
    andi t3, t3, 0x01
    beqz t3, alloc_open_file_done

    addi t1, t1, 1
    li t2, OPEN_FILE_SIZE
    add t0, t0, t2
    j alloc_open_file_loop

alloc_open_file_done:
    mv a0, t1
    ret

alloc_open_file_full:
    li a0, -1
    ret

find_next:
    LOAD_CUR_PROC t1
    mv t2, t1 
    bnez t1, search_loop

    li t1, PROC_FIRST_RUNNABLE
    mv t2, t1
    STORE_CUR_PROC t1
search_loop:    
    li t3, 0x1000
    add t2, t2, t3
    li t4, PROC_END
    bgeu t2, t4, search_loop_overflow

    ld t3, 0x8(t2)
    li t4, 0xffff
    beq t3, t4, found_task  

    j search_loop
search_loop_overflow:
    li t2, PROC_FIRST_RUNNABLE

    ld t3, 0x8(t2)
    li t4, 0xffff
    beq t3, t4, found_task

    j search_loop
search_loop_idle:    
    li t1, PROC_BASE
    ld t3, 0x8(t1)
    li t4, 0xffff
    beq t3, t4, found_task
    
    la a0, msg_kernel_panic_4
    j kernel_panic

found_task:
    mv t1, t2
    STORE_CUR_PROC t1
    mv t0, t1

    # Banana Pi K1-X: QEMU CLINT timer rearm отключён.

.global resume_proc_context
resume_proc_context:
    ld t1, 0xc0(t0)
#if BOARD_TRAP_MODE == BOARD_TRAP_M
    csrw mepc, t1
#else
    csrw sepc, t1
#endif
    
    ld sp, 0x10(t0)
    ld ra, 0x18(t0)
    ld s0, 0x20(t0); ld s1, 0x28(t0); ld s2, 0x30(t0)
    ld s3, 0x38(t0); ld s4, 0x40(t0); ld s5, 0x48(t0)
    ld s6, 0x50(t0); ld s7, 0x58(t0); ld s8, 0x60(t0)
    ld s9, 0x68(t0); ld s10, 0x70(t0); ld s11, 0x78(t0)

    ld a0, 0x80(t0); ld a1, 0x88(t0); ld a2, 0x90(t0)
    ld a3, 0x98(t0); ld a4, 0xa0(t0); ld a5, 0xa8(t0)
    ld a6, 0xb0(t0); ld a7, 0xb8(t0)

    ld t1, 0x100(t0)
#if BOARD_TRAP_MODE == BOARD_TRAP_M
    csrw mscratch, t1
#else
    csrw sscratch, t1
#endif
    ld t1, 0x108(t0); ld t2, 0x110(t0); ld t3, 0x118(t0)
    ld t4, 0x120(t0); ld t5, 0x128(t0); ld t6, 0x130(t0)

#if BOARD_TRAP_MODE == BOARD_TRAP_M
    csrr t0, mscratch
    li t2, 0x1800
    csrs mstatus, t2
    mret
#else
    csrr t0, sscratch
    # SPP=1 -> S-mode, SPP=0 -> U-mode.
    LOAD_CUR_PROC t1
    ld t2, 0xc8(t1)
    bnez t2, 1f
    li t2, 0x100
    csrs sstatus, t2
    sret
1:
    li t2, 0x100
    csrc sstatus, t2
    sret
#endif

create_process:
    li t0, PROC_BASE
    li t1, 0
    create_process_loop:
        li t2, PROC_END
        bge t0, t2, create_process_no_slot

        ld t2, 0x8(t0)
        beqz t2, create_process_slot_found

        addi t1, t1, 1
        li t3, 0x1000
        add t0, t0, t3
        j create_process_loop
        create_process_no_slot:
            la a0, msg_kernel_panic_3
            call kernel_panic
        create_process_slot_found:
            sd t1, 0(t0)

            li t2, PROC_STATE_RUNNING
            sd t2, 0x8(t0)

            li t3, 0xff8
            add t2, t0, t3
            sd t2, 0x10(t0)

            sd a0, 0xc0(t0)      # saved PC / sepc
            sd zero, 0xc8(t0)    # mode: 0 = S-mode kernel task, 1 = U-mode user binary
            sd zero, 0x18(t0)
            sd a1, 0xf0(t0)

            # v6: clear callee-saved registers for a freshly launched
            # user tool. Reused process slots otherwise keep stale s-regs
            # from the previous do-command.
            sd zero, 0x20(t0); sd zero, 0x28(t0); sd zero, 0x30(t0)
            sd zero, 0x38(t0); sd zero, 0x40(t0); sd zero, 0x48(t0)
            sd zero, 0x50(t0); sd zero, 0x58(t0); sd zero, 0x60(t0)
            sd zero, 0x68(t0); sd zero, 0x70(t0); sd zero, 0x78(t0)

            sd zero, 0x80(t0)
            sd zero, 0x88(t0)
            sd zero, 0x90(t0)
            sd zero, 0x98(t0)
            sd zero, 0xa0(t0)
            sd zero, 0xa8(t0)
            sd zero, 0xb0(t0)
            sd zero, 0xb8(t0)

            sd zero, 0x100(t0); sd zero, 0x108(t0); sd zero, 0x110(t0)
            sd zero, 0x118(t0); sd zero, 0x120(t0); sd zero, 0x128(t0)
            sd zero, 0x130(t0)

            mv a0, t0
            ret

create_user_process:
    addi sp, sp, -16
    sd ra, 0(sp)
    sd a2, 8(sp)

    call create_process

    ld t0, 8(sp)
    sd t0, 0x10(a0)
    li t1, 1
    sd t1, 0xc8(a0)       # user binary returns with SPP=0 => U-mode

    ld ra, 0(sp)
    addi sp, sp, 16
    ret
            
switch_context:
    sd sp, 0x10(a0)
    sd ra, 0x18(a0)
    sd s0, 0x20(a0)
    sd s1, 0x28(a0)
    sd s2, 0x30(a0)
    sd s3, 0x38(a0)
    sd s4, 0x40(a0)
    sd s5, 0x48(a0)
    sd s6, 0x50(a0)
    sd s7, 0x58(a0)
    sd s8, 0x60(a0)
    sd s9, 0x68(a0)
    sd s10, 0x70(a0)
    sd s11, 0x78(a0)

    STORE_CUR_PROC a1

    ld sp, 0x10(a1)
    ld ra, 0x18(a1)
    ld s0, 0x20(a1)
    ld s1, 0x28(a1)
    ld s2, 0x30(a1)
    ld s3, 0x38(a1)
    ld s4, 0x40(a1)
    ld s5, 0x48(a1)
    ld s6, 0x50(a1)
    ld s7, 0x58(a1)
    ld s8, 0x60(a1)
    ld s9, 0x68(a1)
    ld s10, 0x70(a1)
    ld s11, 0x78(a1)
    ret

/*
    Обработка непредвиденного исключения (не ecall и не таймер)

    Печатает scause, sepc, stval и уходит в kernel_panic.
    Сюда мы попадаем из yield_no_save, когда trap случился по причине,
    которую мы не ждали: illegal instruction, access fault и прочая радость.

    Регистры процесса к этому моменту уже сохранены в начале yield,
    так что портить t* можно без зазрения совести.
*/
trap_panic:
#if BOARD_TRAP_MODE == BOARD_TRAP_M
    la a0, msg_trap_mcause
#else
    la a0, msg_trap_scause
#endif
    call puts
#if BOARD_TRAP_MODE == BOARD_TRAP_M
    csrr a0, mcause
#else
    csrr a0, scause
#endif
    call puth
    la a0, sym_newline
    call puts

#if BOARD_TRAP_MODE == BOARD_TRAP_M
    la a0, msg_trap_mepc
#else
    la a0, msg_trap_sepc
#endif
    call puts
#if BOARD_TRAP_MODE == BOARD_TRAP_M
    csrr a0, mepc
#else
    csrr a0, sepc
#endif
    call puth
    la a0, sym_newline
    call puts

#if BOARD_TRAP_MODE == BOARD_TRAP_M
    la a0, msg_trap_mtval
#else
    la a0, msg_trap_stval
#endif
    call puts
#if BOARD_TRAP_MODE == BOARD_TRAP_M
    csrr a0, mtval
#else
    csrr a0, stval
#endif
    call puth
    la a0, sym_newline
    call puts

    LOAD_CUR_PROC t0
    beqz t0, trap_panic_no_pcb

    la a0, msg_trap_ra
    call puts
    LOAD_CUR_PROC t0
    ld a0, 0x18(t0)
    call puth
    la a0, sym_newline
    call puts

    la a0, msg_trap_sp
    call puts
    LOAD_CUR_PROC t0
    ld a0, 0x10(t0)
    call puth
    la a0, sym_newline
    call puts

    la a0, msg_trap_a0
    call puts
    LOAD_CUR_PROC t0
    ld a0, 0x80(t0)
    call puth
    la a0, sym_newline
    call puts

    la a0, msg_trap_a1
    call puts
    LOAD_CUR_PROC t0
    ld a0, 0x88(t0)
    call puth
    la a0, sym_newline
    call puts

trap_panic_no_pcb:
    la a0, msg_trap_panic
    j kernel_panic

kernel_panic:
    mv t0, a0 
    la a0, msg_kernel_panic
    call puts
    mv a0, t0
    call puts 

    la a0, rec_kernel_panic_0
    call puts
    la a0, rec_kernel_panic_1
    call puts
    kernel_panic_loop: 
        call getc 

        li t0, 'r'
        beq a0, t0, reboot
        li t0, 's'
        beq a0, t0, shutdown

        j kernel_panic_loop

/*
    Длина строки

    Аргументы:
    a0 - адрес на строку

    Вывод:
    a0 - кол-во символов до нулевого символа
*/
strlen:
    addi sp, sp, -16
    sd t0, 0(sp)
    sd t1, 8(sp)

    li t0, 0
    strlen_loop:
        lb t1, 0(a0)
        beqz t1, strlen_done

        addi t0, t0, 1
        addi a0, a0, 1

        j strlen_loop
    strlen_done:
        mv a0, t0

        ld t0, 0(sp)
        ld t1, 8(sp)
        addi sp, sp, 16
        ret

/*
    Сравнение строк (нормальная версия)

    Аргументы:
    a0 - адрес на строку
    a1 - адрес на строку

    Вывод:
    a0 - 0 если строки одинаковы, иначе 1
*/
strcmp:
    addi sp, sp, -24
    sd t0, 0(sp)
    sd t1, 8(sp)
    sd t2, 16(sp)
    strcmp_loop:
        lb t0, 0(a0)
        lb t1, 0(a1)
    
        bne t0, t1, strcmp_not_equal 

        add t2, t0, t1
        beqz t2, strcmp_equal

        addi a0, a0, 1
        addi a1, a1, 1
        j strcmp_loop
        strcmp_equal:
            li a0, 0
            ld t0, 0(sp)
            ld t1, 8(sp)
            ld t2, 16(sp)
            addi sp, sp, 24
            ret
        strcmp_not_equal:
            li a0, 1
            ld t0, 0(sp)
            ld t1, 8(sp)
            ld t2, 16(sp)
            addi sp, sp, 24
            ret

/* Path compare: collapse // and skip leading ./ so mes include-from-path
   ("mes/module/" + "/" + file) matches packed "mes/module/file". */
path_strcmp:
    addi sp, sp, -24
    sd t0, 0(sp)
    sd t1, 8(sp)
    sd t2, 16(sp)
path_skip_dot0:
    lb t0, 0(a0)
    li t2, '.'
    bne t0, t2, path_skip_sl0
    lb t0, 1(a0)
    li t2, '/'
    bne t0, t2, path_skip_sl0
    addi a0, a0, 2
    j path_skip_dot0
path_skip_sl0:
    lb t0, 0(a0)
    li t2, '/'
    bne t0, t2, path_skip_dot1
    addi a0, a0, 1
    j path_skip_sl0
path_skip_dot1:
    lb t1, 0(a1)
    li t2, '.'
    bne t1, t2, path_skip_sl1
    lb t1, 1(a1)
    li t2, '/'
    bne t1, t2, path_skip_sl1
    addi a1, a1, 2
    j path_skip_dot1
path_skip_sl1:
    lb t1, 0(a1)
    li t2, '/'
    bne t1, t2, path_cmp_loop
    addi a1, a1, 1
    j path_skip_sl1
path_cmp_loop:
    lb t0, 0(a0)
    lb t1, 0(a1)
    li t2, '/'
    bne t0, t2, path_cmp_chars
    bne t1, t2, path_cmp_chars
path_eat_sl0:
    lb t0, 0(a0)
    bne t0, t2, path_eat_sl1
    addi a0, a0, 1
    j path_eat_sl0
path_eat_sl1:
    lb t1, 0(a1)
    bne t1, t2, path_cmp_loop
    addi a1, a1, 1
    j path_eat_sl1
path_cmp_chars:
    lb t0, 0(a0)
    lb t1, 0(a1)
    bne t0, t1, path_cmp_ne
    add t2, t0, t1
    beqz t2, path_cmp_eq
    addi a0, a0, 1
    addi a1, a1, 1
    j path_cmp_loop
path_cmp_eq:
    li a0, 0
    ld t0, 0(sp)
    ld t1, 8(sp)
    ld t2, 16(sp)
    addi sp, sp, 24
    ret
path_cmp_ne:
    li a0, 1
    ld t0, 0(sp)
    ld t1, 8(sp)
    ld t2, 16(sp)
    addi sp, sp, 24
    ret

/*  
    Сравнение строк (версия для команд)
    
    Аргументы:
    a0 - адрес на строку
    a1 - адрес на строку

    Вывод:
    a0 - 0 если строки одинаковы, иначе 1

    (!!!) строки считаются одинаковыми, если функция 
    дошла до нулевого символа строки в адресе a1, т. е.:
    a0: some text blablabla\0
    a1: some text\0
    считаются одинаковыми строками
*/
cmdcmp:
    addi sp, sp, -16
    sd t0, 0(sp)
    sd t1, 8(sp)
    cmdcmp_loop:
        lb t0, 0(a0)
        lb t1, 0(a1)
    
        beqz t1, cmdcmp_equal
        bne t0, t1, cmdcmp_not_equal 
    
        addi a0, a0, 1
        addi a1, a1, 1
        j cmdcmp_loop
        cmdcmp_equal:
            li a0, 0
            ld t0, 0(sp)
            ld t1, 8(sp)
            addi sp, sp, 16
            ret
        cmdcmp_not_equal:
            li a0, 1
            ld t0, 0(sp)
            ld t1, 8(sp)
            addi sp, sp, 16
            ret

/*
    Заполнение блока памяти заданным значением

    Аргументы:
    a0 - адрес на ячейку памяти
    a1 - значение
    a2 - количество ячеек

    Вывод: 
    - 
*/
memset:
    memset_loop:
        beqz a2, memset_done

        sb a1, 0(a0)
        addi a0, a0, 1
        addi a2, a2, -1

        j memset_loop
    memset_done:
        ret

/*
    Копирование блока памяти из (1) адреса во (2)

    Аргументы:
    a0 - адрес (1)
    a1 - адрес (2)
    a2 - кол-во ячеек для копирования

    Вывод:
    -
*/
memcpy:
    addi sp, sp, -8
    sd t0, 0(sp)
memcpy_loop:
    beqz a2, memcpy_done

    lb t0, 0(a0)
    sb t0, 0(a1)

    addi a0, a0, 1
    addi a1, a1, 1
    addi a2, a2, -1

    j memcpy_loop
memcpy_done:
    ld t0, 0(sp)
    addi sp, sp, 8
    ret 

free_slot_file_name:
    addi sp, sp, -8
    sd t0, 0(sp)

    li a0, FILE_NAME_BASE
    li a1, 0
free_slot_file_name_loop:
    li t0, FILE_NAME_END
    bgeu a0, t0, free_slot_file_name_full

    lb t0, 0(a0)
    beqz t0, free_slot_file_name_found

    li t0, FILE_NAME_SLOT
    add a0, a0, t0
    addi a1, a1, 1
    j free_slot_file_name_loop
free_slot_file_name_full:
    li a0, 0
    li a1, -1
free_slot_file_name_found:
    ld t0, 0(sp)
    addi sp, sp, 8
    ret

# a0 = current, a1 = total, a2 = label string
progress_bar:
#if HART_COUNT > 1
    csrr t0, mhartid
    bnez t0, progress_bar_ret
#endif
    beqz a1, progress_bar_ret
    li t0, 4096
    bltu a1, t0, progress_bar_ret

    addi sp, sp, -64
    sd ra, 0(sp)
    sd s0, 8(sp)
    sd s1, 16(sp)
    sd s2, 24(sp)
    sd s3, 32(sp)
    sd s4, 40(sp)
    sd s5, 48(sp)

    mv s0, a0
    mv s1, a1
    mv s2, a2

    li t0, 100
    mul t1, s0, t0
    divu s3, t1, s1
    li t0, 100
    bleu s3, t0, progress_bar_pct_ok
    li s3, 100
progress_bar_pct_ok:
    la t0, progress_last_label
    ld t1, 0(t0)
    bne t1, s2, progress_bar_draw
    la t0, progress_last_pct
    ld t1, 0(t0)
    beq t1, s3, progress_bar_restore

progress_bar_draw:
    la t0, progress_last_label
    sd s2, 0(t0)
    la t0, progress_last_pct
    sd s3, 0(t0)

    li a0, 0x0D
    call putc
    la a0, msg_bar_prefix
    call puts
    mv a0, s2
    call puts

    li t0, 20
    mul t1, s3, t0
    li t0, 100
    divu s5, t1, t0
    li s4, 0
    li a0, '['
    call putc
progress_bar_loop:
    li t0, 20
    bge s4, t0, progress_bar_loop_done
    blt s4, s5, progress_bar_hash
    li a0, '.'
    j progress_bar_put
progress_bar_hash:
    li a0, '#'
progress_bar_put:
    call putc
    addi s4, s4, 1
    j progress_bar_loop
progress_bar_loop_done:
    li a0, ']'
    call putc
    li a0, ' '
    call putc
    mv a0, s3
    call print_dec
    li a0, '%'
    call putc
    li a0, ' '
    call putc
    srli a0, s0, 10
    call print_dec
    li a0, '/'
    call putc
    srli a0, s1, 10
    call print_dec
    la a0, msg_bar_kb
    call puts
    call progress_pad

    li t0, 100
    bne s3, t0, progress_bar_restore
    la a0, sym_newline
    call puts

progress_bar_restore:
    ld ra, 0(sp)
    ld s0, 8(sp)
    ld s1, 16(sp)
    ld s2, 24(sp)
    ld s3, 32(sp)
    ld s4, 40(sp)
    ld s5, 48(sp)
    addi sp, sp, 64
progress_bar_ret:
    ret

# a0 = kilobytes written so far
progress_write_kb:
#if HART_COUNT > 1
    csrr t0, mhartid
    bnez t0, progress_write_kb_skip
#endif
    addi sp, sp, -16
    sd ra, 0(sp)
    sd s0, 8(sp)
    mv s0, a0
    la t0, progress_last_write_kb
    ld t1, 0(t0)
    sub t2, s0, t1
    li t3, 32
    bltu t2, t3, progress_write_kb_done
    sd s0, 0(t0)

    li a0, 0x0D
    call putc
    la a0, msg_bar_prefix
    call puts
    la a0, msg_bar_write
    call puts
    mv a0, s0
    call print_dec
    la a0, msg_bar_kb
    call puts
    call progress_pad

progress_write_kb_done:
    ld ra, 0(sp)
    ld s0, 8(sp)
    addi sp, sp, 16
    ret
#if HART_COUNT > 1
progress_write_kb_skip:
    ret
#endif

# a0 = heap kilobytes
progress_heap_kb:
#if HART_COUNT > 1
    csrr t0, mhartid
    bnez t0, progress_heap_kb_skip
#endif
    addi sp, sp, -16
    sd ra, 0(sp)
    sd s0, 8(sp)
    mv s0, a0
    la t0, progress_last_heap_kb
    ld t1, 0(t0)
    sub t2, s0, t1
    li t3, 256
    bltu t2, t3, progress_heap_kb_done
    sd s0, 0(t0)
    la t0, progress_last_label
    la t1, msg_bar_compile
    sd t1, 0(t0)

    li a0, 0x0D
    call putc
    la a0, msg_bar_prefix
    call puts
    la a0, msg_bar_compile
    call puts
    mv a0, s0
    call print_dec
    la a0, msg_bar_kb
    call puts
    call progress_pad

progress_heap_kb_done:
    ld ra, 0(sp)
    ld s0, 8(sp)
    addi sp, sp, 16
    ret
#if HART_COUNT > 1
progress_heap_kb_skip:
    ret
#endif

progress_pad:
    addi sp, sp, -16
    sd ra, 0(sp)
    sd s0, 8(sp)
    li s0, 24
progress_pad_loop:
    beqz s0, progress_pad_done
    li a0, ' '
    call putc
    addi s0, s0, -1
    j progress_pad_loop
progress_pad_done:
    ld ra, 0(sp)
    ld s0, 8(sp)
    addi sp, sp, 16
    ret

print_dec:
    addi sp, sp, -16
    sd ra, 0(sp)
    li t0, 10
    bltu a0, t0, print_dec_last
    remu t1, a0, t0
    divu a0, a0, t0
    sd t1, 8(sp)
    call print_dec
    ld a0, 8(sp)
print_dec_last:
    addi a0, a0, '0'
    call putc
    ld ra, 0(sp)
    addi sp, sp, 16
    ret

putc:
    addi sp, sp, -16
    sd t0, 0(sp)
    sd t1, 8(sp)

    li t0, UART0_BASE
putc_wait_tx:
    lb t1, UART_LSR(t0)
    andi t1, t1, LSR_TX_IDLE
    beqz t1, putc_wait_tx
    sb a0, 0(t0)

    ld t0, 0(sp)
    ld t1, 8(sp)
    addi sp, sp, 16
    ret

getc:
    addi sp, sp, -16
    sd t0, 0(sp)
    sd t1, 8(sp)

    li t0, UART0_BASE
1:
    lb t1, UART_LSR(t0)
    andi t1, t1, LSR_RX_READY
    beqz t1, 1b
    lb a0, 0(t0)

    ld t0, 0(sp)
    ld t1, 8(sp)
    addi sp, sp, 16
    ret

.global puts
puts:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd t0, 0(sp)
    sd t1, 8(sp)
    sd t2, 16(sp)
    sd t3, 24(sp)
    call uart_lock

    mv t1, a0
    li t2, UART0_BASE
puts_loop:
    lb a0, (t1)
    beqz a0, puts_done
puts_wait_tx:
    lb t3, UART_LSR(t2)
    andi t3, t3, LSR_TX_IDLE
    beqz t3, puts_wait_tx
    sb a0, 0(t2)
    addi t1, t1, 1
    j puts_loop
puts_done:
    call uart_unlock
    ld t0, 0(sp)
    ld t1, 8(sp)
    ld t2, 16(sp)
    ld t3, 24(sp)
    ld ra, 40(sp)
    addi sp, sp, 48
    ret

puth:
    addi sp, sp, -32
    sd s0, 0(sp)
    sd s1, 8(sp)
    sd t0, 16(sp)
    sd ra, 24(sp)
    
    mv s0, a0
    li s1, 16

    la a0, sym_hexnum
    call puts
puth_loop:
    srli a0, s0, 60
    andi a0, a0, 0xF

    li t0, 10
    blt a0, t0, puth_digit
    addi a0, a0, 87
    j puth_output
puth_digit:
    addi a0, a0, 48
puth_output:
    call putc
    
    slli s0, s0, 4
    
    addi s1, s1, -1
    bnez s1, puth_loop
    
puth_done:
    ld ra, 24(sp)
    ld t0, 16(sp)
    ld s1, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 32
    ret

# =============== Вспомогательное ===============
wdt_poll_counter:       .dword 1000000
cmd_prefix:             .string "\r\nSimon says~: "
unknown_cmd:            .string "Unknown command: "

sym_newline:            .string "\r\n"
sym_space:              .string " "
sym_hexnum:             .string "0x"
str_src:                .string "src"
greetings:              .string "[BOOTED]: Stage 2 (simon) - coded by Team 42.\r\n"
hex0_build:             .string "Building "
hex0_builded:           .string "Done."
msg_bar_prefix:         .string "[bar] "
msg_bar_tokenize:       .string "tokenize "
msg_bar_compile:        .string "compile  "
msg_bar_write:          .string "write    "
msg_bar_kb:             .string " KB"
# ===============================================

# ============= Сообщения исключений ============
msg_trap_scause:        .string "[TRAP] scause = "
msg_trap_sepc:          .string "[TRAP] sepc   = "
msg_trap_stval:         .string "[TRAP] stval  = "
msg_trap_mcause:        .string "[TRAP] mcause = "
msg_trap_mepc:          .string "[TRAP] mepc   = "
msg_trap_mtval:         .string "[TRAP] mtval  = "
msg_trap_ra:            .string "[TRAP] ra     = "
msg_trap_sp:            .string "[TRAP] sp     = "
msg_trap_a0:            .string "[TRAP] a0     = "
msg_trap_a1:            .string "[TRAP] a1     = "
msg_trap_panic:         .string "Unhandled exception\r\n"
# ===============================================

# =================== Команды ===================
debug_cmd:              .string "debug"
shutdown_cmd:           .string "shutdown"
reboot_cmd:             .string "reboot"
ls_cmd:                 .string "ls"
hex0_cmd:               .string "hex0"
do_cmd:                 .string "do"
spawn_cmd:              .string "spawn"
wait_cmd:               .string "wait"
view_cmd:               .string "view"
# ===============================================

# ================== Процессы ===================
ps_name_undefined:      .string "undefined"
ps_name_idle:           .string "<init>"
ps_name_sish:           .string "SiSH" 
ps_name_debug:          .string "debug cmd"
# ===============================================

# ================= Для ошибок ==================
msg_kernel_panic:       .string "[KERNEL PANIC]: "

msg_kernel_panic_0:     .string "Debug\r\n"
msg_kernel_panic_1:     .string "Buffer overflowed\r\n"
msg_kernel_panic_2:     .string "Out of memory\r\n"
msg_kernel_panic_3:     .string "No free process slots\r\n"
msg_kernel_panic_4:     .string "Where are all the processes?\r\n"
msg_spawn_full:         .string "No free hart\r\n"
msg_syscall_no_proc:    .string "Syscall without current process\r\n"
uts_sysname:            .string "Linux"
uts_nodename:           .string "simon"
uts_release:            .string "0"
uts_version:            .string "0"
uts_machine:            .string "riscv64"

rec_kernel_panic_0:     .string "Please, press r to reboot\r\n"
rec_kernel_panic_1:     .string "...or press s to shutdown.\r\n"
# ===============================================

.align 3
script_ptr:             .space  8
file_ptr:               .space  8
next_file_data:         .space  8
heap_ptr:               .space  8
progress_write_bytes:   .space  8
progress_last_write_kb: .space  8
progress_last_heap_kb:  .space  8
progress_last_pct:      .space  8
progress_last_label:    .space  8
user_running:           .space  8
user_proc:              .space  8
buffer:                 .space  BUFFER_SIZE
buffer_index:           .space  1
buffer_ready:           .space  1
sish_parsed:           .space  1
current_proc:           .space  8
next_sector:
