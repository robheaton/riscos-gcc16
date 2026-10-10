# riscos-gcc16: GCC 16 for RISC OS

A modern compiler tool chain for RISC OS: **GCC 16.2.0** (C, C++ and Fortran) and **binutils 2.45.1**, for 32-bit ARM RISC OS 5 machines (`arm-riscos-gnueabihf`: EABI, hard float, UnixLib).
It comes in two forms:

* a **native compiler that runs on RISC OS itself**: `gcc`, `g++`, `gfortran`, binutils and GNU `make`, installed with PackMan;
* a **cross compiler for Linux** (x86-64) that produces the same programs.

It is a forward port of the GCCSDK GCC 10.2.0 EABI tool chain, together with a rebuilt and fixed **UnixLib** runtime.

> **Status: experimental.** Everything here has been built and tested by one person on one machine (a Raspberry Pi Compute Module 4, Cortex-A72, RISC OS 5.30).
> It is not part of, and not supported by, the GCCSDK project. Please report problems [here](https://github.com/robheaton/riscos-gcc16/issues), not to the GCCSDK mailing list.

## What you get

| | Native (on RISC OS) | Cross (on Linux) |
|---|---|---|
| **C**, GCC 16.2.0 (default `-std=gnu23`; C89 to C23) | yes | yes |
| **C++**, libstdc++ 6.0.36 (default `-std=gnu++20`; up to C++23): exceptions, RTTI, iostreams, threads, `<format>`, `<print>`, `<ranges>`, `<expected>` | yes | yes |
| **Fortran**, gfortran 16.2.0 (Fortran 2018) | yes | yes |
| **Link time optimisation** (`-flto`) | yes (fat objects; see [limits](docs/KNOWN-ISSUES.md)) | yes (linker plugin) |
| **Coverage and profile-guided optimisation** (`--coverage`, `gcov`, `-fprofile-generate` / `-fprofile-use`) | yes | yes (the Linux `gcov` reads what the RISC OS program wrote) |
| **binutils 2.45.1** (`as ld ar nm objdump objcopy readelf strip ranlib size strings addr2line c++filt elfedit gprof`) | yes | yes |
| **GNU make 4.4.1** | yes | not needed |
| **Profiling** (`-pg`, `gprof`: exact call counts, time sampled 50 times a second) | yes (`gprof` is in the package) | yes (the program writes `gmon.out` on RISC OS, the Linux `arm-riscos-gnueabihf-gprof` reads it) |
| **Throwback** (`-mthrowback`: errors and warnings of the compilers, the assembler and the linker in your text editor) | yes, through DDEUtils (tested with StrongED) | yes, through the SysLogD module (tested on the host only) |
| Stack probing on by default (`-fstack-clash-protection`: needed by RISC OS's lazily mapped stacks) | yes | yes |
| Shared libraries, `dlopen`, threads (`pthread`, `std::thread`, `std::async`), thread-local storage | yes | yes |
| Tuning for ARMv7 and Cortex-A72 (`-mcpu=cortex-a72 -mfpu=neon-fp-armv8 -mfloat-abi=hard`) | yes | yes |
| UnixLib 5.0 rebuilt with GCC 16, plus a series of fixes (fix level 15, [list](docs/RUNTIME.md)) | the runtime packages | the runtime packages |
| Experimental: build relocatable **modules** with `gcc -mmodule` and `cmunge`, the way GCCSDK 4.7.4 did. **For small and medium C and C++ modules, for now**: a C library of about 320 functions of its own (strings, `ctype`, `stdlib`, `time`, `printf`, `sscanf`, `stdio` files, `_swix` ... and, since 16.2.0-17, floating point: exact `%f %e %g %a`, `strtod`, `<math.h>` (run on hardware), and C++: static constructors, `new` and `delete`, `std::string`, `std::vector`, `std::map` ... without exceptions, RTTI, streams or threads (both run on hardware); 16.2.0-14 had 30), and `cmunge` knows most of CMHG (international help, `module-is-runnable` ...). [What can and cannot be built](docs/MODULES.md#what-can-be-built-today-and-what-cannot); for anything bigger GCCSDK 4.7.4 remains the way | yes (C modules; C++ modules are built on Linux) | yes |
| Optional: a **fixed SharedUnixLibrary** module, for programs whose `vfork` children can fail to `exec` (it replaces a system module: [read this first](docs/SHAREDULIB-FIX.md)) | the package `SharedULibFix` | not needed |

Not included: OpenMP, the sanitizers, wide-character iostreams (`std::wcout`), `std::stacktrace`, `REAL(16)` in Fortran, a debugger. The full list, with
how each statement was tested, is in [docs/FEATURES.md](docs/FEATURES.md) and [docs/KNOWN-ISSUES.md](docs/KNOWN-ISSUES.md).

## Quick start: compile on RISC OS

1. Download from the [latest release](https://github.com/robheaton/riscos-gcc16/releases/latest) `SharedLibs-C-armeabihf_16.2.0-13_arm.zip` and `Gcc16_16.2.0-19_arm.zip` (70 MB).
   A zip must have file type Zip (&A91); if it arrives as Text or Data, type `*SetType <file> &A91` in a Task window.
2. **Drag each zip onto the PackMan icon on the icon bar**, first `SharedLibs-C-armeabihf`, then `Gcc16` (PackMan insists on this order), and confirm the installs.
3. **Reboot.** (PackMan replaces the files, but the running system keeps the old UnixLib until the machine restarts.)
4. Double-click `!GCC16` (PackMan puts it in your `Apps.Utilities` directory). It sets up the search path and the filename translation the compiler needs; do it again after each reboot.
5. In a directory of your own, make a directory `c` and, in it, a text file `hello` that contains
   ```c
   #include <stdio.h>
   int main (void) { puts ("hello from GCC 16 on RISC OS"); return 0; }
   ```
6. Open a Task window (Ctrl-F12), go to the directory that contains `c` (replace the path below with your own), give the window room, and compile and run:

   ```
   Dir ADFS::HardDisc4.$.Work
   WimpSlot -min 48M -max 48M
   gcc -O2 -o hello hello.c
   hello
   ```
   (UnixLib maps the Unix name `hello.c` to `c.hello`; the linker gives `hello` the file type ELF, &E1F.)

`g++ -O2 -o hello hello.cc`, `gfortran -O2 -o hello hello.f90`, `gcc -O2 -flto ...`, `gcc -Wall -mthrowback -c ...` and `make` work the same way.
The full guide is [docs/INSTALL-RISCOS.md](docs/INSTALL-RISCOS.md) and [docs/USING-NATIVE.md](docs/USING-NATIVE.md).

## Quick start: cross-compile on Linux

You need Linux x86-64 with glibc 2.38 or newer (Ubuntu 24.04, Debian 13, Fedora 39 or later).

```bash
tar -xf riscos-gcc16-cross-16.2.0-19-x86_64-linux.tar.xz
export PATH=$PWD/riscos-gcc16-cross-16.2.0-19-x86_64-linux/bin:$PATH
arm-riscos-gnueabihf-gcc -O2 -o hello,e1f hello.c        # ,e1f gives the file type ELF when the file is copied over a Samba share
```

Copy `hello,e1f` to the RISC OS machine and run it there; it needs the `SharedLibs-C-armeabihf` package (step 2 above), and C++ and Fortran programs need `SharedLibs-C++-armeabihf` and
`SharedLibs-Fortran-armeabihf` as well, unless you link those libraries statically. See [docs/CROSS-COMPILER.md](docs/CROSS-COMPILER.md). To build the tool chain from source: [docs/BUILDING.md](docs/BUILDING.md).

## Downloads

Everything is attached to the [releases page](https://github.com/robheaton/riscos-gcc16/releases). Check the downloads against `SHA256SUMS`.

| File | What it is | Needed for |
|---|---|---|
| `SharedLibs-C-armeabihf_16.2.0-13_arm.zip` (2 MB) | the C runtime: UnixLib 5.0 rebuilt with GCC 16 and fixed, loader, libgcc_s | running **any** program from this tool chain |
| `Gcc16_16.2.0-19_arm.zip` (70 MB) | the native compilers and tools, as `!GCC16` | compiling on RISC OS |
| `Gcc16SelfTest_16.2.0-19_arm.zip` (19 KB) | the self-test of the native compiler, as `!GCC16Test` | checking an installation |
| `SharedULibFix_1.16-vforkfix3_arm.zip` (24 KB) | the fixed SharedUnixLibrary 1.16-vforkfix3 with an installer that checks everything, a restore script and a check, as `!SULFix`. **It replaces a system module: [read this first](docs/SHAREDULIB-FIX.md)** | programs that `vfork` children that can fail to `exec` (optional) |
| `SharedLibs-C++-armeabihf_16.2.0-5_arm.zip` (0.7 MB) | libstdc++ 6.0.36 | running C++ programs that link it dynamically (the cross compiler's default) |
| `SharedLibs-Fortran-armeabihf_16.2.0-2_arm.zip` (0.4 MB) | libgfortran 5 | running Fortran programs that link it dynamically (the cross compiler's default) |
| `riscos-gcc16-cross-16.2.0-19-x86_64-linux.tar.xz` (54 MB) | the cross compiler for Linux | compiling on Linux |
| `gcc-16.2.0.tar.xz`, `binutils-2.45.1.tar.xz`, `make-4.4.1.tar.gz`, `gccsdk-unixlib-r7800.tar.xz` | the unmodified upstream sources the binaries were built from (UnixLib is a snapshot of GCCSDK svn r7800) | the source offer, see [SOURCES.md](SOURCES.md) |

## How it was tested

On a Raspberry Pi Compute Module 4 (Cortex-A72) with RISC OS 5.30, ARMEABISupport 1.08 and Shared Object Manager 3.04. The compiler, UnixLib and the runtime packages of this release are those of 16.2.0-16 and earlier (16.2.0-17, -18 and -19 changed the module kit only); they passed all of the following on that machine (what was run with the packages of 16.2.0-19 is in the last paragraph of this section):

* the C regression suite `rotest` (34,541 checks: integers, 64-bit arithmetic, floating point, conversions, varargs, `alloca`, `setjmp`, unwinding, atomics, PIC data, C23) and the C++ suite `cxxtest` (139 checks), built both
  by the cross compiler and by the native compiler (the native compiler's objects are byte-identical to the cross compiler's);
* the thread tests (`std::thread`, `std::async`, `call_once`, timed waits, `thread_local`), the Fortran suite (122 + 54 + 26 + 30 + 9 checks, plus an error-exit test) and a dynamic-library suite;
* the native compiler building real software: zlib 1.3.1 (also with `-flto`) and GNU make 4.4.1 itself;
* the runtime's own checks: 48 library checks, 23 memory-guard checks, 12 process-exit checks, six heap-growth scenarios of `vfork` + `exec` children, a table of 15,066 `sscanf` cases, the `.fini_array` (destructors), `getrlimit (RLIMIT_STACK)`, 20 POSIX semaphore checks and the `.gcda` files of coverage and profile programs (16.2.0-12), and, new in 16.2.0-13, the profiler behind `-pg`;
* coverage: programs built by the cross compiler wrote their `.gcda` files on the Pi and the Linux `gcov` read them; the native compiler did the same (`--coverage`), the native `gcov` read the file and wrote the annotated source (80.00% of 15 lines, every line count checked by a small program), and `-fprofile-use` found the profile of an instrumented run (checks 9 and 10 of the self-test);
* gprof: a program built by the cross compiler with `-pg` ran on the Pi in three runs of 6 seconds and wrote a valid `gmon.out` each time: the call counts equal the program's own counters exactly, 237 to 240 samples at 50 a second, and the native `gprof` and the Linux `gprof` print the same profile; the native compiler did the same (`gcc -O1 -pg`, a run, `gprof`): a program that calls two functions ten times each gets exactly 10 calls for each in the report (check 11 of the self-test);
* throwback: the native assembler and the native linker send their errors to StrongED (a throwback window with the entries; a double click opens the source at the line); the compilers' throwback was tested before (16.2.0-8);
* modules (16.2.0-14): the self-test builds a small module on the Pi with `cmunge` and `gcc -mmodule`, loads it, runs its command and removes it; a network module of 800 lines of C (sockets, files and OS calls through 23 OSLib veneers) was built on the Pi in 3 seconds by the commands of a GCCSDK 4.7.4 makefile, is **byte for byte the module that the Linux cross compiler makes from the same source**, and passed 39 network checks on the machine (a soak of 300 commands, files up to 500 KB, a client that vanishes). The same test with the packages of 16.2.0-15 (the new kit): 4 seconds, byte for byte the module that the Linux tarball makes, and the same 39 network checks.
* the module kit of 16.2.0-15 (run on the Pi on 2026-10-07, before the release): the kit's C library against glibc on the real file system (every compared section equal, the `stdio` section of 21,068 results among them; also the heap, `setjmp`, `getenv` and the real SWIs), a module run as a program with `module-is-runnable` (`*RMRun` with arguments, the exit codes), international help, `add-syntax:`, a SWI prefix that differs from the title, a generic veneer in SVC mode, in USER mode and at interrupt time, `exit` in a command, and the keyboard and screen streams of `stdio`.
* three modules of the RISC OS Open sources built with the module kit of 16.2.0-16 (run on the Pi on 2026-10-08, before the release): Squash (with its assembler), MimeMap and DrawFile, loaded over the ROM's own modules, gave the same results as the ROM's in everything the same test program compares: 545 checks on each side, 9,820 lines of results compared and none different, the ROM's modules started again afterwards, and DrawFile painted 13 cases (8 Draw files) into a sprite with the same pixel count, bounding box and checksum ([docs/OS-MODULES.md](docs/OS-MODULES.md#compared-with-the-rom)). Running the same program on an interpreter beforehand had found two faults of `cmunge` in 16.2.0-14 and 16.2.0-15, fixed in 16.2.0-16 ([CHANGELOG](CHANGELOG.md));
* floating point, `math.h` and C++ in modules, built with the module kit of 16.2.0-17 (run on the Pi on 2026-10-09 and 2026-10-10, before the release): a module (`FpSvc`) that formats and reads floating point numbers, does arithmetic and calls `math.h` in SVC mode and in a generic veneer at interrupt time gave the answers that glibc and Python give (892 + 791 + 10 checks, and the same bits as the host in 100 interrupt-time calls); two modules written in C++ ran their static constructors and destructors in the right order, `new` and `delete`, virtual functions, and `std::string`, `std::vector`, `std::map`, `std::set`, `std::list`, `std::unordered_map`, smart pointers and `std::function`, printing the same text as the host (33 lines, 0 differ); and the kit's C library as a module (`LibTest`) gave glibc's results in every compared section, 318,000 more results for floating point among them ([docs/MODULES.md](docs/MODULES.md#c-in-modules)).
* the CMHG options of 16.2.0-18 and the faults of the kit that it fixes (run on the Pi on 2026-10-10, before the release): a test module (`CmhgT`, 65 checks) with a SWI function of its own, `handler:` and `no-handler:` commands, generic veneers with `private-word:` and `carry-capable:` and vector veneers with `error-capable:`, all with real flags; two modules with `swi-decoding-code:` (the kernel asks the code for names and numbers; the first run found two faults, fixed); and a module (`KitFix`) that checks that an undefined weak reference and an absolute symbol stay 0 ([docs/MODULES.md](docs/MODULES.md), [docs/OS-MODULES.md](docs/OS-MODULES.md#new-in-1620-18)).
* BSD sockets and the OSLib veneers in modules, built with the module kit of 16.2.0-19 (run on the Pi on 2026-10-10, before the release): a module (`SockHw`) that does TCP and UDP on the loopback address (non-blocking `connect`, `select`, `accept` with the peer's address, `shutdown`, `EBADF`, `EWOULDBLOCK`, `ECONNREFUSED`), resolves `localhost` through the Resolver, and calls the X and non-X OSLib functions of `libOSLib32.a`: 53 checks, 0 failed; an echo server in the same module answered a client on another machine (three messages and the end of file); a non-X OSLib error ended the whole command line, as a non-X SWI error does; and `__modlib_stack_left ()` read 32,172 bytes at the top of a command and fell with the depth of the recursion. A private stack for modules was tried in the same session: the Task window aborted ([docs/MODULES.md](docs/MODULES.md#sockets)).
* the fixed SharedUnixLibrary (the optional `SharedULibFix` package 1.16-vforkfix3, since 16.2.0-14): the module has been installed on the test machine since 4 Oct 2026, the `vfork` loops that froze the machine with the stock module run without a freeze, and the regression suites pass with it; the installer's logic runs through 28 simulated scenarios; the installer itself was run on the machine (`Install` on the stock module, a reboot, `Check`, `Restore`, `Install` again, a reboot, `Check`, and an `Install` that correctly refused): all passed.

The packages of this release were installed with PackMan on that machine and checked with the self-test (`tests/selftest`: twelve checks, 32 seconds; check 12 builds, loads and runs a module with the kit of this release).
How the build instructions were checked for this release is at the top of [docs/BUILDING.md](docs/BUILDING.md).
The test programs are in [tests/](tests/) and are described in [docs/TESTING.md](docs/TESTING.md). Only one machine was used: other ARMv7 machines should work, but have not been tried.

## Documentation

| | |
|---|---|
| [docs/INSTALL-RISCOS.md](docs/INSTALL-RISCOS.md) | requirements, installing, checking, upgrading and removing the packages |
| [docs/USING-NATIVE.md](docs/USING-NATIVE.md) | using `gcc`, `g++`, `gfortran`, `make`, `-flto` and throwback on RISC OS |
| [docs/CROSS-COMPILER.md](docs/CROSS-COMPILER.md) | using the Linux cross compiler |
| [docs/FEATURES.md](docs/FEATURES.md) | everything that is included, with how it was tested |
| [docs/RUNTIME.md](docs/RUNTIME.md) | the runtime packages and the UnixLib fixes |
| [docs/KNOWN-ISSUES.md](docs/KNOWN-ISSUES.md) | what does not work, and workarounds |
| [docs/BUILDING.md](docs/BUILDING.md) | building everything from source |
| [docs/TESTING.md](docs/TESTING.md) | running the tests |
| [docs/OS-MODULES.md](docs/OS-MODULES.md) | the C modules of the RISC OS Open sources built with the kit (new in 16.2.0-16): 38 of 66 build and link (29 before 16.2.0-18), what stops the others, and three of them run on a Raspberry Pi in place of the ROM's, with the same results |
| [docs/MODULES.md](docs/MODULES.md) | modkit: building relocatable modules (experimental): what works, the C library of the kit, what does not yet, and the plan |
| [docs/SHAREDULIB-FIX.md](docs/SHAREDULIB-FIX.md) | the optional fixed SharedUnixLibrary module: what it fixes, how to install it and go back |
| [docs/UPSTREAM.md](docs/UPSTREAM.md) | the bugs found in UnixLib, SharedUnixLibrary, ARMEABISupport and RISC OS, with patches |
| [docs/HISTORY.md](docs/HISTORY.md) | how the port was made |
| [CHANGELOG.md](CHANGELOG.md), [SOURCES.md](SOURCES.md), [LICENSES.md](LICENSES.md) | releases, where everything comes from, licences |

## Credits, licence, and how it was made

This builds on the work of the **GCCSDK** project and its contributors: the GCC 10.2.0 EABI port that this one was forward-ported from, UnixLib, ARMEABISupport (written by Lee Noar), SharedUnixLibrary, the Shared Object Manager and the
autobuilder. The sources it is based on are GCCSDK's svn trunk at revision 7800 (October 2024). It also builds on **GCC**, **binutils** and **GNU make** from the GNU project, and on **RISC OS** itself.

The scripts, tools and documents of this repository are released under the **GNU General Public Licence, version 3 or later**; patches keep the licence of the files they change
(GCC and binutils: GPL-3.0-or-later; UnixLib: mostly the revised BSD licence, some files LGPL; `modkit/include/swisnums.h`: Apache-2.0). The exact terms are in [LICENSES.md](LICENSES.md).

The analysis, the patches and the drafting were done with the help of an AI assistant (Claude); every run on the machine was done by Rob Heaton.
