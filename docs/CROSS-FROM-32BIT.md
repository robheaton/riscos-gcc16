# The RISC OS 64 compiler on a 32-bit RISC OS machine

**Pre-release (0.1).** `riscos64-cross32-0.1` is the AArch64 cross compiler of the [RISC OS 64 tool chain](AARCH64.md), built as programs for **32-bit RISC OS 5**, so that you can compile programs and modules for RISC OS 64 on a 32-bit machine (a Raspberry Pi 3 or 4, for example). It is the same GCC 16.2.0 and binutils 2.45.1 as the Linux cross compiler, and it produces the same files: the assembly, the object files, the linked programs and the modules it makes are byte for byte the same as the Linux compiler's (the one exception is listed under [What is tested](#what-is-tested)).

It was asked for by Martin, the main author of RISC OS 64: *a native 32-bit GCC that makes 64-bit code*.

## What you need

* a 32-bit RISC OS 5 machine with the C runtime of the [Gcc16 package](INSTALL-RISCOS.md) (`SharedLibs-C-armeabihf`, SharedUnixLibrary 1.12 or later), about 150 MB of disc and plenty of memory (the compilers proper are big programs: a heap of up to 512 MB is reserved for each);
* RISC OS 64 (or the 64-bit mode of the RISC OS Pyromaniac emulator) to run what you make.

## Install and use

Unzip `Gcc64On32-0.1.zip` from the [release](https://github.com/robheaton/riscos-gcc16/releases/tag/riscos64-cross32-0.1): it is one application, `!GCC64`.

1. Double-click `!GCC64` once after each boot: it sets the search path, the return code limit and UnixLib's file name translation for every program of the tool chain.
2. Open a Task window and give it room **before** you start a compiler: `WimpSlot -min 64M -max 64M` (the slot of a program that is running cannot be changed; in a small slot the compiler stops with "WimpSlot not big enough to run ELF program").
3. Go to your work directory and compile. Source files live in the usual RISC OS directories (`hello.c` is `c.hello`, `util.h` is `h.util`, `main.o` is `o.main`):

   ```
   aarch64-riscos-gcc -O2 -munixlib -o hello hello.c          a program for RISC OS 64 (file type &FF8: run it there)
   aarch64-riscos-g++ -O2 -munixlib -o hellopp hellopp.cc     C++ with the standard library and exceptions
   aarch64-riscos-gcc -c -O2 foo.c                            an object file
   ```

4. A module, from a CMHG file and its C code:

   ```
   cmunge64 -d foo_h.h -o foo_hdr.o foo.cmhg
   aarch64-riscos-gcc -mmodule -O2 foo.c foo_hdr.o -o Foo     a 64-bit module (file type &FFA)
   ```

   The last step runs `modreloc64` for you. See [MODULES64.md](MODULES64.md) for the module kit, and `mkoslib` for the OSLib veneers.

Copy the result to the RISC OS 64 machine (or to a share that both machines see) and run it there. The linker gives its output the file type &FF8 itself.

## What is in it

| Where | What |
|---|---|
| `bin/aarch64-riscos-gcc`, `-g++`, `-cpp`, `-ar`, `-nm`, `-objdump`, ... | GCC 16.2.0 (C and C++) and the GNU tools of binutils 2.45.1 for the target `aarch64-riscos` |
| `bin/cmunge64`, `bin/mkoslib` | the module kit: CMHG files to module headers, and the OSLib veneers (C programs: there is no Python on RISC OS) |
| `aarch64-riscos/bin/` | `as`, `ld`, `modreloc64` and the other binutils; the drivers find them here |
| `aarch64-riscos/include`, `aarch64-riscos/lib`, `lib/gcc/aarch64-riscos/16.2.0` | the headers and libraries of the 64-bit SDK: picolibc, UnixLib 64, libstdc++, libgcc, the start-up files and linker scripts, the module kit |
| `tests/` | `hello.c` and an Obey file that compiles it |

## What is tested

On a 32-bit RISC OS 5 machine (a Raspberry Pi Compute Module 4), with the programs run on RISC OS 64:

* **C:** a program compiled, assembled and linked on the 32-bit machine: the assembly, the object file and the linked program are the same, byte for byte, as the Linux cross compiler's, and it runs on RISC OS 64.
* **C++:** a program that uses `iostream`, `vector`, `string` and `sort`, and an exception test (a throw and catch through several frames with destructors, `std::out_of_range` from `map::at`, a thrown `int`): it runs correctly on RISC OS 64. The linked file differs from the Linux one in 44 four-byte words of the `.eh_frame` table (entries for discarded duplicate functions: zero here, a leftover value there); the exception test passes.
* **Modules:** `Hello64`, and a module with a CMHG file (`Svc64`), built on the 32-bit machine are byte for byte the same as the Linux-built ones.
* **The module tools in C:** `cmunge64`, `mkoslib` and `modreloc64` give the same output as their Python originals on every CMHG file of the repository (51 files; where a file uses something that is not supported, both refuse with the same message), and `mkoslib` gives the same veneers in function mode and for the objects and archives of 64-bit modules (`--from-objects`). [`riscos64/hosted32/compare-tools.sh`](../riscos64/hosted32/compare-tools.sh) repeats the comparison.

Not tried yet: larger programs (zlib, Lua, ...) with timings, and a PackMan package.

## Limits

* It is a cross compiler: what it makes cannot run on the 32-bit machine.
* The compilers are big and take time: a small C file takes seconds, the first C++ file with `iostream` about a minute on a Raspberry Pi 4 class machine.
* A command line can be limited to 255 characters by the kernel: use `@file` response files for long lists.
* No Fortran, no LTO and no plug-ins. `cmunge64` does not support veneers or runnable modules (as on Linux).
* `mkoslib` finds the header of a function by its name only (`xos_cli` is in `os.h`); the Linux tool also searches the other headers.
* The linker stopped once with a BFD assertion (`elf-strtab.c`) on the first C++ link of the tests, and did not in the many links that followed. If you see it, link again, and tell us.

## How it is built

[`riscos64/hosted32/`](../riscos64/hosted32/) has the scripts and the patch. `build-hosted32.sh WORK` builds GCC and binutils with the 32-bit cross compiler of this repository (host `arm-riscos-gnueabihf`, target `aarch64-riscos`; about 25 minutes on 22 cores) and `stage-tree.sh` makes the tree in the RISC OS layout; `package-hosted32.py` packs the zip. What was needed, each point found by testing:

* the host side of the 32-bit native compiler (`riscos32-host.c`: a 512 MB heap and a 64 MB stack for the compiler proper; the driver's file name hooks; response files in libiberty) is added to the AArch64 target sources (`patches/gcc-hosted-riscos32.patch`);
* configure must *run* an assembler and a linker for AArch64 to learn what they can do: `--with-build-time-tools` points it at the Linux ones. Without that, the compiler is configured as if the assembler knew nothing and makes valid but different code;
* the files must be in the RISC OS layout (`stdio.h` as `h/stdio`, `crt0.o` as `o/crt0`), because UnixLib swaps those suffixes;
* the linker sets the file type &FF8 of its output when it runs on 32-bit RISC OS (`patches/ld-aif64-filetype.patch`);
* the module tools are C programs: `cmunge64` (the parser of the 32-bit `cmunge`, the AArch64 header of `cmunge64.py`), `mkoslib` (with a reader for the 64-bit objects) and `modreloc64`.
