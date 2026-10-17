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

/* -mmodule (and -mmodule-fp, which allows the floating point and vector registers in the module): a 64-bit module.  The options are those of riscos64/kit/modflags.sh: no library, no GOT (the module
   relocates its own pointers), x18 reserved, no floating point or vector registers unless -mmodule-fp.  C++ gets no exceptions, no RTTI and no thread safe statics (the module has no runtime for them).  */
#define RISCOS_MODULE_P "mmodule|mmodule-fp"
#define RISCOS_MODULE_CC1_SPEC \
  "%{" RISCOS_MODULE_P ":-ffreestanding -fno-pic -fno-pie -fvisibility=hidden -mcmodel=small %{!mmodule-fp:-mgeneral-regs-only} " \
  "-mno-outline-atomics -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-tree-loop-distribute-patterns}"

#undef CC1_SPEC
#define CC1_SPEC AARCH64_ERRATA_COMPILE_SPEC " -ffixed-x18 %{!mno-strict-align:%{!mstrict-align:-mstrict-align}} " RISCOS_MODULE_CC1_SPEC
#undef CC1PLUS_SPEC
#define CC1PLUS_SPEC CC1_SPEC " %{" RISCOS_MODULE_P ":-fno-exceptions -fno-rtti -fno-threadsafe-statics}"

/* A module sees the headers of the kit (aarch64-riscos/modkit: the library headers of the module C library, and modkit64.h), which come before the headers of picolibc.  The directories are relative to
   the directory that the driver gives as -iprefix (lib/gcc/aarch64-riscos/VERSION/).  */
#undef CPP_SPEC
#define CPP_SPEC "%{" RISCOS_MODULE_P ":-DMODULE " \
  "-iwithprefixbefore ../../../../aarch64-riscos/modkit/riscos64 -iwithprefixbefore ../../../../aarch64-riscos/modkit/riscos64/include " \
  "-iwithprefixbefore ../../../../aarch64-riscos/modkit/include}"

/* A program: the AIF64 header, then the start-up code, then the usual crt files; the linker script makes the image, and objcopy writes it as it is (the header holds the sizes).
   A module: the kit's runtime (and the module's own header object, which is one of the input files), the kit's C library and no start files of the C library.  */
/* (the system calls are objects, not a library: picolibc has its own sbrk.o, and an archive that comes later would not replace it)  */
#undef STARTFILE_SPEC
#define STARTFILE_SPEC "%{" RISCOS_MODULE_P ":modrt64%O%s modswi64%O%s;: aifhdr%O%s crt0%O%s riscos_start%O%s riscos_syscalls%O%s swi%O%s crti%O%s crtbegin%O%s}"
#undef ENDFILE_SPEC
#define ENDFILE_SPEC "%{" RISCOS_MODULE_P ":;: crtend%O%s crtn%O%s}"
#undef LIB_SPEC
/* -munixlib: UnixLib 64 (riscos64/unixlib): the translation of Unix file names, directories, the current directory ...; -u makes the linker take the library in (the start-up code refers to it only weakly).
   libriscosstub.a (searched before libc.a, so that its signal () and raise () replace picolibc's fallbacks) has small POSIX functions and the ones that only UnixLib 64 really provides (opendir ...), which fail with ENOSYS if UnixLib 64 is not linked in.  */
#define LIB_SPEC "%{" RISCOS_MODULE_P ":-lmodkit64 -lmodcxx64 -lgcc;:%{munixlib:-u __ul64_init -lunixlib64} --start-group -lriscosstub -lc -lm -lgcc --end-group}"
#undef LINK_SPEC
#define LINK_SPEC "-maarch64riscos -static -X --no-warn-rwx-segments %{" RISCOS_MODULE_P ":-T module64.ld%s --emit-relocs;:-T aif64.ld%s %{!r:--aif64}}" AARCH64_ERRATA_LINK_SPEC

/* A program: the linker writes the AIF image itself (--aif64, which the AArch64 RISC OS 64 emulation of ld has: the loadable contents of the linked ELF file, what objcopy -O binary would make; the linker script
   pads the end of the data so that the file is exactly the read-only plus the read/write size of the header).  A module: modreloc64 turns the linked ELF file into the image, in place (it lists the pointers that
   the module relocates itself).  */
#undef POST_LINK_SPEC
#define POST_LINK_SPEC "%{!r:%{" RISCOS_MODULE_P ":modreloc64 -q %{o*:%*}%{!o:a.out}}}"

#endif /* GCC_AARCH64_RISCOS_H */
