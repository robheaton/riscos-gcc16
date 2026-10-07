/* hello.ld - a flat, position independent module image: header first, then code, read-only data, data and bss in ONE loadable output section (the image is in the RMA: writable) */
ENTRY(_start)
SECTIONS
{
  . = 0;
  .image : {
    /* the symbols that the Norcroft linker defines (assembler sources of the OS import them): the module is one image that starts at 0 and is moved by modreloc, so they are addresses inside it */
    "Image$$RO$$Base" = .;
    KEEP(*(.text.header))
    *(.text .text.*)
    *(.rodata .rodata.*)
    . = ALIGN(4);
    "Image$$RO$$Limit" = .;
    "Image$$RW$$Base" = .;
    *(.data .data.*)
    . = ALIGN(4);
    "Image$$RW$$Limit" = .;
    "Image$$ZI$$Base" = .;
    *(.bss .bss.* COMMON)
    . = ALIGN(4);
    "Image$$ZI$$Limit" = .;
    __image_end = .;
  }
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*) *(.eh_frame*) *(.ARM.attributes) }
}
