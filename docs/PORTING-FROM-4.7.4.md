# Moving a module from GCCSDK 4.7.4

This page is for someone who has a module that GCCSDK 4.7.4 builds (`gcc -mmodule`, CMunge, the Shared C Library or `-znoscl`) and wants to build it with this tool chain. It says what works as it is, what to change, and what is still missing, so that you can decide quickly. The details of the kit are in [MODULES.md](MODULES.md).

**The short version.** The commands are the same (`cmunge -tgcc -32bit -p ...`, `gcc -mmodule ...`). The code is ARMv6, soft float, and uses a small C library of its own instead of the Shared C Library. The real modules of GCCSDK (the Shared Object Manager, ARMEABISupport and SysLogD) and its four example modules build from their own sources and Makefiles with this tool chain ([`modkit/tests/test-gccsdk-modules.py`](../modkit/tests/test-gccsdk-modules.py), [`test-gccsdk-examples.py`](../modkit/tests/test-gccsdk-examples.py)); that is how most of the differences below were found.

## What works as it is

| In your Makefile | Here |
|---|---|
| `cmunge -tgcc -32bit -p -d header.h -o header.o module.cmhg` | the same, with all the CMHG directives that GCCSDK's own modules use (SWI tables and decoding code, service calls, vectors, events, generic veneers with `private-word:` and `carry-capable:`, `error-chunk-base-number:` and `error-identifiers:` since 16.2.0-20) |
| `cmunge -zbase`, `-zoslib`, `-zerrors`, `-xh FILE`, `-xhdr FILE`, `-depend FILE`, `-apcs 3/32/nonreent/fpe2/noswst/nofpr/nofp` | accepted (`-depend` with the same warning as CMunge's: no file is written; the `-apcs` flags in CMunge's abbreviated forms) |
| `gcc -mmodule -O3 -std=gnu99 -c file.c` | the same; the code is made for ARMv6, soft float, ARM state, and `__TARGET_MODULE__` and `__TARGET_SCL__` are defined as before |
| `gcc -mlibscl -mmodule ...`, `-mpoke-function-name`, `-mthrowback`, `-mhard-float`, `-march=armv7-a` and the like | accepted, and `-mmodule` replaces the architecture and the float ABI (`-mlibscl` after `-mmodule` needs a compiler of 16.2.0-20 or later) |
| `gcc -mmodule -o Module,ffa main.o header.o` | the same: the driver links with the module linker script and runs `modreloc`, so the output is the flat module image |
| `-LOSLib: -lOSLib32` | works after `install-oslib.py --oslib-src <your svn checkout of OSLib> --toolchain <tool chain>` (new in 16.2.0-21; [OSLIB.md](OSLIB.md)): it builds **the real OSLib** with its own tools. OSLib is GPL with a linking exception and is not part of this project, so the library is built from your copy. X and non-X functions are all there, and the non-X ones are safe in modules (SVC mode). The older `OSLIB=<folder with oslib/> modkit/bin/mkoslib-lib.sh <tool chain>` still makes a smaller library from the headers alone |
| `mkresfs` (GCCSDK's tool for ResourceFS modules) | it writes C, so it works with this tool chain as it is |
| `asasm` for assembler files | GCCSDK's `asasm` writes ELF files that link here (the RISC OS modules of the OS sources were built that way) |

## What to change

* **Remove `-mfpu=fpa`, `-mfpu=fpe2`, `-mfpu=fpe3`, `-mapcs-26`.** The compiler does not know them (GCC 16 has no FPA). A module is soft float whatever the Makefile says, so nothing is lost.
* **Floating point.** 4.7.4 modules used the FPA instructions through the Floating Point Emulator, or the Shared C Library's own. A module here is soft float with exact `printf` and `scanf` of `%f %e %g %a`, `strtod` and the whole of `<math.h>` as far as it goes (below). The answers are those of glibc to the last bit for the exact functions, and within 1 to 2.2 units in the last place for the others.
* **The C library.** The Shared C Library is not used: there is no `-lscl`, no stubs, and `library-enter-code:` and `library-initialisation-code:` (which redirect its start-up) are refused. What the library has is in [MODULES.md](MODULES.md#the-c-library-of-modkit): the whole of C90 and C95 except the wide input and output functions, the `_kernel_` functions that modules use, `<swis.h>` (`_swi`, `_swix`), `stdio` with files, floating point, sockets (`<sys/socket.h>`, `<netdb.h>`), the descriptor functions on files (`open`, `read`, `write`, `lseek`, `fstat`, `ftruncate` ...), the POSIX and BSD helpers that 4.7.4's module library had (`stpcpy`, `setenv`, `gmtime_r`, `gettimeofday`, `getline`, `fnmatch`, `warn` ..., listed in [MODULES.md](MODULES.md#the-c-library-of-modkit); since 16.2.0-21), and the headers of GCCSDK's UnixLib that modules include for constants (`<sys/elf.h>`, `<sys/mman.h>`, `<fcntl.h>`, `<limits.h>`; the constants, not the functions).
* **SWI names.** `<swis.h>` has the names of the RISC OS Open sources and of GCCSDK's own list (`ARMEABISupport_MemoryOp`, `SysLog_LogMessage` ...), as plain macros. The `X` forms of the names that only GCCSDK has are not defined (its own modules use `[XName]` as the name of an assembler operand).
* **Build with `-lOSLib32` (see [OSLIB.md](OSLIB.md)) or `mkoslib`.** Without OSLib on the machine `mkoslib -I <OSLib>/oslib -o oslibv.c --from-objects main.o` writes the veneers of the functions that your objects use.

## What is the same as in 4.7.4

* **The stack.** The C code of a module runs on the kernel's SVC stack (32 KB for the whole OS) or the IRQ stack (8 KB), as it did: GCCSDK's own documentation says that module code in SVC and IRQ mode is compiled **without** stack checking (`-mmodule` implies `-mno-apcs-stack-check`), because those stacks are not chunked. There is no private stack and no overflow check; `long __modlib_stack_left (void)` (`<kernel.h>`) says how many bytes are left on the SVC stack.
* **C++** in a module: 4.7.4 had constructors and destructors; here they are there too (`module-is-c-plus-plus:`), and `new`, virtual functions, templates and the containers of the C++ library work as well ([MODULES.md](MODULES.md#c-in-modules)); there are no exceptions and no RTTI.

## What is still missing

| Missing | What to do |
|---|---|
| `library-enter-code:`, `library-initialisation-code:`, `-blank`, `-tnorcroft`, `-26bit`, `-apcs 3/reent` | nothing: these belong to the Shared C Library, to Norcroft and to old APCS variants |
| the CMunge-only directives `vector-traps:` and `pdriver-handler:`, the `help` flag of a command, `module-is-runnable: simple-app:`, `swi-handler-code: f (flags-capable:)` | write the handler by hand (a generic veneer does what `vector-traps:` does) |
| the C99 functions `erf`, `lgamma`, `tgamma`, `fma`, `remquo`, `scalbln`, `nexttoward`, `<complex.h>`, `<fenv.h>`, the wide input and output functions (`fwprintf`, `fgetwc` ...) | the Shared C Library of 4.7.4 did not have them either (it is C90 with a part of C95) |
| `stat`, `lstat`, `dup`, `dup2`, `fsync`, `access`, `mkdir`, `chdir`, `opendir`, `fork`, `getpid` ... | 4.7.4's module library did not have them either (a module has descriptors for files, `open`, `read`, `write`, `lseek`, `fstat` ... since 16.2.0-21, and `FILE *` streams)  |
| DeskLib (`libDesk.a`, `libDesk-sclmodule.a`) | built from your copy of [DeskLib32](https://github.com/adyoull/desklib32) with `install-desklib.py` ([DESKLIB.md](DESKLIB.md)); not shipped, its licence allows only unaltered copies. OSLib and OSLibSupport are built from source by `install-oslib.py` ([OSLIB.md](OSLIB.md)); the other libraries that GCCSDK ships for the old ABI cannot be linked: build them from source for this tool chain |
| Norcroft inline assembler, `__value_in_regs`, `__swi` (as the RISC OS sources use them) | GCC cannot read them: rewrite those blocks |
| a private stack, or stack checking | see above: 4.7.4 did not have them for module code in SVC and IRQ mode |

## Checking a Makefile

The quickest check is to build the module with the unchanged Makefile and look at the first error: it is one of the lines above in almost every case. If the error is something else, please report it with the line that failed ([the issues page](https://github.com/robheaton/riscos-gcc16/issues)): this page was written from exactly such lists, from the real modules of GCCSDK.
