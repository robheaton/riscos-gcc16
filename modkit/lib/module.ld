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
    /* the static constructors and destructors of C++ (lib/cxxrt.c runs them): the words are addresses, which modreloc relocates like any other */
    . = ALIGN(4);
    __init_array_start = .;
    KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
    KEEP(*(.init_array .ctors))
    __init_array_end = .;
    __fini_array_start = .;
    KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
    KEEP(*(.fini_array .dtors))
    __fini_array_end = .;
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
