/* mod64sf.ld - the linker script of the small assembler modules Mod64S and Mod64F: one flat image, linked at address 4 (an explicit address: a section with an explicit address is not moved to the largest
   alignment of its input).  The module is loaded at a page boundary plus 4, so a link address and a run time address then have the same low 12 bits: alignment is kept (.balign 8 data is 8-byte aligned
   when the module runs) and ADRP would be right too.  There is no zero-initialised data.  (The older Mod64 of module tests 0.1-0.3 was linked at 0: its data was 4 bytes off an 8-byte boundary at run time.) */
OUTPUT_FORMAT("elf64-littleaarch64")
ENTRY(_mod_start)
SECTIONS
{
  .image 4 : {
    *(.text .text.*)
    *(.rodata .rodata.*)
    *(.data .data.*)
    . = ALIGN(4);
  }
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.ARM.attributes) *(.bss*) }
}
