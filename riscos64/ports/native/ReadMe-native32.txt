riscos64-native32-@VERSION@ - the compiler that RUNS on RISC OS 64 and makes programs for 32-bit RISC OS: C and C++ (PREVIEW)
==============================================================================================================================

GCC 16.2.0 (the C compiler cc1 and the C++ compiler cc1plus) and the GNU assembler and linker (binutils 2.45.1), built as programs for RISC OS 64, with the target arm-riscos-gnueabihf: the programs it makes run on 32-bit
RISC OS 5 (file type &E1F), not on RISC OS 64.  The headers and libraries (UnixLib 5, libgcc, libstdc++) are those of the 32-bit native package (Gcc16).  The object files are the same, byte for byte, as the Linux
cross compiler makes from the same source (checked for a C and a C++ program).

Try it: double-click one of the Run files in this folder.
  RunTo32     compiles and links a C program and a C++ program (iostream, vector, sort, an exception): out.hello32 and out.hellopp32
  RunZlib32   compiles zlib 1.3.1 (16 files) and links its test program zl.out.zexample32
Copy the result to a 32-bit machine and run it there.
The Run files are not Obey files.  A Run file is a small launcher program (ulrun, source in the repository) that reads the script of the same name in the folder scripts, and runs its lines one by one: a program that is
started from an Obey file cannot start a program and come back, and the compiler is a driver that starts cc1, as, collect2 and ld.  The scripts are text files that you can read and change.

Your own program
  1. Copy RunTo32 (the launcher) to RunMine in the same folder, and scripts.RunTo32 to scripts.RunMine.
  2. Put your source in the folder c (C) or cc (C++): a file hello.c is c/hello, hello.cc is cc/hello.
  3. Change the compiler lines of scripts.RunMine and double-click RunMine.
The lines of the C sample:
  <T64$Dir>.xgcc -B bin/ -B lib/ -L lib -isystem include -O2 -x c -c c/hello -o o/hello32
  <T64$Dir>.xgcc -B bin/ -B lib/ -L lib o/hello32 -o out/hello32
(for C++ use xg++ and -isystem include/c++/16.2.0 -isystem include/c++/16.2.0/arm-riscos-gnueabihf -isystem include.)  Run the script Clean before each compiler line (as the samples do): the compiler leaves its COMPILER_PATH,
LIBRARY_PATH, GCC_EXEC_PREFIX and COLLECT_* variables set when it ends.  A response file (@file) works around the 255 character command line limit (zl/link).  The -B, -L and file names must be relative to this
folder (the launcher goes to it first): absolute -B paths make collect2 lose ld.  lib/specs makes the link work without extra options.  A link with very many object files can fail on some file systems (too many
files open at once): put them into an archive first with bin.ar, as RunZlib32 does.

Needs and limits
  * RISC OS 64 and a big application slot: the scripts set WimpSlot -min 128M -max 128M (a large C++ program may need 320M).  C and C++ (no Fortran, no LTO).
  * Programs link the static libstdc++ (about 780 KB for a C++ hello).  A 32-bit machine needs the C runtime of the Gcc16 package (SharedLibs-C-armeabihf) to run them.
  * The driver (xgcc, xg++) and collect2 have a small heap on purpose (see the ReadMe of riscos64-native).  Programs that are killed leave their memory areas behind until the next reboot.
  * Preview.  It is not a PackMan package.

Licences: GCC and binutils are under the GNU General Public License version 3 or later; the libraries are UnixLib 5 (BSD and LGPL), libgcc and libstdc++ (GPL with the GCC Runtime Library Exception, so programs you build
may be licensed as you like); zlib has its own licence (Licences.zlib-LICENSE).  The texts are in the folder Licences.
Source, patches and build scripts: https://github.com/robheaton/riscos-gcc16 (the tag riscos64-native32-@VERSION@; the unmodified GCC and binutils sources are at gnu.org).
