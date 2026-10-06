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
| **binutils 2.45.1** (`as ld ar nm objdump objcopy readelf strip ranlib size strings addr2line c++filt elfedit`) | yes | yes |
| **GNU make 4.4.1** | yes | not needed |
| **Throwback** (`-mthrowback`: errors and warnings in your text editor) | yes, through DDEUtils (tested with StrongED) | yes, through the SysLogD module (tested on the host only) |
| Stack probing on by default (`-fstack-clash-protection`: needed by RISC OS's lazily mapped stacks) | yes | yes |
| Shared libraries, `dlopen`, threads (`pthread`, `std::thread`, `std::async`), thread-local storage | yes | yes |
| Tuning for ARMv7 and Cortex-A72 (`-mcpu=cortex-a72 -mfpu=neon-fp-armv8 -mfloat-abi=hard`) | yes | yes |
| UnixLib 5.0 rebuilt with GCC 16, plus a series of fixes (fix level 14, [list](docs/RUNTIME.md)) | the runtime packages | the runtime packages |
| Experimental: build relocatable **modules** with this tool chain ([modkit](docs/MODULES.md)) | no | yes |

Not included: OpenMP, the sanitizers, `gprof` (`-pg` links and runs but writes nothing), wide-character iostreams (`std::wcout`), `std::stacktrace`, `REAL(16)` in Fortran, a debugger. The full list, with
how each statement was tested, is in [docs/FEATURES.md](docs/FEATURES.md) and [docs/KNOWN-ISSUES.md](docs/KNOWN-ISSUES.md).

## Quick start: compile on RISC OS

1. Download from the [latest release](https://github.com/robheaton/riscos-gcc16/releases/latest) `SharedLibs-C-armeabihf_16.2.0-12_arm.zip` and `Gcc16_16.2.0-12_arm.zip` (69 MB).
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
tar -xf riscos-gcc16-cross-16.2.0-12-x86_64-linux.tar.xz
export PATH=$PWD/riscos-gcc16-cross-16.2.0-12-x86_64-linux/bin:$PATH
arm-riscos-gnueabihf-gcc -O2 -o hello,e1f hello.c        # ,e1f gives the file type ELF when the file is copied over a Samba share
```

Copy `hello,e1f` to the RISC OS machine and run it there; it needs the `SharedLibs-C-armeabihf` package (step 2 above), and C++ and Fortran programs need `SharedLibs-C++-armeabihf` and
`SharedLibs-Fortran-armeabihf` as well, unless you link those libraries statically. See [docs/CROSS-COMPILER.md](docs/CROSS-COMPILER.md). To build the tool chain from source: [docs/BUILDING.md](docs/BUILDING.md).

## Downloads

Everything is attached to the [releases page](https://github.com/robheaton/riscos-gcc16/releases). Check the downloads against `SHA256SUMS`.

| File | What it is | Needed for |
|---|---|---|
| `SharedLibs-C-armeabihf_16.2.0-12_arm.zip` (2 MB) | the C runtime: UnixLib 5.0 rebuilt with GCC 16 and fixed, loader, libgcc_s | running **any** program from this tool chain |
| `Gcc16_16.2.0-12_arm.zip` (69 MB) | the native compilers and tools, as `!GCC16` | compiling on RISC OS |
| `Gcc16SelfTest_16.2.0-12_arm.zip` (13 KB) | the self-test of the native compiler, as `!GCC16Test` | checking an installation |
| `SharedLibs-C++-armeabihf_16.2.0-5_arm.zip` (0.7 MB) | libstdc++ 6.0.36 | running C++ programs that link it dynamically (the cross compiler's default) |
| `SharedLibs-Fortran-armeabihf_16.2.0-2_arm.zip` (0.4 MB) | libgfortran 5 | running Fortran programs that link it dynamically (the cross compiler's default) |
| `riscos-gcc16-cross-16.2.0-12-x86_64-linux.tar.xz` (54 MB) | the cross compiler for Linux | compiling on Linux |
| `gcc-16.2.0.tar.xz`, `binutils-2.45.1.tar.xz`, `make-4.4.1.tar.gz`, `gccsdk-unixlib-r7800.tar.xz` | the unmodified upstream sources the binaries were built from (UnixLib is a snapshot of GCCSDK svn r7800) | the source offer, see [SOURCES.md](SOURCES.md) |

## How it was tested

On a Raspberry Pi Compute Module 4 (Cortex-A72) with RISC OS 5.30, ARMEABISupport 1.08 and Shared Object Manager 3.04. The current release passes all of the following on that machine:

* the C regression suite `rotest` (34,541 checks: integers, 64-bit arithmetic, floating point, conversions, varargs, `alloca`, `setjmp`, unwinding, atomics, PIC data, C23) and the C++ suite `cxxtest` (139 checks), built both
  by the cross compiler and by the native compiler (the native compiler's objects are byte-identical to the cross compiler's);
* the thread tests (`std::thread`, `std::async`, `call_once`, timed waits, `thread_local`), the Fortran suite (122 + 54 + 26 + 30 + 9 checks, plus an error-exit test) and a dynamic-library suite;
* the native compiler building real software: zlib 1.3.1 (also with `-flto`) and GNU make 4.4.1 itself;
* the runtime's own checks: 48 library checks, 23 memory-guard checks, 12 process-exit checks, six heap-growth scenarios of `vfork` + `exec` children, a table of 15,066 `sscanf` cases, and, new in 16.2.0-12, the `.fini_array` (destructors), `getrlimit (RLIMIT_STACK)`, 20 POSIX semaphore checks and the `.gcda` files of coverage and profile programs;
* coverage: programs built by the cross compiler wrote their `.gcda` files on the Pi and the Linux `gcov` read them; the native compiler did the same (`--coverage`), the native `gcov` read the file and wrote the annotated source (80.00% of 15 lines, every line count checked by a small program), and `-fprofile-use` found the profile of an instrumented run (checks 9 and 10 of the self-test);

The packages of this release were installed with PackMan on that machine and checked with the self-test (`tests/selftest`: ten checks, 26 seconds).
The build instructions were checked too: every command of [docs/BUILDING.md](docs/BUILDING.md) was run again, in order, from a fresh copy of this repository in an empty home directory, and the results were compared with the released files.
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
| [docs/MODULES.md](docs/MODULES.md) | modkit: building relocatable modules (experimental) |
| [docs/UPSTREAM.md](docs/UPSTREAM.md) | the bugs found in UnixLib, SharedUnixLibrary, ARMEABISupport and RISC OS, with patches |
| [docs/HISTORY.md](docs/HISTORY.md) | how the port was made |
| [CHANGELOG.md](CHANGELOG.md), [SOURCES.md](SOURCES.md), [LICENSES.md](LICENSES.md) | releases, where everything comes from, licences |

## Credits, licence, and how it was made

This builds on the work of the **GCCSDK** project and its contributors: the GCC 10.2.0 EABI port that this one was forward-ported from, UnixLib, ARMEABISupport (written by Lee Noar), SharedUnixLibrary, the Shared Object Manager and the
autobuilder. The sources it is based on are GCCSDK's svn trunk at revision 7800 (October 2024). It also builds on **GCC**, **binutils** and **GNU make** from the GNU project, and on **RISC OS** itself.

The scripts, tools and documents of this repository are released under the **GNU General Public Licence, version 3 or later**; patches keep the licence of the files they change
(GCC and binutils: GPL-3.0-or-later; UnixLib: mostly the revised BSD licence, some files LGPL). The exact terms are in [LICENSES.md](LICENSES.md).

The analysis, the patches and the drafting were done with the help of an AI assistant (Claude); every run on the machine was done by Rob Heaton.
