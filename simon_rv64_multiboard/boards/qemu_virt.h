#ifndef BOARD_CONFIG_LOADED
#define BOARD_CONFIG_LOADED 1
#endif

#define BOARD_TRAP_M 1
#define BOARD_TRAP_S 2

/* Template for qemu-system-riscv64 -M virt -bios image_qemu.bin.
 * Kept mostly for local debugging; hardware configs above are the tested ones.
 */
#define BOARD_TRAP_MODE BOARD_TRAP_M
#define BOARD_WDT_ENABLE 0

#define UART0_BASE      0x10000000
#define UART_LSR        5
#define LSR_RX_READY    1
#define LSR_TX_IDLE     0x20

/* QEMU virt: 4 harts, CLINT at 0x02000000. Banana/Lichee leave HART_COUNT=1. */
#define HART_COUNT      4
#define HART_STACK_SIZE 0x1000
#define CLINT_BASE      0x02000000
/* In the user-code hole so boot decode and warmup do not touch it. */
#define HART_MAILBOX    0x81F00000

#define SOC_WDT_BASE    0x00000000
#define WDT_WMER        0x00b8
#define WDT_WMR         0x00bc
#define WDT_WSR         0x00c0
#define WDT_WCR         0x00c8
#define WDT_WFAR        0x00b0
#define WDT_WSAR        0x00b4

#define MCAUSE_ECALL_U      8
#define MCAUSE_ECALL_M      11
#define MCAUSE_TIMER_M      0x8000000000000005

#define SRAM_SIZE       0x17FFFF
#define STACK_OFFSET    SRAM_SIZE
/* Mes C + Scheme + nyacc + tcc sources are several MB. Stage2 must sit
 * after the whole image; 8MB leaves headroom past the current files.pl. */
#define PAYLOAD_OFFSET  0x800000

#define IMAGE_SCAN_BASE     0x80000000
#define IMAGE_SCAN_END      0x80800000

/* Image is loaded at 0x80000000 (boot + simon.hex0 + files.pl) and must not
 * overlap stage2 or warmup. Do not warmup STAGE2: boot writes simon there. */
#define STAGE2_BASE         0x80800000
#define KERNEL_STACK_TOP    0x80810000
#define OPEN_FILES_BASE     0x80810100
#define OPEN_FILES_END      0x80818100
#define OPEN_FILE_MAX_FD    1024
#define FD_BASE             0x80820000
#define PROC_BASE           0x80900000
#define PROC_FIRST_RUNNABLE 0x80901000
#define PROC_END            0x8091ffff
#define FILE_NAME_BASE      0x80920000
#define FILE_NAME_END       0x8099ffff
#define FILE_DATA_BASE      0x809A0000
#define USER_CODE_BASE      0x82000000
/* Four 8MB hex2 windows (slots 0..3). File bump skips the whole range. */
#define USER_CODE_END       0x84000000
#define FILE_HOLE_GUARD     0x800000
#define SLOT_CODE_SIZE      0x800000
/* 64MB after the hex2 windows so skipped files do not share SLOT0_HEAP.
 * Each slot heap shrinks 16MB so 4×heap still ends at 6GiB. */
#define SLOT_HEAP_SIZE      0x5E000000
#define SLOT0_CODE          0x82000000
#define SLOT1_CODE          0x82800000
#define SLOT2_CODE          0x83000000
#define SLOT3_CODE          0x83800000
#define SLOT0_HEAP          0x88000000
#define SLOT1_HEAP          0xE6000000
#define SLOT2_HEAP          0x144000000
#define SLOT3_HEAP          0x1A2000000
#define SLOT0_HEAP_END      0xE6000000
#define SLOT1_HEAP_END      0x144000000
#define SLOT2_HEAP_END      0x1A2000000
#define SLOT3_HEAP_END      0x200000000
#define SLOT0_ARGS          0xE5FFF000
#define SLOT1_ARGS          0x143FFF000
#define SLOT2_ARGS          0x1A1FFF000
#define SLOT3_ARGS          0x1FFFFF000
/* argc/argv + user SP for `do` (slot 0). Top of slot0 heap. */
#define ARGS_BLOCK_BASE     SLOT0_ARGS
#define HEAP_BASE           SLOT0_HEAP
/* QEMU -m 6G maps RAM [0x80000000, 0x200000000). Slot0 `do` gets ~1.5GiB. */
#define HEAP_END            SLOT0_HEAP_END

#define EARLY_WARMUP_START  0x80900000
#define EARLY_WARMUP_END    0x809A0000
#define WARMUP1_START       0x80900000
#define WARMUP1_END         0x809A0000
#define WARMUP2_ENABLE      1
#define WARMUP2_START       0x80810000
#define WARMUP2_END         0x80900000
