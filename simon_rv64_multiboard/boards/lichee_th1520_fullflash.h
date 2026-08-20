#ifndef BOARD_CONFIG_LOADED
#define BOARD_CONFIG_LOADED 1
#endif

/* Common trap modes */
#define BOARD_TRAP_M 1
#define BOARD_TRAP_S 2

/* Lichee Pi 4A / TH1520 vendor U-Boot full-flash path.
 * U-Boot starts the standalone payload in M-mode at 0x40000000.
 */
#define BOARD_TRAP_MODE  BOARD_TRAP_M
#define BOARD_WDT_ENABLE 0

#define UART0_BASE       0xFFE7014000
#define UART_LSR         0x14
#define LSR_RX_READY     0x01
#define LSR_TX_IDLE      0x20

#define SOC_WDT_BASE     0x00000000
#define WDT_WMER         0x00b8
#define WDT_WMR          0x00bc
#define WDT_WSR          0x00c0
#define WDT_WCR          0x00c8
#define WDT_WFAR         0x00b0
#define WDT_WSAR         0x00b4

#define MCAUSE_ECALL_U      8
#define MCAUSE_ECALL_M      11
#define MCAUSE_TIMER_M      0x8000000000000005

#define SRAM_SIZE        0x17FFFF

/* build/files.pl is several MiB. Keep the stage1 stack and decoded stage2
 * above the complete raw image instead of overwriting embedded sources.
 */
#define STACK_OFFSET     0x7F0000
#define PAYLOAD_OFFSET   0x800000

#define IMAGE_SCAN_BASE     0x40000000
#define IMAGE_SCAN_END      0x40800000

/* 0x40000000..0x407fffff contains the loaded image. Do not reuse it. */
#define STAGE2_BASE         0x40800000
#define KERNEL_STACK_TOP    0x40810000
#define OPEN_FILES_BASE     0x40810100
#define OPEN_FILES_END      0x40812100
#define OPEN_FILE_MAX_FD    256
#define FD_BASE             0x40820000
#define PROC_BASE           0x40900000
#define PROC_FIRST_RUNNABLE 0x40901000
#define PROC_END            0x4091ffff
#define FILE_NAME_BASE      0x40920000
#define FILE_NAME_END       0x4099ffff
#define FILE_DATA_BASE      0x409A0000

/* The seed hex2_riscv64.hex1 linker starts its instruction pointer at
 * 0x82000000, so all bootstrap tools must execute at that same address.
 * Simon copies every launched tool here before FENCE.I and execution.
 */
#define USER_CODE_BASE      0x82000000
#define ARGS_BLOCK_BASE     0x43FFF000
#define HEAP_BASE           0x44000000
#define HEAP_END            0x7F000000

/* Warm only Simon work areas. In particular, never touch the loaded image or
 * STAGE2_BASE while stage1 is still decoding Simon.
 */
#define EARLY_WARMUP_START  0x40900000
#define EARLY_WARMUP_END    0x409A0000
#define WARMUP1_START       0x40900000
#define WARMUP1_END         0x409A0000
#define WARMUP2_ENABLE      1
#define WARMUP2_START       0x40810000
#define WARMUP2_END         0x40900000
