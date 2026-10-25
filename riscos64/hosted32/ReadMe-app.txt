GCC 64 @VERSION@ - the AArch64 (RISC OS 64) cross compiler that runs on 32 bit RISC OS
=====================================================================================

GCC 16.2.0 and binutils 2.45.1 for the target aarch64-riscos, built as programs for 32 bit RISC OS (the same runtime as the Gcc16 package).  It compiles and links programs and modules for RISC OS 64 on a 32 bit machine.  It is a PRE-RELEASE.

What is in it
  bin/aarch64-riscos-gcc, -g++, -cpp, -ar, -nm, -objdump ...      the compilers and the GNU tools (C and C++)
  bin/cmunge64, bin/mkoslib                                       the module kit: CMHG files to module headers; the OS library veneers
  aarch64-riscos/bin/                                             as, ld, modreloc64 ... (the drivers find them here)
  aarch64-riscos/include, aarch64-riscos/lib                      the headers and libraries of the 64 bit SDK: picolibc, UnixLib 64, libstdc++, the start-up files and linker scripts, the module kit
  tests/                                                          hello.c and the Obey file T1

You need
  * a 32 bit machine with RISC OS 5 and the C runtime of the Gcc16 package (SharedLibs-C-armeabihf, SharedUnixLibrary 1.12), about 150 MB of disc, and memory: the compilers use a lot, see below
  * RISC OS 64 (or Pyromaniac's 64 bit mode) to run what you make

Use
  1. Double-click !GCC64 once after each boot (it sets the search path, the return code limit and the file name translation).
  2. Open a Task window and give it room BEFORE you start the compiler:   WimpSlot -min 64M -max 64M
     (the slot of a program that runs cannot be changed, and a compiler started in a small slot stops with "WimpSlot not big enough").
  3. Go to your work directory (Dir ...) and compile.  Source files live in the usual RISC OS directories (hello.c is c.hello, util.h is h.util, main.o is o.main):
         aarch64-riscos-gcc -O2 -munixlib -o hello hello.c        the result is a program of RISC OS 64 (file type &FF8: just run it there)
         aarch64-riscos-g++ -O2 -munixlib -o hellopp hellopp.cc   C++ with the standard library and exceptions
         aarch64-riscos-gcc -c -O2 foo.c                          an object file (o.foo)
  4. A module: write a CMHG file and its C code, then
         cmunge64 -d foo_h.h -o foo_hdr.o foo.cmhg
         aarch64-riscos-gcc -mmodule -O2 foo.c foo_hdr.o -o Foo   -> Foo, a 64 bit module (file type &FFA)
     The last step runs modreloc64 for you.  mkoslib makes veneers of OSLib functions: see the module kit documents (MODULES64.md in the repository).
  Copy what you made to the RISC OS 64 machine (or put it on a share both machines see).

Limits to know about
  * It is a cross compiler: the programs it makes cannot run on the 32 bit machine.
  * The compilers proper (cc1, cc1plus) are big programs and take their time: a small C file needs seconds, the first C++ file with iostream about a minute on a Raspberry Pi 4 class machine.  The heap of a compiler is a dynamic area of up to 512 MB.
  * A command line can be limited to 255 characters by the kernel: use @file response files for long lists of files.
  * Fortran, LTO and plug-ins are not built.  Veneers and runnable modules are not supported by cmunge64 (as in the Linux tool chain).
  * mkoslib finds a function's header by its name only (xos_cli is in os.h); on Linux it also searches the other headers.
  * The linker once failed with a BFD assertion (elf-strtab.c) on the very first C++ link and never again in many links afterwards; if you see it, try again and tell us.

Licences: GCC and binutils GPL-3.0 (with the runtime library exception for libgcc and libstdc++), picolibc BSD-style, UnixLib its own licence; see LICENSES.md in the repository: https://github.com/robheaton/riscos-gcc16
