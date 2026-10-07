# Using the Linux cross compiler

The cross compiler runs on **Linux x86-64** and makes programs for RISC OS (`arm-riscos-gnueabihf`). It is the same GCC 16.2.0 and binutils 2.45.1 as the native compiler, and it produces the same code
(the native compiler's objects are byte-identical to the cross compiler's).

## Install

You need Linux x86-64 with **glibc 2.38 or newer** (Ubuntu 24.04, Debian 13, Fedora 39 or later; check with `ldd --version`). Nothing else: the compilers have no other dependencies. On an older system, [build it from source](BUILDING.md).

```bash
sha256sum -c SHA256SUMS --ignore-missing          # in the folder where you downloaded the files
tar -xf riscos-gcc16-cross-16.2.0-14-x86_64-linux.tar.xz
export PATH=$PWD/riscos-gcc16-cross-16.2.0-14-x86_64-linux/bin:$PATH
arm-riscos-gnueabihf-gcc --version
```

The tree (about 380 MB unpacked) is relocatable: unpack it anywhere, or move it later. To check an unpacked copy, run [`tests/cross-smoke/cross-smoke.sh`](../tests/cross-smoke/cross-smoke.sh) on it
(it compiles and links C, C++, Fortran, LTO and shared-library programs and checks that the compiler finds everything inside the tree).

The programs are `arm-riscos-gnueabihf-gcc`, `-g++`, `-gfortran`, `-cpp`, `-gcov` (with `-gcov-dump` and `-gcov-tool`), and the binutils `-gprof`, `-as`, `-ld`, `-ar`, `-nm`, `-objdump`, `-objcopy`, `-readelf`, `-strip`, `-ranlib`, `-size`, `-strings`, `-addr2line`, `-c++filt` and `-elfedit`.
The UnixLib headers and libraries (the "sysroot") are inside the tree, so no `--sysroot` option is needed. RISC OS libraries such as OSLib are not included.

## Compile

```bash
arm-riscos-gnueabihf-gcc      -O2 -o hello,e1f hello.c          # C        (default -std=gnu23)
arm-riscos-gnueabihf-g++      -O2 -o hello,e1f hello.cc         # C++      (default -std=gnu++20; -std=c++23 for <print>, <expected> ...)
arm-riscos-gnueabihf-gfortran -O2 -o hello,e1f hello.f90        # Fortran
arm-riscos-gnueabihf-gcc -O2 -flto -o prog,e1f a.c b.c          # link time optimisation (uses the linker plugin: archives are optimised too)
arm-riscos-gnueabihf-gcc -O2 -fPIC -shared -o libfoo.so foo.c   # a shared library
arm-riscos-gnueabihf-gcc -O2 -mcpu=cortex-a72 -mfpu=neon-fp-armv8 -mfloat-abi=hard -o hello,e1f hello.c   # tuned for the Cortex-A72
```

* The defaults are ARMv7-A, VFPv3, hard float, and **stack probing on** (`-fstack-clash-protection`, because RISC OS maps the stack one page at a time; see [KNOWN-ISSUES.md](KNOWN-ISSUES.md)).
* `-pthread` is not an option on this target: threads are in UnixLib.
* **The `,e1f` suffix** gives the file the RISC OS file type ELF (&E1F) when it is copied to a RISC OS Samba (or NFS) share. Otherwise copy the file and type `*SetType hello &E1F` on RISC OS.

## What a program needs on the RISC OS machine

Install the runtime packages from the same release with PackMan (see [INSTALL-RISCOS.md](INSTALL-RISCOS.md)). By default C++ and Fortran programs link their runtime libraries **dynamically**:

| Program | Needs on RISC OS | How to make it need only the C runtime |
|---|---|---|
| C | `SharedLibs-C-armeabihf` | (already) |
| C++ | `SharedLibs-C-armeabihf` and `SharedLibs-C++-armeabihf` (libstdc++) | link with `-static-libstdc++` |
| Fortran | `SharedLibs-C-armeabihf` and `SharedLibs-Fortran-armeabihf` (libgfortran) | link with `-static-libgfortran` |

Do not use `-static-libgcc`: the link then fails, because UnixLib refers to libgcc symbols (it works only with `-Wl,--allow-shlib-undefined`).
Check what a program needs with `arm-riscos-gnueabihf-readelf -d prog,e1f | grep NEEDED`.
Use the runtime of the same release (`SharedLibs-C-armeabihf` 16.2.0-13): older ones lack the fixes listed in [RUNTIME.md](RUNTIME.md).

## Coverage and profile-guided optimisation

Build on Linux, run on RISC OS (with the runtime `SharedLibs-C-armeabihf` 16.2.0-12 or later: earlier ones never ran the exit function that writes the counts), read the result on Linux:

```bash
arm-riscos-gnueabihf-gcc -O0 --coverage -c prog.c                 # prog.o and prog.gcno
arm-riscos-gnueabihf-gcc --coverage -o prog,e1f prog.o
```

Copy `prog,e1f` to RISC OS and run it. The program writes `prog.gcda` to the **absolute Linux path** of the object file it was built from, and that path does not exist on RISC OS: tell libgcov where to write instead, with two variables.
`GCOV_PREFIX` is the directory to write in; `GCOV_PREFIX_STRIP` is the number of leading directories of the Linux path that are cut off. For an object built in `/home/me/work` the file is `/home/me/work/prog.gcda`, and cutting off its three directories (`home`, `me`, `work`) leaves `prog.gcda`:

```
*Set GCOV_PREFIX .
*Set GCOV_PREFIX_STRIP 3
*prog
```

writes `prog.gcda` (the RISC OS file `prog/gcda`) in the current directory. Copy the file back and read it with the cross `gcov`, next to `prog.gcno` and `prog.c`:

```bash
arm-riscos-gnueabihf-gcov -b -c prog.gcda       # or: arm-riscos-gnueabihf-gcov prog.c
```

For profile-guided optimisation build with `-fprofile-generate`, run on RISC OS as above on typical input, copy the `.gcda` file to Linux **next to the object file, with the same name**, and build again with `-fprofile-use`:

```bash
arm-riscos-gnueabihf-gcc -O2 -fprofile-generate -c prog.c && arm-riscos-gnueabihf-gcc -fprofile-generate -o prog,e1f prog.o
# ... run on RISC OS, copy prog.gcda back to this directory ...
arm-riscos-gnueabihf-gcc -O2 -fprofile-use -Werror=missing-profile -c prog.c && arm-riscos-gnueabihf-gcc -o prog,e1f prog.o
```

(Do not use `-fprofile-dir`: it makes GCC name the files with `#` characters, which RISC OS does not accept. Compile with `-c` and link in a second step: in one step GCC names the data files after the output and the source, `prog-prog.gcno`.)
## Profiling with gprof

Build with `-pg` on Linux, run on RISC OS (with the runtime `SharedLibs-C-armeabihf` 16.2.0-13 or later: it has the profiler), read the profile on Linux:

```bash
arm-riscos-gnueabihf-gcc -O1 -pg -o prog,e1f prog.c        # -pg on every compile and on the link
# on RISC OS, in the folder of the program:      prog          (it writes gmon.out = the RISC OS file gmon/out when it ends)
arm-riscos-gnueabihf-gprof prog,e1f gmon.out               # the flat profile and the call graph (-b: without the explanations)
```

The program is linked against the `libunixlib.so` that is inside the tool chain (the one of 16.2.0-13, which exports `__gnu_mcount_nc`) and `gcrt0.o` (the start file for `-pg`). The calls are counted exactly, the time is sampled 50 times a second by a thread that UnixLib starts, so give the program some seconds of work; a sample belongs to a function, not to a line; only the program's own code is profiled. The limits are listed in [USING-NATIVE.md](USING-NATIVE.md#profiling-with-gprof) and [KNOWN-ISSUES.md](KNOWN-ISSUES.md). `gmon.out` has the program's addresses (it is not specific to the machine): copy it from the RISC OS Samba share under the name `gmon.out`.
`tests/gprof/` has a test program for it ([README](../tests/gprof/README.md)).

## Throwback from the cross compiler

`-mthrowback` also works for the cross compiler (and, since 16.2.0-13, for the cross assembler and linker, which get `--throwback` from the driver): every error, warning and note that has a file and a line is sent as a syslog (UDP) datagram to the RISC OS machine, where GCCSDK's *SysLogD* module turns it into DDEUtils throwback in your editor.

```bash
export THROWBACK_HOST=riscos-pi               # the RISC OS machine (host name or address)
export THROWBACK_PORT=514                       # optional, 514 is the default
arm-riscos-gnueabihf-gcc -Wall -mthrowback -c main.c
```

Set `THROWBACK_DEBUG` to any value to be told why nothing arrives. This path was tested on the host (66 checks of the text handling and the datagrams for the compilers; the assembler and the linker send datagrams of the same form); it has not been tried against a real SysLogD yet. The native compiler's throwback (through DDEUtils directly) *was* tested on the machine.

## RISC OS modules (`-mmodule`, `cmunge`)

Since 16.2.0-14 the tool chain builds relocatable modules the way GCCSDK 4.7.4 did, but **without a C library** (modkit), and for small, self-contained C modules only for now: [what can and cannot be built](MODULES.md#what-can-be-built-today-and-what-cannot).

```bash
cmunge -tgcc -32bit -p -d header.h -o header.o module.cmhg          # the CMHG file: header, veneers, C header
arm-riscos-gnueabihf-gcc -mmodule -O2 -c main.c -o main.o           # ARMv6, soft float, freestanding, the headers of modkit
arm-riscos-gnueabihf-gcc -mmodule -o MyModule,ffa main.o header.o   # module linker script and libmodkit.a, then modreloc: the flat image that RMLoad takes
```

What it is, what `libmodkit.a` has and the limits: [MODULES.md](MODULES.md). `cmunge`, `modreloc` and `mkoslib` (the OSLib veneers, in place of `-lOSLib32`) are small C programs (no Python); the native compiler on RISC OS has the same ones. The example makefiles are in `modkit/` of this repository.

## Using it from a build system

Point your build at the compilers, for example with make:

```make
CC  = arm-riscos-gnueabihf-gcc
CXX = arm-riscos-gnueabihf-g++
FC  = arm-riscos-gnueabihf-gfortran
AR  = arm-riscos-gnueabihf-ar
```

For autoconf projects `./configure --host=arm-riscos-gnueabihf` is the usual way. Neither that nor CMake or Meson cross files have been tried with this tool chain.

## Differences from GCCSDK's GCC 10.2.0 cross compiler

| | GCCSDK 10.2.0 | this tool chain |
|---|---|---|
| GCC / binutils | 10.2.0 / 2.30 | 16.2.0 / 2.45.1 |
| Default language standard | C17, C++14 | C23 (`gnu23`), C++20 (`gnu++20`) |
| Stack probing | off | **on** (`-fstack-clash-protection`) |
| UnixLib | 5.0 as built by GCCSDK | 5.0 rebuilt with GCC 16 and fixed ([RUNTIME.md](RUNTIME.md)) |
| Throwback | accepted, ignored by the EABI compilers | works |

GCC 16 is much stricter than GCC 10 about old C: for example implicit function declarations are errors by default (use `-std=gnu17` or `-Wno-implicit-function-declaration` to build old code).
Programs built by the GCCSDK 10.2.0 compilers keep working on the same runtime.

## Building the tool chain yourself

[BUILDING.md](BUILDING.md).
