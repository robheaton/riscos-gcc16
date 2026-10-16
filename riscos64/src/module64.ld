OUTPUT_FORMAT("elf64-littleaarch64")
ENTRY(_mod_start)
SECTIONS
{
  . = 0;
  .text : { *(.text) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.ARM.attributes) *(.bss*) }
}
