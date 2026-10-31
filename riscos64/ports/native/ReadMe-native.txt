riscos64-native-@VERSION@ - the GCC compiler for RISC OS 64, running on RISC OS 64: C and C++ (PREVIEW)
=====================================================================================================

GCC 16.2.0 (the C compiler cc1 and the C++ compiler cc1plus), the GNU assembler and linker (binutils 2.45.1) and the headers and libraries (UnixLib 64, libstdc++), built as programs for RISC OS 64,
so that you can compile programs and modules for RISC OS 64 on the machine itself.  It makes programs for RISC OS 64.  (To make programs for 32-bit RISC OS from RISC OS 64, see riscos64-native32.)

Try it: double-click one of the Run files in this folder.
  RunNatCxx   compiles, links and runs a C++ program (iostream, vector, sort, an exception): about 8 seconds
  Run64Zlib   compiles zlib 1.3.1 (16 files) with the C compiler, links and runs its test program: about 20 seconds
The Run files are not Obey files.  A Run file is a small launcher program (ulrun, source in the repository) that reads the script of the same name in the folder scripts, and runs its lines one by one: a program that is
started from an Obey file cannot start a program and come back, and the compiler is a driver that starts cc1, as, collect2 and ld.  The scripts are text files that you can read and change.

Your own program
  1. Copy RunNatCxx (the launcher) to RunMine in the same folder, and scripts.RunNatCxx to scripts.RunMine.
  2. Put your source in the folder cc (a C++ file hello.cc is cc/hello) or c (C: a file hello.c is c/hello).
  3. Change the two compiler lines of scripts.RunMine (for C use xgcc and -x c, as scripts.Run64Zlib does), and double-click RunMine.
The compile line of the C++ sample:
  <T64$Dir>.xg++ -B bin/ -B lib/ -L lib -isystem include/c++/16.2.0 -isystem include/c++/16.2.0/aarch64-riscos -isystem include -O1 -x c++ -c cc/hellopp -o o/hellopp
and its link line:
  <T64$Dir>.xg++ -B bin/ -B lib/ -L lib -munixlib o/hellopp -o out/hellopp
Run the script Clean before each compiler line (as the samples do): the compiler leaves its COMPILER_PATH, LIBRARY_PATH, GCC_EXEC_PREFIX and COLLECT_* variables set when it ends, and the next run starts clean without them.
A command line is limited to 255 characters by the kernel: put long lists of files in a response file and give it as @file (see zl/link).  A link with very many object files can fail on some file systems (too many files
open at once): put them into an archive first with bin.ar, as Run64Zlib does.

Needs and limits
  * RISC OS 64 and a big application slot: the scripts set WimpSlot -min 128M -max 128M (a large C++ program may need 320M; C needs less).  About 150 MB of free application memory.
  * C and C++ (no Fortran, no LTO).  A C++ file with iostream takes seconds to a minute.
  * The driver (xgcc, xg++) and collect2 have a small heap on purpose, so that the compiler and the linker can have a big one: do not relink them without -Wl,--defsym=__ul64_heap_max=0x1000000.
  * Programs that are killed leave their memory areas behind until the next reboot.
  * Preview: it works, but it is rough, and it is not a PackMan package yet.  Unlike riscos64-native-0.13 it has no Build command and no Lua, make or sed examples (they are in that release).

Licences: GCC and binutils are under the GNU General Public License version 3 or later; the libraries are UnixLib 64 (BSD and LGPL) and libstdc++ (GPL with the GCC Runtime Library Exception, so programs you build may be
licensed as you like); zlib has its own licence (Licences.zlib-LICENSE).  The texts are in the folder Licences.
Source, patches and build scripts: https://github.com/robheaton/riscos-gcc16 (the tag riscos64-native-@VERSION@; the unmodified GCC and binutils sources are at gnu.org).
