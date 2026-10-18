aarch64-riscos GCC 16.2.0 (RISC OS 64), tool chain @VERSION@  --  a cross compiler from Linux (x86-64) to AIF64 programs and 64-bit modules for RISC OS on AArch64
==========================================================================================================================================================

This is a PRE-RELEASE: it has been tested on RISC OS Pyromaniac (build service), and in part on a real RISC OS 64 machine.  See share/riscos64/doc/KNOWN-ISSUES.md.

What is in it
  bin/aarch64-riscos-gcc, -g++, -ld, -as, -ar, -objcopy, ...   GCC 16.2.0 and binutils 2.45.1 for the target aarch64-riscos
  bin/cmunge64, bin/mkoslib                                     the module kit: CMHG files to module headers, OS library headers
  aarch64-riscos/                                               the SDK: C library (picolibc plus the SDK glue), UnixLib 64, libstdc++ (C++20, threads), start-up files, linker script, module kit
  share/riscos64/doc/                                           RISCOS64.md (the design), SDK.md, MODULES64.md and KIT.md (modules), UNIXLIB64.md, KNOWN-ISSUES.md
  share/riscos64/source/                                        the patches to GCC and binutils, the scripts that built this, the sources of the SDK and UnixLib 64 (the source offer; see also SOURCES.md)

Use
  export PATH=$PWD/bin:$PATH
  aarch64-riscos-gcc -O2 hello.c -o hello             -> hello,ff8   (an AIF64 program; the ,ff8 is the RISC OS file type)
  aarch64-riscos-gcc -munixlib prog.c -o prog         with UnixLib 64: Unix file names, processes (vfork, exec, popen), signals, threads, sockets
  aarch64-riscos-g++ -munixlib -pthread prog.cc       C++20 with the standard library, std::thread and the rest
  aarch64-riscos-gcc -mmodule -O2 mod.c ...           a 64-bit module (see MODULES64.md; cmunge64 writes the header from a CMHG file)
Run ./selftest.sh to check that it works on your machine.  The programs run on RISC OS 64 and on RISC OS Pyromaniac (https://github.com/gerph/pyromaniac; the build service at riscos.online).

Limits to know about
  * Threads are cooperative (they switch in blocking calls and sched_yield): no preemption.  One processor.
  * Sockets are written to the documented Internet module interface and have not been tried on a real module.  IPv4 only.
  * No Shared C Library (SCL) and no shared libraries: programs are linked statically.
  * SIGSEGV and friends are delivered only to programs that install a handler.

Licences: GCC and binutils GPL-3.0 (with the runtime library exception for libgcc and libstdc++), picolibc BSD-style, UnixLib its own licence: see share/riscos64/licenses/ and LICENSES.md.
