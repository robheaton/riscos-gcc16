# riscos-gcc16: GCC 16 for RISC OS

A modern compiler tool chain for RISC OS: **GCC 16.2.0** (C, C++ and Fortran) and **binutils 2.45.1**, for 32-bit ARM RISC OS 5 machines (`arm-riscos-gnueabihf`: EABI, hard float, UnixLib).
It comes in two forms:

* a **native compiler that runs on RISC OS itself**: `gcc`, `g++`, `gfortran`, binutils and GNU `make`, installed with PackMan;
* a **cross compiler for Linux** (x86-64) that produces the same programs.

It is a forward port of the GCCSDK GCC 10.2.0 EABI tool chain, together with a rebuilt and fixed **UnixLib** runtime.

> **Status: experimental.** Everything here has been built and tested by one person on one machine (a Raspberry Pi Compute Module 4, Cortex-A72, RISC OS 5.30).
> It is not part of, and not supported by, the GCCSDK project. Please report problems [here](https://github.com/robheaton/riscos-gcc16/issues), not to the GCCSDK mailing list.
>
> There is also an early preview for **64-bit RISC OS** (AArch64), described [further down](#risc-os-64-aarch64-early-preview); everything else on this page is for 32-bit machines.

**Release lists:** [all 32-bit releases](https://github.com/robheaton/riscos-gcc16/releases?q=v16.2.0&expanded=true) · [all 64-bit (AArch64) preview releases](https://github.com/robheaton/riscos-gcc16/releases?q=%22RISC+OS+64%22&expanded=true). (GitHub shows both kinds on its [releases page](https://github.com/robheaton/riscos-gcc16/releases); these links show them separately.)

## Downloads (32-bit)

The file names below are download links (they always give the file from the [latest release](https://github.com/robheaton/riscos-gcc16/releases/latest); older versions are on the [releases page](https://github.com/robheaton/riscos-gcc16/releases)). Check the downloads against [`SHA256SUMS`](https://github.com/robheaton/riscos-gcc16/releases/latest/download/SHA256SUMS).

| File | Description | Required for |
|---|---|---|
| [`SharedLibs-C-armeabihf_16.2.0-15_arm.zip`](https://github.com/robheaton/riscos-gcc16/releases/latest/download/SharedLibs-C-armeabihf_16.2.0-15_arm.zip) (2 MB) | the C runtime: UnixLib 5.0 rebuilt with GCC 16 and fixed, loader, libgcc_s | running **any** program from this tool chain |
| [`Gcc16_16.2.0-24_arm.zip`](https://github.com/robheaton/riscos-gcc16/releases/latest/download/Gcc16_16.2.0-24_arm.zip) (70 MB) | the native compilers and tools, as `!GCC16` | compiling on RISC OS |
| [`Gcc16SelfTest_16.2.0-24_arm.zip`](https://github.com/robheaton/riscos-gcc16/releases/latest/download/Gcc16SelfTest_16.2.0-24_arm.zip) (19 KB) | the self-test of the native compiler, as `!GCC16Test` | checking an installation |
| [`SharedULibFix_1.16-vforkfix3_arm.zip`](https://github.com/robheaton/riscos-gcc16/releases/latest/download/SharedULibFix_1.16-vforkfix3_arm.zip) (24 KB) | the fixed SharedUnixLibrary 1.16-vforkfix3 with an installer that checks everything, a restore script and a check, as `!SULFix`. **It replaces a system module: [read this first](docs/SHAREDULIB-FIX.md)** | programs that `vfork` children that can fail to `exec` (optional) |
| [`SharedLibs-C++-armeabihf_16.2.0-6_arm.zip`](https://github.com/robheaton/riscos-gcc16/releases/latest/download/SharedLibs-C++-armeabihf_16.2.0-6_arm.zip) (0.7 MB) | libstdc++ 6.0.36 | running C++ programs that link it dynamically (the cross compiler's default) |
| [`SharedLibs-Fortran-armeabihf_16.2.0-2_arm.zip`](https://github.com/robheaton/riscos-gcc16/releases/latest/download/SharedLibs-Fortran-armeabihf_16.2.0-2_arm.zip) (0.4 MB) | libgfortran 5 | running Fortran programs that link it dynamically (the cross compiler's default) |
| [`riscos-gcc16-cross-16.2.0-24-x86_64-linux.tar.xz`](https://github.com/robheaton/riscos-gcc16/releases/latest/download/riscos-gcc16-cross-16.2.0-24-x86_64-linux.tar.xz) (54 MB) | the cross compiler for Linux | compiling on Linux |
| [`gcc-16.2.0.tar.xz`](https://github.com/robheaton/riscos-gcc16/releases/latest/download/gcc-16.2.0.tar.xz), [`binutils-2.45.1.tar.xz`](https://github.com/robheaton/riscos-gcc16/releases/latest/download/binutils-2.45.1.tar.xz), [`make-4.4.1.tar.gz`](https://github.com/robheaton/riscos-gcc16/releases/latest/download/make-4.4.1.tar.gz), [`gccsdk-unixlib-r7800.tar.xz`](https://github.com/robheaton/riscos-gcc16/releases/latest/download/gccsdk-unixlib-r7800.tar.xz) | the unmodified upstream sources the binaries were built from (UnixLib is a snapshot of GCCSDK svn r7800) | the source offer, see [SOURCES.md](SOURCES.md) |

<br>

---

<br>

## Features

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
| UnixLib 5.0 rebuilt with GCC 16, plus a series of fixes (fix level 17, [list](docs/RUNTIME.md)) | the runtime packages | the runtime packages |
| Experimental: build relocatable **modules** the way GCCSDK 4.7.4 did (`gcc -mmodule`, `cmunge`, `mkoslib`), with a C library of about 590 functions of its own (strings, `ctype`, `stdlib`, `time`, `printf`, `sscanf`, `stdio` files, `_swix`, exact floating point, `<math.h>` with the C99 functions, BSD sockets as file descriptors, `stat`, `dup`, `iconv`, wide characters, `<complex.h>` ...; 4.7.4's module library has 439 public names, the kit 423 of them) and C++ (constructors, `new`, templates, `std::string`, `std::vector`, `std::map` ...; no exceptions, RTTI, streams or threads). **Run on the Pi:** Sunfish (an NFS client filing system of 298 KB: it mounts an export and reads a file), an 800-line network module (built on the Pi, byte for byte the Linux build), three modules of the RISC OS Open sources in place of the ROM's (545 checks, all results equal). **Built, not run:** GCCSDK's own SOManager, ARMEABISupport, SysLogD and four examples from their own Makefiles; 38 of the 66 C modules of the RISC OS Open sources. `cmunge` accepts every CMHG directive of CMunge 0.76 except `library-enter-code`, `library-initialisation-code`, `help:` and `simple-app:`. **Needs an ARMv6 or newer CPU (never run on one) and is soft float only; modules that use much of the C library are bigger than 4.7.4's (the library is inside the module). Stay with GCCSDK 4.7.4 for older CPUs, hardware floating point, C++ exceptions, RTTI or iostreams in a module, the smallest possible module, or Shared C Library applications.** [Why move, who should not, what to change](docs/PORTING-FROM-4.7.4.md); [what can and cannot be built](docs/MODULES.md#what-can-be-built-today-and-what-cannot) | yes (C modules; C++ modules are built on Linux) | yes |
| Optional: **OSLib** (`libOSLib32.a`, `libOSLibSupport32.a` and the 333 headers), built from OSLib's own sources by `install-oslib.py` from your svn checkout (GPL: not shipped); usable in programs and in modules ([docs/OSLIB.md](docs/OSLIB.md)) | the recipe | the recipe |
| Experiment: **NetSurf** (the development version, RISC OS front end) and its libraries, built by `build-netsurf.py` (sources downloaded by the script, not shipped); it ran on the Pi ([docs/NETSURF.md](docs/NETSURF.md)) | the recipe | the recipe |
| Optional: **RUfl** (`librufl.a`, NetSurf's Unicode font library with font substitution), built by `install-rufl.py` from a clone of `librufl` (MIT: not shipped); run on the Pi ([docs/RUFL.md](docs/RUFL.md)) | the recipe | the recipe |
| Optional: **FlexLib** and **SFLib** (`libFlexLib32.a`, `libSFLib32.a`), built by `install-flexlib.py` and `install-sflib.py` from checkouts of Stephen Fryatt's repositories (Apache 2.0 and EUPL 1.2: not shipped); run on the Pi in both of flex's memory modes ([docs/FLEXLIB.md](docs/FLEXLIB.md)) | the recipe | the recipe |
| Optional: **DeskLib** (`libDesk.a` for UnixLib programs and `libDesk-sclmodule.a` for modules), built by `install-desklib.py` from a copy of [DeskLib32](https://github.com/adyoull/desklib32) that you fetch yourself (its licence allows only unaltered copies, so it is not shipped); the cross compiler reads `#include "DeskLib:Core.h"` ([docs/DESKLIB.md](docs/DESKLIB.md)) | the recipe | the recipe |
| Optional: a **fixed SharedUnixLibrary** module, for programs whose `vfork` children can fail to `exec` (it replaces a system module: [read this first](docs/SHAREDULIB-FIX.md)) | the package `SharedULibFix` | not needed |

Not included: OpenMP, the sanitizers, wide-character iostreams (`std::wcout`), `std::stacktrace`, `REAL(16)` in Fortran, a debugger, applications for the Shared C Library (`-mlibscl`, AIF). The full list, with
how each statement was tested, is in [docs/FEATURES.md](docs/FEATURES.md) and [docs/KNOWN-ISSUES.md](docs/KNOWN-ISSUES.md).

## Quick start: compile on RISC OS

1. Download the first two files of the [32-bit table at the top](#downloads-32-bit): `SharedLibs-C-armeabihf_16.2.0-15_arm.zip` and `Gcc16_16.2.0-24_arm.zip` (70 MB).
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
tar -xf riscos-gcc16-cross-16.2.0-24-x86_64-linux.tar.xz
export PATH=$PWD/riscos-gcc16-cross-16.2.0-24-x86_64-linux/bin:$PATH
arm-riscos-gnueabihf-gcc -O2 -o hello,e1f hello.c        # ,e1f gives the file type ELF when the file is copied over a Samba share
```

Copy `hello,e1f` to the RISC OS machine and run it there; it needs the `SharedLibs-C-armeabihf` package (step 2 above), and C++ and Fortran programs need `SharedLibs-C++-armeabihf` and
`SharedLibs-Fortran-armeabihf` as well, unless you link those libraries statically. See [docs/CROSS-COMPILER.md](docs/CROSS-COMPILER.md). To build the tool chain from source: [docs/BUILDING.md](docs/BUILDING.md).

<br>

---

<br>

## RISC OS 64 (AArch64): early preview

> [!NOTE]
> Everything above is for today's 32-bit RISC OS 5 machines, and that is what almost everyone should use. **If you are on a 32-bit machine you can skip this section.**

There is also a separate, early preview of the same idea for **64-bit RISC OS (AArch64)**. It is a cross compiler for Linux only (there is no native compiler on RISC OS 64 yet), and it has been tested only on the RISC OS Pyromaniac emulator, **not yet on a real RISC OS 64 machine**, so please treat it as a first look. What you can do with it today:

* **Compile C and C++** on Linux with `aarch64-riscos-gcc` (C++ includes exceptions, threads and the standard library) and run the programs on RISC OS 64. They use a 64-bit port of UnixLib, so ordinary Unix-style code (files, `printf`, `malloc`, threads, signals, `getopt`, ...) builds as it does on the 32-bit side.
* **Write modules** in C or C++ for RISC OS 64 (`-mmodule`, with a CMHG-compatible `cmunge64`).
* **Use a command-line toolbox**, built with the same compiler: a shell (`sh`), GNU `sed`, `grep`, `awk`, `make`, `diff`, `find`, `tar`, `gzip`, `xz` and `zstd`, `zip`/`unzip`, `less`, `jq`, `m4` and more (the `!GNU` application), and the GNU coreutils (`!CoreUtils`, 102 programs, laid out like the 32-bit package).
* **Link against ready-made libraries**: zlib, libpng, libjpeg-turbo, expat and PCRE2.

What it cannot do yet: there is no `fork`, so programs that start several processes at once, and `bash`, do not work (the included `sh` covers most scripts); sockets are written but untried; and the modules cannot yet intercept vectors or be Wimp tasks.

**To try it** (Linux x86-64 with glibc 2.38 or newer, the same as for the 32-bit cross compiler):

```bash
tar -xf aarch64-riscos-gcc-16.2.0-0.5-linux-x86_64.tar.xz
export PATH=$PWD/aarch64-riscos-gcc-16.2.0-0.5-linux-x86_64/bin:$PATH
aarch64-riscos-gcc -munixlib -O2 -o hello,ff8 hello.c      # ,ff8 is the RISC OS file type of a 64-bit program
```

Copy `hello,ff8` to a RISC OS 64 machine and run it there, or run it on the [RISC OS Pyromaniac](https://github.com/gerph/pyromaniac) emulator (its hosted [build service](https://build.riscos.online/) needs nothing installed).

| Download (all pre-releases) | Size | Description |
|---|---|---|
| [`riscos64-toolchain-0.5`](https://github.com/robheaton/riscos-gcc16/releases/tag/riscos64-toolchain-0.5) | 60 MB | the Linux cross compiler (x86-64) |
| [`riscos64-gnu-tools-6`](https://github.com/robheaton/riscos-gcc16/releases/tag/riscos64-gnu-tools-6) | 11 MB | `!GNU`: the shell and the tools |
| [`riscos64-coreutils-9.5-3`](https://github.com/robheaton/riscos-gcc16/releases/tag/riscos64-coreutils-9.5-3) | 22 MB | `!CoreUtils`: `ls`, `cp`, `sort`, ... |
| [`riscos64-libs-1`](https://github.com/robheaton/riscos-gcc16/releases/tag/riscos64-libs-1) | 0.7 MB | the libraries, for the cross compiler |

These are marked as pre-releases, so GitHub's "Latest" release stays the 32-bit one; the 64-bit releases have names that start `riscos64-`: [list of all 64-bit releases](https://github.com/robheaton/riscos-gcc16/releases?q=%22RISC+OS+64%22&expanded=true).

Start with [docs/AARCH64.md](docs/AARCH64.md) (the compiler) and [docs/USERLAND64.md](docs/USERLAND64.md) (the shell and tools); [docs/RISCOS64.md](docs/RISCOS64.md) says what was tested.

<br>

---

<br>

## Documentation

The documents cover both the **32-bit** tool chain (the main part of this project) and the early **64-bit (AArch64)** preview; the tables below keep them apart.

### 32-bit and general

| | |
|---|---|
| [docs/INSTALL-RISCOS.md](docs/INSTALL-RISCOS.md) | requirements, installing, checking, upgrading and removing the packages |
| [docs/USING-NATIVE.md](docs/USING-NATIVE.md) | using `gcc`, `g++`, `gfortran`, `make`, `-flto` and throwback on RISC OS |
| [docs/CROSS-COMPILER.md](docs/CROSS-COMPILER.md) | using the Linux cross compiler |
| [docs/FEATURES.md](docs/FEATURES.md) | everything that is included, with how it was tested |
| [docs/RUNTIME.md](docs/RUNTIME.md) | the runtime packages and the UnixLib fixes |
| [docs/KNOWN-ISSUES.md](docs/KNOWN-ISSUES.md) | what does not work, and workarounds |
| [docs/HOW-IT-WAS-TESTED.md](docs/HOW-IT-WAS-TESTED.md) | how the 32-bit tool chain was tested: the machine, the suites and the real programs that were built |
| [docs/BUILDING.md](docs/BUILDING.md) | building everything from source |
| [docs/TESTING.md](docs/TESTING.md) | running the tests |
| [docs/OS-MODULES.md](docs/OS-MODULES.md) | the C modules of the RISC OS Open sources built with the kit (new in 16.2.0-16): 38 of 66 build and link (29 before 16.2.0-18), what stops the others, and three of them run on a Raspberry Pi in place of the ROM's, with the same results |
| [docs/MODULES.md](docs/MODULES.md) | modkit: building relocatable modules (experimental): what works, the C library of the kit, what does not yet, and the plan |
| [docs/PORTING-FROM-4.7.4.md](docs/PORTING-FROM-4.7.4.md) | moving a module from GCCSDK 4.7.4: why, who should stay with it, what works as it is, what to change, what behaves differently |
| [docs/SHAREDULIB-FIX.md](docs/SHAREDULIB-FIX.md) | the optional fixed SharedUnixLibrary module: what it fixes, how to install it and go back |
| [docs/UPSTREAM.md](docs/UPSTREAM.md) | the bugs found in UnixLib, SharedUnixLibrary, ARMEABISupport and RISC OS, with patches |
| [docs/HISTORY.md](docs/HISTORY.md) | how the port was made |
| [CHANGELOG.md](CHANGELOG.md), [SOURCES.md](SOURCES.md), [LICENSES.md](LICENSES.md) | releases, where everything comes from, licences |

### 64-bit (AArch64) preview

| | |
|---|---|
| [docs/RISCOS64.md](docs/RISCOS64.md) | RISC OS 64 (AArch64) on Pyromaniac: the tests, the module kit, the SDK, the library test |
| [docs/AARCH64.md](docs/AARCH64.md) | the `aarch64-riscos` tool chain for RISC OS 64: build it, programs, C++, `-munixlib`, modules, what is tested and what is not |
| [docs/MODULES64.md](docs/MODULES64.md) | writing modules for RISC OS 64 in C and C++: `aarch64-riscos-gcc -mmodule`, `cmunge64`, the C library, floating point, what is not supported |
| [docs/UNIXLIB64.md](docs/UNIXLIB64.md) | a C library for programs on RISC OS 64: what UnixLib is (measured), three ways to port it, a component plan with hours, risks |
| [docs/USERLAND64.md](docs/USERLAND64.md) | the shell and the GNU tools for RISC OS 64 (`!GNU`, `!CoreUtils`): setup, file names, the process model, the `sh` reference, tool notes, limits, a porting guide |
| [docs/PORTS-TESTS.md](docs/PORTS-TESTS.md) | the tests that come with the ports (libpng, zlib, expat, jq, PCRE2, xz, libjpeg-turbo ...) run on the emulator: what passed and what was found |

<br>

---

<br>

## Credits, licence, and how it was made

This builds on the work of the **GCCSDK** project and its contributors: the GCC 10.2.0 EABI port that this one was forward-ported from, UnixLib, ARMEABISupport (written by Lee Noar), SharedUnixLibrary, the Shared Object Manager and the
autobuilder. The sources it is based on are GCCSDK's svn trunk at revision 7800 (October 2024). It also builds on **GCC**, **binutils** and **GNU make** from the GNU project, and on **RISC OS** itself.

The **thread ticker** of the runtime (16.2.0-15 and later, UnixLib fix level 17) is the work of **Andrew Youll**, from his [riscos-unixlib](https://github.com/adyoull/riscos-unixlib), an unofficial fork of GCCSDK's UnixLib; it fixed programs that made other tasks abort.

The 64-bit preview could not have been tested without **Gerph**'s **[RISC OS Pyromaniac](https://github.com/gerph/pyromaniac)**, the RISC OS emulator on which every 64-bit program and module here was run, and the hosted [RISC OS Build service](https://build.riscos.online/), which runs it for anyone. Many thanks.

The scripts, tools and documents of this repository are released under the **GNU General Public Licence, version 3 or later**; patches keep the licence of the files they change
(GCC and binutils: GPL-3.0-or-later; UnixLib: mostly the revised BSD licence, some files LGPL; `modkit/include/swisnums.h`: Apache-2.0). The exact terms are in [LICENSES.md](LICENSES.md).
