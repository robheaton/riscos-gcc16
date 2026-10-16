/* linker script of an AIF64 absolute: the image is linked at &8000, the header first */
OUTPUT_FORMAT("elf64-littleaarch64")
ENTRY(_aif_header)
SECTIONS
{
  . = 0x8000;
  .text : { KEEP(*(.aifhdr)) *(.text .text.*) }
  .rodata : { *(.rodata .rodata.*) }
  . = ALIGN(16);
  __ro_end = .;
  .data : { *(.data .data.*) }
  . = ALIGN(16);
  __data_end = .;
  __bss_start = .;
  .bss : { *(.bss .bss.* COMMON) }
  . = ALIGN(16);
  __bss_end = .;
  __aif_ro_size = __ro_end - 0x8000;
  __aif_rw_size = __data_end - __ro_end;
  __aif_zi_size = __bss_end - __bss_start;
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.ARM.attributes) }
}
