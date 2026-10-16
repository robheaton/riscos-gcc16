/* mod64d.ld - the linker script of Mod64D, a 64-bit module with writable data, a zero-initialised area and pointers that the module relocates itself ("the published way": flags bit 2).
   The layout follows the one that the 64-bit modules of riscos64-clib have (riscos64-clib/clib/linker/rmf.lnk): linked at address 4 (the module is loaded at a page boundary plus 4, so that
   ADRP / ADD addressing keeps working), code, read-only data, the pointers that need relocating, the read-write data on a page boundary, then the zero-initialised area, which is NOT in the file:
   the kernel allocates it after the module (the size is a word after the flags word of the header).  Everything the module can refer to is reached PC-relative; the only things that need fixing when
   the module is loaded somewhere else are the 64-bit words that hold an address (R_AARCH64_ABS64): tools/modreloc64.py lists them in a table that the module header reserves. */
OUTPUT_FORMAT("elf64-littleaarch64")
ENTRY(_mod_start)
SECTIONS
{
  .image 4 : {                          /* an explicit address: the section is not aligned to the largest alignment of its input (32), it starts at 4 */
    KEEP(*(.text.header))
    *(.text .text.*)
    *(.rodata .rodata.*)
    *(.data.rel.ro .data.rel.ro.*)
    *(.got .got.plt)                    /* there must be none: modreloc64.py refuses a module that has a GOT entry */
    . = ALIGN(4096);                    /* the read-write region on a page boundary, as riscos64-clib's modules have it */
    *(.data.rel .data.rel.*)
    *(.data .data.*)
    . = ALIGN(16);                      /* the link address of the end of the image is a multiple of 16 (so the file is 12 bytes past a multiple of 16: the image starts at 4): the kernel puts the zero-initialised area right after the file, and the code expects it here */
    __image_end = .;
  }
  .zi (NOLOAD) : {
    __ZIStart = .;
    *(.bss .bss.* COMMON)
    . = ALIGN(16);
    __ZIEnd = .;
  }
  __ZISize = SIZEOF(.zi);               /* an absolute value: the header word that holds it is not an address */
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.ARM.attributes) *(.riscv.attributes) }
}
