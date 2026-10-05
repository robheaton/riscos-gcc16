/* The big heap maximum (512 MB of reserved logical address space, see riscos-da.c) for the native programs that can use a lot of memory: the binutils programs (as, ld,
   objcopy, strip, ...).  Linked, in addition to riscos-da.o, by build-binutils-native.sh; cc1 and cc1plus get the same value from gcc/config/arm/riscos.c.
   <program>$HeapMax (an integer, in MB) overrides it for one program.  */
int __dynamic_da_max_size = 512 * 1024 * 1024;
/* The main stack of these programs: 8 MB (libunixlib 16.2.0-6 and later; an older libunixlib ignores it and the stack stays 1 MB).  The c++filt demangler and the linker script parser
   recurse; 1 MB is enough for ordinary input, 8 MB (what Linux gives a program) for the odd one.  Costs address space only.  */
int __stack_size = 8 * 1024 * 1024;
