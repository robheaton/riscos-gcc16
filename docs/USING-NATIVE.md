# Using the native compiler on RISC OS

This page assumes the packages are installed ([INSTALL-RISCOS.md](INSTALL-RISCOS.md)). The commands are typed in a **Task window** (Ctrl-F12).

## Every session

1. Double-click `!GCC16` once after each boot: its `!Run` puts `GCC16bin:` (the drivers `gcc`, `g++`, `gfortran`, `cpp`, and `make`, `gcov` and `gprof`) and `GCC16tbin:` (the binutils) on `Run$Path`, sets the return-code limit
   (`Sys$RCLimit`) and sets UnixLib's filename suffix swapping for every tool.
2. Open a Task window and give it room: the C++ compiler is a 30 MB program and the driver that starts it is saved next to it.

   ```
   WimpSlot -min 48M -max 48M
   ```
   (or set the "Next" slot of the Task Manager to 48 MB before you open the window).
3. Go to your work directory: `Dir <your directory>`.

## File names: Unix names, RISC OS directories

The tools use Unix-style names on the command line and UnixLib translates them: a name that ends in one of the usual source extensions lives in a **directory of that name**.

| You type | RISC OS file |
|---|---|
| `hello.c` | `c.hello` |
| `hello.cc`, `hello.cpp`, `hello.cxx` | `cc.hello`, `cpp.hello`, `cxx.hello` |
| `hello.f90` (also `f95`, `f03`, `f08`, `for`, `f`) | `f90.hello` |
| `util.h` | `h.util` |
| `main.o` | `o.main` |
| `hello.s` | `s.hello` |

(The full list is in `UnixEnv$gcc$sfix`: `f for F f90 F90 f95 F95 f03 F03 f08 F08 fpp cc cxx cpp c++ C i ii rpo c m h hh s S xrb xrs l o y tcc cmhg adb ads ali`.)
So source files live in `c`, `cc`, `f90` and `h` directories, objects in `o`, and `#include "util.h"` finds `h.util`. Names with other extensions (`libutil.a`, `Makefile`) and names without one (`hello`) are used as they are.
The linker gives a program the file type **ELF (&E1F)**: run it by typing its name (it needs the `SharedLibs-C-armeabihf` runtime).

**A trap in UnixLib's guess.** For a *relative* name UnixLib has to guess whether it is a Unix name or a RISC OS name, and a name whose middle part is one of those suffixes, such as `prog.c.gcov`, is taken for the RISC OS path `prog.c.gcov`: the file `gcov` in the directory `c` in the directory `prog`. The tools of this package only use Unix names and say so (`gcov` writes `prog.c.gcov` as the file `prog/c/gcov`).
A program of your own that opens such a file must do the same: `#include <unixlib/local.h>` and define `int __riscosify_control = __RISCOSIFY_STRICT_UNIX_SPECS;`, which makes every relative name of that program a Unix name.

## Compiling

```
gcc -O2 -o hello hello.c                     C
g++ -O2 -o hello hello.cc                    C++
gfortran -O2 -o hello hello.f90              Fortran
gcc -O2 -c main.c                            compile only: o.main
gcc -O2 -o app main.o util.o                 link
gcc -S -O2 hello.c                           assembly: s.hello
```

* The defaults are C `-std=gnu23`, C++ `-std=gnu++20`, hard float, ARMv7-A with VFPv3, and **stack probing on** (`-fstack-clash-protection`: it is needed because RISC OS maps the stack a page at a time; turn it off with `-fno-stack-clash-protection` only if you know why).
* For the Cortex-A72 (Raspberry Pi 4 and others): `-mcpu=cortex-a72 -mfpu=neon-fp-armv8 -mfloat-abi=hard`.
* `libstdc++` and `libgfortran` are linked **statically** by the native compiler, so C++ and Fortran programs need only the C runtime.
* `-lm` is accepted (the maths functions are in UnixLib itself). `-pthread` is not an option here: threads are in UnixLib, and `std::thread` works without it.
* Other tools: `ar`, `nm`, `objdump`, `objcopy`, `readelf`, `strip`, `ranlib`, `size`, `strings`, `addr2line`, `c++filt` and `elfedit` are on `Run$Path` after step 1 above.

## make

GNU make 4.4.1 is in `!GCC16.bin`. It never uses a shell: every recipe line is split into words and run directly, so a recipe can call `gcc`, `g++`, `ar` and RISC OS commands
(`Copy`, `Delete`, `Echo`, `Stamp` ...) but has **no pipes, redirections or `&&`**. The default C compiler is `gcc`. Programs are found through `Run$Path`. `-jN` runs one job at a time.

```make
CFLAGS = -O2
OBJS = main.o util.o

app: $(OBJS)
	gcc -o app $(OBJS)

main.o util.o: util.h
```

```
make app
```

(The makefile uses the Unix names `main.c`, `util.h`, `main.o`; the `c`, `h` and `o` directories work as for the compiler.)

## Link time optimisation

```
gcc -O2 -flto -o prog a.c b.c          C, two files, optimised across them
g++ -O2 -flto ...    gfortran -O2 -flto ...
gcc -O2 -flto -c a.c                   or compile each file with -flto and link with -flto
gcc -O2 -flto a.o b.o -o prog
```

The compiler writes its intermediate form into the object files next to the normal code ("fat" objects, which also link without LTO) and the link step compiles the whole program at once, so functions can be inlined across files.
The native linker has no plugin support, so this goes through `collect2` and `lto-wrapper` (the Linux cross compiler uses the linker plugin instead). Consequences:

* objects inside an **archive** (`libfoo.a` made with `ar`) are **not** optimised across modules: give the object files to the link directly;
* the optimisation jobs run one after the other (`-flto=N`, `-flto=auto` and a make job server are accepted and do the same as `-flto`);
* the link needs more memory and time than a plain link (`lto1` is as big as `cc1`).

## Coverage and profile-guided optimisation

```
gcc -O0 --coverage -c prog.c          compile with instrumentation: o.prog, and the notes file prog.gcno
gcc --coverage -o prog prog.o         link (libgcov is added)
prog                                  run it: when it ends it writes, or adds its counts to, prog.gcda
gcov prog.c                           prints the percentage of the lines that ran and writes the annotated source prog.c.gcov
```

`gcov -b prog.c` adds the branches, `-c` the counts instead of percentages, `-f` the functions. Run the program as often as you like (also with different input): every run adds to `prog.gcda`; delete the file to start again.
Compile with `-c` and link in a second step, as above: compiling and linking in one command (`gcc --coverage -o prog prog.c`) makes GCC name the data files after the output *and* the source, `prog-prog.gcno` and `prog-prog.gcda`.
UnixLib turns the names into RISC OS files as usual: `prog.gcno` is the file `prog/gcno`, `prog.gcda` is `prog/gcda` and the annotated source `prog.c.gcov` is `prog/c/gcov` (see the note on file names above).

Profile-guided optimisation is the same idea: build with `-fprofile-generate`, run the program on typical input, then build again with `-fprofile-use`, which reads the profile that the run left next to the object file:

```
gcc -O2 -fprofile-generate -c prog.c
gcc -fprofile-generate -o prog prog.o
prog                                  typical input: writes prog.gcda
gcc -O2 -fprofile-use -Werror=missing-profile -c prog.c       (the error makes sure that the profile was found)
gcc -o prog prog.o
```

* This needs the runtime `SharedLibs-C-armeabihf` 16.2.0-12 or later: the counts are written by an exit function that runs from the program's `.fini_array`, which earlier runtimes never ran. A program that ends with `_exit`, `abort` or a crash writes nothing.
* The program has the absolute name of its `.gcda` file built in (the directory the object file was built in), so the counts go there wherever you run it from; `GCOV_PREFIX` and `GCOV_PREFIX_STRIP` change that, as [CROSS-COMPILER.md](CROSS-COMPILER.md#coverage-and-profile-guided-optimisation) explains.
* Profiling with `-pg` and `gprof` is in the next section.

## Profiling with gprof

```
gcc -O1 -pg -o prog prog.c         compile and link with profiling (g++ and gfortran too; -pg goes on every compile and on the link)
prog                               when it ends it writes gmon.out, the RISC OS file gmon/out, in the current directory
gprof prog gmon.out                the flat profile and the call graph      (-b: without the explanations, -p: only the flat profile, -q: only the call graph)
```

The compiler counts every call exactly; the time is sampled 50 times a second, so a program has to run for some seconds to give a useful profile. What to know:

* **Function level.** A sample belongs to a function, not to a line: on this processor the interrupt is taken at a fixed place of a loop, so every hot loop shows as one bin of the histogram.
* **Only your code.** Time inside UnixLib and libstdc++ is not in the profile, and their calls are not recorded; the time of a call of `malloc` or `printf` is not charged to the caller.
* **Lost ticks.** The samples come from a second thread that UnixLib starts behind `profil ()`, at the ticks of the ticker that its threads use. A tick that falls into a part of UnixLib that must not be interrupted (`malloc`, stdio) is lost, and so is time when the Wimp is running other tasks: expect about 80 percent of the elapsed time of a CPU-bound program (4.7 of 6 seconds in the test).
* **Threads.** A program that has threads of its own is sampled only at every other thread's turn: the profile has the right shape and too little time. The sampler is a thread too: a program that counts its threads will see it.
* The program must end by returning from `main` or by `exit`: `_exit`, `abort` and a crash leave no `gmon.out`. Set `GMON_VERBOSE` (to anything) to have the number of samples printed when it ends.
* Needs the runtime 16.2.0-13 (UnixLib fix level 15). `gmon.out` is `gmon/out`: `gprof prog gmon.out` finds it by the Unix name, as for the other files ([File names](#file-names-unix-names-risc-os-directories)).
* The Linux cross compiler's gprof reads the same file: copy `gmon.out` to Linux and run `arm-riscos-gnueabihf-gprof prog gmon.out` ([CROSS-COMPILER.md](CROSS-COMPILER.md#profiling-with-gprof)).

## Throwback

```
gcc -Wall -mthrowback -c main.c        g++ -mthrowback ...        gfortran -mthrowback ...
```

Every error, warning and note that has a file and a line is also sent to your text editor through the **DDEUtils** module, which lists them in a throwback window; a double click opens the source at the line. The text on the screen does not change, and
neither does the generated code. Put `-mthrowback` in the compiler options of your makefile.

* Needs DDEUtils loaded (the DDE, StrongED and others load it) and an editor that has registered with it (StrongED: tick "Throwback requests" in its Choices). It was tested with **StrongED**.
* An editor can only register with a module that is already there: if DDEUtils was loaded after the editor started, restart the editor.
* Throwback works only in the desktop. Set `THROWBACK_DEBUG` (to anything) to be told on the screen why no throwback arrives.
* The assembler and the linker send theirs too, since 16.2.0-13: `-mthrowback` makes the driver give them `--throwback`, and `as --throwback` and `ld --throwback` work on their own. The assembler reports its errors at the lines of your source; the linker reports an error that has a source file and line in the object's debug information (an undefined reference in a file compiled with `-g`) and nothing for the rest. A file name that starts with a path variable (`<Obey$Dir>.s.foo`) is a file like any other.

## Modules (`-mmodule`, `cmunge`), new in 16.2.0-14

Relocatable modules with the small C library of the module kit (about 285 functions, `stdio` files and floating point among them; 16.2.0-14 had 30; no UnixLib), built on RISC OS ([MODULES.md](MODULES.md) says what a module of this kind is, how it is made and what the limits are):

```
cmunge -tgcc -32bit -p -d header.h -o header.o module.cmhg     the CMHG file (cmhg/header or cmhg.module: any name): the module header and its veneers (o.header) and the C header (h.header)
gcc -c -O2 -mmodule -x c -o main.o c/main                      compile (the source must not need a C library beyond the functions of modkit's headers, listed under "The C library of modkit" in MODULES.md: see also its Limits)
mkoslib -I <OSLib>/oslib -o oslibv.c --from-objects main.o     the SWI veneers of the OSLib functions that main.o uses (replaces -lOSLib32); then   gcc -c -mmodule oslibv.c
gcc -mmodule -o MyModule main.o header.o oslibv.o              the link: the driver runs modreloc after the linker, and the file MyModule is the module (file type &FFA)
RMLoad MyModule
```

* **Name the output without `,ffa`** on RISC OS: a UnixLib program does not take a `,ffa` at the end of a name as a file type unless it asks to, so `-o MyModule,ffa` would make a file whose name has a comma in it. The file type &FFA is set by `modreloc`, which is what the driver's post-link step is for. An output named `x.elf` stays an ELF file (for a debugger).
* **OSLib**: `mkoslib` reads OSLib's C headers (the register layout of every function is in the comment above it): `-I` is the `oslib` folder, with the headers as the compiler finds them (`h.os`, `h.wimp` ... : the OSLib that GCCSDK installs on Linux has `os.h`: copy it as `oslib/h/os`). A module needs the headers that its source includes and what they include (`osf32.h` with `os.h`, for instance). The compile of the generated `oslibv.c` needs the same folder: `-I<the folder that has oslib>`.
* The tools run in a Task window with the same slot as the compiler (`WimpSlot -min 48M -max 48M`). `cmunge -p` starts `gcc` for the preprocessor; `cmunge -o` starts it for the assembler.
* **Save your work before `RMLoad`ing a module that is new**: a mistake in a module can crash the machine (the self-test's checker looks at the header and the relocation table of the module it built before it loads it).
* Tested on the Raspberry Pi: the C library of the kit, modules made with `cmunge` (a module that is run as a program, international help, generic veneers called from SVC code, from user code and in interrupt time) and the `stdio` files, keyboard and screen streams were run there on 2026-10-07, and three modules of the RISC OS Open sources built with the kit gave the same results as the ROM's in everything the test program compares, on 2026-10-08 ([what was proven on hardware](MODULES.md#what-was-proven-on-hardware)). Floating point in modules has not been run on hardware, and C++ in modules does not work yet. With 16.2.0-14, a network module of 800 lines (sockets, files and OS calls through 23 OSLib functions) was built by these commands in 3 seconds and is **byte for byte the module that the Linux cross compiler makes from the same source**; loaded, it passed the network tests (39 checks: banner, commands, files up to 500 KB, a vanishing client, a soak of 300 commands). The same test with 16.2.0-15 (the new kit): 4 seconds, the same file, the same 39 checks. Check 12 of the self-test builds a small module, loads it, runs its command and removes it.
* A makefile written for GCCSDK 4.7.4 needs the compiler names changed, OSLib veneers instead of `-lOSLibH32`, and nothing for the link: see the porting section of [MODULES.md](MODULES.md).

## Memory, stacks and temporary files

* Every tool keeps its heap in a dynamic area. The maximum is only **reserved address space**: 32 MB for the drivers and `make`, 512 MB for `cc1`, `cc1plus`, `f951` and the binutils. `<program>$HeapMax` (an integer, in MB) changes it, e.g. `SetEval cc1plus$HeapMax 1024`.
* The main stacks are built in: `cc1`, `cc1plus` and `f951` get 64 MB (deeply recursive templates and constexpr evaluation need about 4 KB per level), `make` 8 MB, the binutils and the drivers 1 MB (UnixLib's default). They need the runtime package 16.2.0-6 or later.
* Temporary files go to `TMPDIR`, or `UnixFS$/tmp` (`<Wimp$ScrapDir>` by default). **Keep them on a local disc**: with `TMPDIR` on a network share the compiler was about 20% slower.
* RISC OS can limit the maximum size of a dynamic area (128 MB on the test machine), so the compilers get less than the 512 MB they ask for. That is enough for ordinary sources; a compile that ends with `out of memory` or `virtual memory exhausted` has run into it. Split the source file or lower the optimisation level.

## Environment reference

| Variable | Meaning |
|---|---|
| `GCC16$Dir`, `GCC16$Version` | where `!GCC16` is, and its version (set by `!Boot`) |
| `GCC16bin$Path`, `GCC16tbin$Path` | `!GCC16.bin.` (drivers, make) and `!GCC16.arm-riscos-gnueabihf.bin.` (binutils) |
| `Sys$RCLimit` | 65536: a failing compile must not be reported as a RISC OS error |
| `UnixEnv$<tool>$sfix` | the suffix list for the filename translation (above) |
| `TMPDIR` | where temporary files go |
| `<program>$HeapMax` | maximum heap of one program, in MB |
| `THROWBACK_DEBUG` | any value: explain on the screen why throwback does not work |
| `GMON_VERBOSE` | any value: a program built with `-pg` prints how many samples it took when it ends |

## A first multi-file project

`c.main`:
```c
#include <stdio.h>
#include "util.h"
int main (void) { printf ("2 + 3 = %d, 10 squared = %d\n", util_add (2, 3), util_square (10)); return 0; }
```
`c.util`:
```c
#include "util.h"
int util_add (int a, int b) { return a + b; }
int util_square (int a) { return a * a; }
```
`h.util`:
```c
int util_add (int a, int b);
int util_square (int a);
```
```
gcc -O2 -c main.c
gcc -O2 -c util.c
gcc -O2 -o app main.o util.o
app
```
prints `2 + 3 = 5, 10 squared = 100`. To make a static library: `ar rcs libutil.a util.o`, and look inside it with `nm libutil.a`.

## Problems

[INSTALL-RISCOS.md](INSTALL-RISCOS.md#if-something-goes-wrong) lists the common ones; [KNOWN-ISSUES.md](KNOWN-ISSUES.md) lists the limits. To check that the whole installation works, run the self-test in [`tests/selftest`](../tests/selftest/README.md).
