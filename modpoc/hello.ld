/* hello.ld - a flat, position independent module image: header first, then code, read-only data, data and bss in ONE loadable output section (the image is in the RMA: writable) */
ENTRY(_start)
SECTIONS
{
  . = 0;
  .image : {
    KEEP(*(.text.header))
    *(.text .text.*)
    *(.rodata .rodata.*)
    . = ALIGN(4);
    *(.data .data.*)
    *(.bss .bss.* COMMON)
    . = ALIGN(4);
    __image_end = .;
  }
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*) *(.eh_frame*) *(.ARM.attributes) }
}
