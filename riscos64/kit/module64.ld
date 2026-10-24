/* module64.ld - the linker script of the 64-bit modules made with the kit (aarch64-riscos-gcc -mmodule), "the published way" (flags bit 2): the module relocates itself.  Copied from mod64d.ld of module test 0.4.
   The layout follows the one that the 64-bit modules of riscos64-clib have (riscos64-clib/clib/linker/rmf.lnk): linked at address 4 (the module is loaded at a page boundary plus 4, so that
   ADRP / ADD addressing keeps working), code, read-only data, the pointers that need relocating, the read-write data on a page boundary, then the zero-initialised area, which is not in the linked image (modreloc64.py writes it into the file as zeros after the image: a kernel is not required to allocate an area after a module):
   the header has its size (a word after the flags word) for a kernel that does allocate it.  Everything the module can refer to is reached PC-relative; the only things that need fixing when
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
    . = ALIGN(8);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) .init_array))
    PROVIDE_HIDDEN (__init_array_end = .);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) .fini_array))
    PROVIDE_HIDDEN (__fini_array_end = .);
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
