riscos-gcc16 cross toolchain: GCC 16.2.0 + binutils 2.45.1 for RISC OS (arm-riscos-gnueabihf)
=============================================================================================

What it is
  A Linux x86-64 cross compiler (C, C++, Fortran, LTO) that makes 32-bit ARM EABI hard-float programs for RISC OS 5, with UnixLib 5.0's headers and libraries inside.
  Experimental: built and tested by one person on one machine (a Raspberry Pi Compute Module 4 with RISC OS 5.30).

Host requirements
  Linux x86-64 with glibc 2.38 or newer (Ubuntu 24.04, Debian 13, Fedora 39 or later).  Nothing else is needed.

Use it
  export PATH=<this directory>/bin:$PATH
  arm-riscos-gnueabihf-gcc      -O2 -o hello,e1f hello.c        C       (default -std=gnu23)
  arm-riscos-gnueabihf-g++      -O2 -o hello,e1f hello.cc       C++     (default -std=gnu++20)
  arm-riscos-gnueabihf-gfortran -O2 -o hello,e1f hello.f90      Fortran
  arm-riscos-gnueabihf-gcc -O2 -flto -o prog,e1f a.c b.c        link time optimisation
  The ",e1f" suffix gives the file the RISC OS file type ELF when it is copied to a RISC OS Samba share (or type  *SetType hello &E1F  on RISC OS).

Run the programs on RISC OS
  Install the runtime packages from the same release with PackMan: SharedLibs-C-armeabihf (always), SharedLibs-C++-armeabihf and SharedLibs-Fortran-armeabihf
  (only for programs that link libstdc++ or libgfortran dynamically, the default; link with -static-libstdc++ or -static-libgfortran to avoid that).
  Do not use -static-libgcc.

More
  Documentation, source, issues:  https://github.com/robheaton/riscos-gcc16
  The licences are in licenses/; this tool chain is GPL-3.0-or-later (libgcc, libstdc++ and libgfortran with the GCC Runtime Library Exception).
  The tree is relocatable: unpack it anywhere.  tests/cross-smoke/cross-smoke.sh in the repository checks a copy.
