# The GCC compiler on RISC OS 64

**Pre-releases.** Two packages hold the GCC 16.2.0 compiler (C and C++) and the GNU assembler and linker (binutils 2.45.1) as programs **for RISC OS 64**, so that a RISC OS 64 machine can compile on its own:

| Pre-release | Zip | Makes programs for | Unpacked |
|---|---|---|---|
| [`riscos64-native-0.15`](https://github.com/robheaton/riscos-gcc16/releases/tag/riscos64-native-0.15) | 57 MB | **RISC OS 64** (AArch64, file type &FF8) | 169 MB |
| [`riscos64-native32-0.1`](https://github.com/robheaton/riscos-gcc16/releases/tag/riscos64-native32-0.1) | 41 MB | **32-bit RISC OS 5** (ARM, ELF, file type &E1F): run them on a 32-bit machine | 105 MB |

`riscos64-native-0.15` replaces [`riscos64-native-0.13`](https://github.com/robheaton/riscos-gcc16/releases/tag/riscos64-native-0.13) (C only). The cross compiler for Linux ([GETTING-STARTED-64.md](GETTING-STARTED-64.md)) is still the main route to RISC OS 64 programs and modules: it is faster and better tested. These two are for compiling on the target itself, and for making 32-bit programs from a 64-bit machine. The opposite direction, a 32-bit machine that makes 64-bit programs, is [CROSS-FROM-32BIT.md](CROSS-FROM-32BIT.md).

They are previews: they work, they are rough, and they are not PackMan packages.

## What you need

* a RISC OS 64 machine with about 200 MB of free application memory (the compilers are big programs; the scripts give them `WimpSlot -min 128M -max 128M`, and a large C++ file can need 320M) and about 170 MB of disc for the first package (105 MB for the second);
* for `native32`, a 32-bit RISC OS 5 machine with the C runtime of the [Gcc16 package](INSTALL-RISCOS.md) (`SharedLibs-C-armeabihf`) to run what it makes.

## Install and try

Unzip so that the folder `riscos64-native-0.15` (or `riscos64-native32-0.1`) is somewhere on a disc with room for it (use an unzip that understands the `,ff8` file type suffixes in the names). Read its `ReadMe`, then double-click one of the **Run** files:

| Package | Run file | What it does |
|---|---|---|
| `native` | `RunNatCxx` | compiles, links and runs a C++ program (`iostream`, `vector`, `sort`, an exception) |
| `native` | `Run64Zlib` | compiles zlib 1.3.1 (16 files) with the C compiler, links its test program and runs it (about 17 seconds) |
| `native32` | `RunTo32` | compiles and links a C program and a C++ program for 32-bit RISC OS: `out.hello32`, `out.hellopp32` |
| `native32` | `RunZlib32` | compiles zlib 1.3.1 for 32-bit RISC OS and links `zl.out.zexample32` |

Copy the `native32` results to a 32-bit machine to run them.

**The Run files are not Obey files.** A Run file is a copy of a small launcher program (`ulrun`, source: [`riscos64/ports/native/ulrun.c`](../riscos64/ports/native/ulrun.c)) that reads the script of the same name in the folder `scripts` and runs its lines one by one. A program that is started from an Obey file cannot start another program and come back with the Obey command of the system, and the compiler is a driver that starts `cc1`, `as`, `collect2` and `ld` (a known problem of the SDK, not solved). The scripts are plain text and use the part of Obey that the samples need: `| comment`, `Obey NAME` (another script), and any command of the operating system, one to a line. The launcher goes to its own folder first, sets `T64$Dir` to it and `UnixEnv$swap` to 1, and stops at the first command that fails.

## Your own program

1. Copy `RunNatCxx` to `RunMine` (for `native32`, copy `RunTo32`), and `scripts.RunNatCxx` to `scripts.RunMine`.
2. Put the source in the folder `c` (C: `hello.c` is `c.hello`) or `cc` (C++), and change the compiler lines of `scripts.RunMine`.
3. Double-click `RunMine`.

The compile line of the C++ sample, as a model (the `-B`, `-L` and file names are relative to the folder; `Obey Clean` before each compiler line removes the variables that the compiler leaves set):

```
Obey Clean
<T64$Dir>.xg++ -B bin/ -B lib/ -L lib -isystem include/c++/16.2.0 -isystem include/c++/16.2.0/aarch64-riscos -isystem include -O1 -x c++ -c cc/hellopp -o o/hellopp
Obey Clean
<T64$Dir>.xg++ -B bin/ -B lib/ -L lib -munixlib o/hellopp -o out/hellopp
SetType <T64$Dir>.out.hellopp &FF8
```

For C use `xgcc` and `-x c`. For `native32` the lines are in `scripts.RunTo32` (the C++ folder is `include/c++/16.2.0/arm-riscos-gnueabihf`, and there is no `-munixlib`). A command line is limited to 255 characters by the kernel: give long lists of files in a response file, `@file` (see `zl/link`).

## What is in the folder

| Where | What |
|---|---|
| `xgcc`, `xg++` | the compiler drivers (they only start the other programs, so they have a small heap on purpose) |
| `bin/` | `cc1`, `cc1plus`, `as`, `ld`, `collect2`, `objcopy`, `ar` |
| `lib/`, `include/` | the start-up files and libraries (UnixLib, libgcc, libstdc++ for `native`; for `native32` those of the 32-bit [Gcc16 package](INSTALL-RISCOS.md)) and the headers, in the RISC OS layout (`stdio.h` is `h.stdio`) |
| `zl/` | the zlib 1.3.1 sources for the zlib sample |
| `scripts/`, `Run*` | the scripts and their launchers |
| `Licences/` | GPL, LGPL, the GCC Runtime Library Exception, UnixLib's licence and zlib's |

## What is tested

On a RISC OS 64 machine, with the folders unpacked from the zips, 31 October 2026:

* **`native`**: `RunNatCxx`, started by a double-click in the Filer, compiled, linked and ran its C++ program and printed the expected lines; `Run64Zlib` compiled the 16 files of zlib with the C compiler, put them into an archive with `ar`, linked the test program and ran it: all seven checks printed the expected text (the last is `inflate with dictionary: hello, hello!`).
* **`native32`**: `RunTo32` and `RunZlib32` ran to their end. The object files of the C and the C++ sample are byte for byte the object files that the Linux cross compiler (16.2.0-24) makes from the same source. The three programs they linked (a C program, the C++ program and zlib's test program) were run on a 32-bit RISC OS 5 machine (a Raspberry Pi Compute Module 4) and printed the expected text; the zlib test passes all its checks there.
* The launcher: started by a double-click in the Filer, and by name from a command with and without the file system of its folder in the name, and from a different current directory. The other three Run files were started from a command, not from the Filer.

Not tried: large programs other than zlib, Fortran and LTO (not in these packages), the PackMan format.

## Limits

* RISC OS 64 and a big application slot are needed (see above); with the small default slot the compilers crash.
* C and C++, no Fortran, no LTO. A C++ file that includes `iostream` takes seconds to a minute.
* Programs that are killed leave their memory areas behind until the next reboot.
* A link with very many object files can fail on some file systems (too many files open at once): put the objects into an archive first with `bin.ar`, as the zlib samples do.
* `native-0.15` has no `Build` command and no Lua, make or sed examples; `native-0.13` has them (for C).
* `native32`: the programs link the static libstdc++ (780 KB for a C++ hello world), and the 32-bit machine needs the C runtime of the Gcc16 package.

## Building the packages

The programs: [`riscos64/ports/build-native-binutils.sh`](../riscos64/ports/build-native-binutils.sh) (`TARGET=arm-riscos-gnueabihf` for the 32-bit target) and [`build-native-gcc.sh`](../riscos64/ports/build-native-gcc.sh) (`LANGS=c,c++`; the driver and `collect2` are relinked with a small heap), and for the 32-bit target [`riscos64/hosted64/build-hosted64.sh`](../riscos64/hosted64/build-hosted64.sh). The folders: [`ports/make-native-tree.py`](../riscos64/ports/make-native-tree.py) (`--cxx`) and [`hosted64/make-tree32.py`](../riscos64/hosted64/make-tree32.py) lay out the programs, libraries and headers; [`ports/finish-native-package.py`](../riscos64/ports/finish-native-package.py) adds `ar`, zlib, the scripts, the launchers (built from [`ulrun.c`](../riscos64/ports/native/ulrun.c) with `-Wl,--defsym=__ul64_heap_max=0x400000`), the `ReadMe` and the licences, and writes the zip.

Licences: GCC and binutils are GPL-3.0-or-later, libstdc++ and libgcc add the GCC Runtime Library Exception, UnixLib is BSD and LGPL, zlib has its own licence; the texts are in the `Licences` folder of each package and in [LICENSES.md](../LICENSES.md).
