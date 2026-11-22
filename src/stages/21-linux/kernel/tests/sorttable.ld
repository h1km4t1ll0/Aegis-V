OUTPUT_ARCH(riscv)
ENTRY(_start)
PHDRS { all PT_LOAD FLAGS(7); }
SECTIONS {
  . = 0x10000;
  .text : { *(.text*) } :all
  . = ALIGN(0x10000);
  __ex_table : { *(__ex_table) } :all
  .data : { *(.data*) } :all
  /DISCARD/ : { *(.comment*) *(.riscv.attributes) }
}
