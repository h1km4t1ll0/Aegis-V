OUTPUT_ARCH(riscv)
ENTRY(_start)
SECTIONS {
  . = 0x80000000;
  .text : { KEEP(*(.sbi.entry)) *(.text .text.*) }
  .rodata : { *(.rodata .rodata.*) }
  .data : { *(.data .data.* .sdata .sdata.*) }
  . = ALIGN(16);
  __bss_start = .;
  .bss (NOLOAD) : { *(.bss .bss.* .sbss .sbss.*) *(COMMON) }
  . = ALIGN(16);
  __bss_stop = .;
  ASSERT(. < 0x80100000, "SBI exceeds reserved region")
  /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.riscv.attributes) }
}
