/* Definitions for AArch64 RISC OS (RISC OS 64): AArch64 ELF objects, linked into AIF64 programs and, with -mmodule, into 64-bit modules.  The files that a program is linked with (the AIF64 header, the start-up
   code, the linker script) are in the SDK of riscos64/ in the repository, installed in the library directory of the target (aarch64-riscos/lib).  */
#ifndef GCC_AARCH64_RISCOS_H
#define GCC_AARCH64_RISCOS_H

#undef TARGET_OS_CPP_BUILTINS
#define TARGET_OS_CPP_BUILTINS()		\
  do						\
    {						\
      builtin_define ("__riscos__");		\
      builtin_define ("__riscos64__");		\
      builtin_assert ("system=riscos");		\
    }						\
  while (0)

/* x18 is reserved by RISC OS 64, and a module or program must not rely on unaligned access: both are the defaults for this target.  */
#undef CC1_SPEC
#define CC1_SPEC AARCH64_ERRATA_COMPILE_SPEC " -ffixed-x18 %{!mno-strict-align:%{!mstrict-align:-mstrict-align}}"
#undef CC1PLUS_SPEC
#define CC1PLUS_SPEC CC1_SPEC

/* A program: the AIF64 header, then the start-up code, then the usual crt files; the linker script makes the image, and objcopy writes it as it is (the header holds the sizes).  */
#undef STARTFILE_SPEC
/* (the system calls are objects, not a library: picolibc has its own sbrk.o, and an archive that comes later would not replace it)  */
#define STARTFILE_SPEC " aifhdr%O%s crt0%O%s riscos_start%O%s riscos_syscalls%O%s swi%O%s crti%O%s crtbegin%O%s"
#undef ENDFILE_SPEC
#define ENDFILE_SPEC " crtend%O%s crtn%O%s"
#undef LIB_SPEC
#define LIB_SPEC "--start-group -lc -lm -lgcc --end-group"
#undef LINK_SPEC
#define LINK_SPEC "-maarch64riscos -static -X --no-warn-rwx-segments -T aif64.ld%s" AARCH64_ERRATA_LINK_SPEC

/* ld cannot write a raw image itself (the AArch64 backend refuses a change of output format), so objcopy turns the linked ELF file into the AIF64 image in place; the linker script pads the end of the data so
   that the file is exactly the read-only plus the read/write size of the header.  */
#undef POST_LINK_SPEC
#define POST_LINK_SPEC "%{!r:objcopy -O binary %{o*:%*}%{!o:a.out}}"

#endif /* GCC_AARCH64_RISCOS_H */
