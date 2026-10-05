# Using the Linux cross compiler

The cross compiler runs on **Linux x86-64** and makes programs for RISC OS (`arm-riscos-gnueabihf`). It is the same GCC 16.2.0 and binutils 2.45.1 as the native compiler, and it produces the same code
(the native compiler's objects are byte-identical to the cross compiler's).

## Install

You need Linux x86-64 with **glibc 2.38 or newer** (Ubuntu 24.04, Debian 13, Fedora 39 or later; check with `ldd --version`). Nothing else: the compilers have no other dependencies. On an older system, [build it from source](BUILDING.md).

```bash
sha256sum -c SHA256SUMS --ignore-missing          # in the folder where you downloaded the files
tar -xf riscos-gcc16-cross-16.2.0-11-x86_64-linux.tar.xz
export PATH=$PWD/riscos-gcc16-cross-16.2.0-11-x86_64-linux/bin:$PATH
arm-riscos-gnueabihf-gcc --version
```

The tree (about 380 MB unpacked) is relocatable: unpack it anywhere, or move it later. To check an unpacked copy, run [`tests/cross-smoke/cross-smoke.sh`](../tests/cross-smoke/cross-smoke.sh) on it
(it compiles and links C, C++, Fortran, LTO and shared-library programs and checks that the compiler finds everything inside the tree).

The programs are `arm-riscos-gnueabihf-gcc`, `-g++`, `-gfortran`, `-cpp`, and the binutils `-as`, `-ld`, `-ar`, `-nm`, `-objdump`, `-objcopy`, `-readelf`, `-strip`, `-ranlib`, `-size`, `-strings`, `-addr2line`, `-c++filt` and `-elfedit`.
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
Use the runtime of the same release (`SharedLibs-C-armeabihf` 16.2.0-11): older ones lack the fixes listed in [RUNTIME.md](RUNTIME.md).

## Throwback from the cross compiler

`-mthrowback` also works for the cross compiler: every error, warning and note that has a file and a line is sent as a syslog (UDP) datagram to the RISC OS machine, where GCCSDK's *SysLogD* module turns it into DDEUtils throwback in your editor.

```bash
export THROWBACK_HOST=riscos-pi               # the RISC OS machine (host name or address)
export THROWBACK_PORT=514                       # optional, 514 is the default
arm-riscos-gnueabihf-gcc -Wall -mthrowback -c main.c
```

Set `THROWBACK_DEBUG` to any value to be told why nothing arrives. This path was tested on the host (66 checks of the text handling and the datagrams); it has not been tried against a real SysLogD yet. The native compiler's throwback (through DDEUtils directly) *was* tested on the machine.

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
