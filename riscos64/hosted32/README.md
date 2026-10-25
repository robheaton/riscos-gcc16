# AArch64 (RISC OS 64) cross compiler that runs on 32 bit RISC OS

Martin (the RISC OS 64 author) asked for a native 32 bit version of GCC that makes 64 bit code.  This directory builds it: `aarch64-riscos-gcc`, `g++`, `cpp`, `as`, `ld`, `ar`, `nm`, `objdump`, `objcopy`, `readelf`, `strip`... as programs for **32 bit RISC OS** (EABI, UnixLib 5, the same runtime as the existing native GCC 16 package) that compile and link programs for RISC OS 64.

## State (25 Oct 2026)
Tested on a 32 bit RISC OS 5 machine (a Raspberry Pi Compute Module 4); the programs it made were run on RISC OS 64:
* C: hello.c compiled, linked and run: the assembly, object file and linked program are byte for byte the same as the Linux-hosted cross compiler's.
* C++: iostream/vector/string/sort program, and an exception test (throw/catch through frames with destructors, std::out_of_range from map::at, a thrown int): runs correctly.  The linked file differs from the Linux one in 44 four-byte `.eh_frame` words (FDEs of discarded duplicate functions: zero instead of a leftover pc-relative value); harmless.
* Module: Hello64 (-mmodule, with the C modreloc64 as the post-link step) is identical to the Linux-built module.
* Module tools in C (no Python), built for 32 bit RISC OS and put into the tree: `cmunge64` (src/cmunge64.c: the parser and C header of modkit/src/cmunge.c, the AArch64 assembler text of cmunge64.py; identical output on all 51 CMHG files of the repository, or the same refusal for the unsupported items), `mkoslib` (src/mkoslib64.c: modkit's mkoslib.c plus an ELF64 reader for `--from-objects` of 64-bit objects and archives) and `modreloc64` (../tools/modreloc64.c).  On the 32 bit machine: a module with a CMHG file (svc64) built with cmunge64 + `aarch64-riscos-gcc -mmodule` is identical to the Linux-built one; mkoslib gives the same veneers as the Python tool in function mode and `--from-objects` mode.  `compare-tools.sh` repeats the comparison against the Python tools on Linux.  (mkoslib on RISC OS finds a function's header by its name only, as modkit's native one does: xos_cli in os.h; the Linux build also scans the other headers.)
* Linker: sets the file type &FF8 of its AIF64 output when it runs on 32 bit RISC OS (checked on the 32 bit machine: `Info` shows Absolute, and the program runs on RISC OS 64 without SetType).
* Not done: a PackMan package, larger programs (zlib ...), a speed figure.
* Seen once, not reproduced: ld "BFD assertion fail elf-strtab.c:327" while linking the first C++ program; the same link worked 6 times afterwards (nothing changed).  Keep an eye on it.

## Build
`build-hosted32.sh WORK` (about 25 minutes): GCC and binutils for host arm-riscos-gnueabihf and target aarch64-riscos, then `stage-tree.sh` makes `WORK/tree-ro/gcc16a64r` (the RISC OS layout: `stdio.h` is `h/stdio`, `crt0.o` is `o/crt0` ... because UnixLib swaps those suffixes).
Things that were needed (each found by testing):
* `patches/gcc-hosted-riscos32.patch`: config.host entry for arm-riscos hosts with an aarch64-riscos target, the heap/stack file (`riscos32-host.c`: the compiler proper gets a 512 MB heap and 64 MB stack), the driver hooks (`riscos32-gcc.c`: file names), and the host patches of the 32 bit native compiler (response files in libiberty, off_t, physmem).
* `--with-build-time-tools=probe/` (symlinks to the Linux AArch64 binutils): configure has to RUN an assembler to learn that it supports `.p2align max-skip`, mergeable strings ...; without it the compiler is configured "assembler knows nothing" and makes different (valid but worse) code.
* libctf: UnixLib has `qsort_r` but declares nothing: `qsortr.h` forced in.
* the `specs` make target runs the ARM driver on Linux (fails): stage-tree.sh makes the file from the Linux-hosted driver (same text).

## Using it on the 32 bit machine
`Obey <dir>.Setup` (Run$Path, return code limit, filename suffix swapping), and run in a Task window with room: `WimpSlot -min 64M -max 64M` BEFORE starting the programs (a slot cannot be changed while an EABI program runs in it).  Then e.g.
`aarch64-riscos-gcc -O2 -munixlib -o hello hello.c`; the linker gives the output the file type FF8 (AIF64) itself (`patches/ld-aif64-filetype.patch`).  `-mmodule` builds a module; `cmunge64 -d x_h.h -o hdr.o x.cmhg` makes its header object (needs UnixEnv$cmunge64$sfix, which Setup sets).
