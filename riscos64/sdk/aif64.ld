/* aif64.ld - linker script of a hosted AIF64 absolute (RISC OS 64): the image is linked at &8000, the AIF header first; the C library (picolibc) wants the constructor tables, and the heap starts at the end
   of the image (crt/riscos_start.c sets the end of the heap from the RAM limit that OS_GetEnv gives). */
OUTPUT_FORMAT("elf64-littleaarch64")
ENTRY(_aif_header)
SECTIONS
{
  . = 0x8000;
  .text : { KEEP(*(.aifhdr)) *(.text .text.*) }
  .rodata : { *(.rodata .rodata.*) }
  . = ALIGN(16);
  __ro_end = .;
  .data : {
    *(.data .data.*)
    . = ALIGN(8);
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array))
    PROVIDE_HIDDEN(__preinit_array_end = .);
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) .init_array))
    PROVIDE_HIDDEN(__init_array_end = .);
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*) .fini_array))
    PROVIDE_HIDDEN(__fini_array_end = .);
  }
  . = ALIGN(16);
  __data_end = .;
  __bss_start = .;
  .bss : { *(.bss .bss.* COMMON) }
  . = ALIGN(16);
  __bss_end = .;
  _end = .;
  end = .;
  __heap_start = .;
  __aif_ro_size = __ro_end - 0x8000;
  __aif_rw_size = __data_end - __ro_end;
  __aif_zi_size = __bss_end - __bss_start;
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.ARM.attributes) }
}
