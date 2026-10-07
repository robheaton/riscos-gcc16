# Changelog

## Unreleased (after 16.2.0-15): modkit built against the C modules of the RISC OS Open sources

In the `modkit/` sources and the tools of this repository; not in a release (the packages of 16.2.0-15 do not have it). The C modules of the RISC OS Open sources were built with the kit as the OS build builds them ([docs/OS-MODULES.md](docs/OS-MODULES.md)): **28 of the 66 build and link into a module image without a change to their sources, and 41 compile completely**. What that found, and what changed:

* **`cmunge`**: numbers after the C preprocessor are constant expressions (the OS's headers write `Service_X` as `(0x60)`); the generated header has what CMunge's has and OS sources use (`<prefix>_00`, the `X` SWI names, `error_BAD_SWI`, `arg_CONFIGURE_SYNTAX`, `arg_STATUS`, `configure_*`); **`event-handler:`** (ENTRY[/HANDLER] [numbers]); a handler name followed by something (`(flags-capable:)`) is an error, not a bad symbol; `library-enter-code` and `library-initialisation-code` are refused with the reason. 62 of the 66 CMHG files of the OS are accepted (58 with 16.2.0-15).
* **The C library**: `<kernel.h>` has `size_t`, `_kernel_irqs_on`, `_kernel_irqs_off`, `_kernel_irqs_disabled`, `_kernel_processor_mode`, `_kernel_RMAalloc`, `_kernel_RMAextend`, `_kernel_RMAfree`; `<swis.h>` has the **SWI numbers** (951 SWIs and their `X` names: `include/swisnums.h`, made by `bin/mkswis.py` from the OS's own assembler headers), `XOS_Bit`, `_vswi` and `_vswix`; new `<inttypes.h>` and `<signal.h>` (`signal`, `raise`); `__errno` is the BSD name of `errno` (the TCP/IP libraries' veneers write to it); the functions of `<string.h>` and `<ctype.h>` that are not ISO C are declared only when the program is not strict ISO C (`-std=c99`), so that a module that defines its own `stricmp` compiles. The library is 85,000 bytes (81,000 in 16.2.0-15).
* **`module.ld`** defines `Image$$RO$$Base` and the other symbols of the Norcroft linker that assembler sources import.
* **`modreloc` refuses a module that has a loaded section outside `.image`.** A section of another name (an assembler `AREA`, a section attribute) was left out of the module without a word, and the module jumped into its relocation table: found when DHCP, which called `socket`, did that. The message says how to rename the section.
* **`mkoslib`**: "the processor status register" outputs (34 more OSLib functions), static libraries in `--from-objects`, and no veneer for a function that one of the module's own objects defines.
* **Tests**: `libtest` has sections for `<inttypes.h>` (against glibc), `signal`, the new `_kernel_*` functions (RMA blocks, the interrupt functions and the processor mode, on the interpreter, which learned `CPSIE` / `CPSID`) and `_vswi`; `sim-veneers.py` runs event handlers (events in the list, events not in it, claim and pass on); `test-ctools.py` compares the C and Python tools on expressions, `event-handler` and the new refusals (2978 comparisons instead of 2956, no difference; they now also compile small programs against `<string.h>` and `<ctype.h>` with `-std=c99`, `-std=gnu99` and `-D_GNU_SOURCE`); new `tools/build-os-modules.py`, `modkit/tests/sim-osmodules.py` and `tools/modinfo.py`.

## v16.2.0-15: 2026-10-07, modkit: a C library for modules, `stdio` files and most of CMHG

| Asset | Version |
|---|---|
| native compiler `Gcc16` | 16.2.0-15 |
| runtime `SharedLibs-C-armeabihf` | 16.2.0-13 (unchanged) |
| runtime `SharedLibs-C++-armeabihf` | 16.2.0-5 (unchanged) |
| runtime `SharedLibs-Fortran-armeabihf` | 16.2.0-2 (unchanged) |
| self-test `Gcc16SelfTest` | 16.2.0-15 |
| Linux cross compiler | 16.2.0-15 |
| optional `SharedULibFix` (the fixed SharedUnixLibrary) | 1.16-vforkfix3 (unchanged) |

New since 16.2.0-14, all of it in `modkit` (the module kit that `gcc -mmodule` and `cmunge` use). The native `Gcc16` and the Linux cross compiler carry the new kit (`install-modkit.sh`); the compiler (GCC 16.2.0), binutils 2.45.1, UnixLib and the three runtime packages are not changed.

* **A C library for modules**, about 165 functions instead of the 30 of 16.2.0-14, one object per group of functions: `ctype`, `getenv`, `sscanf`, `strtok`, `strdup`, `rand`, `time` and `clock`, `mktime`, `gmtime`, `strftime`, `atexit`, `qsort`, `bsearch`, the rest of `string.h` and `stdlib.h`, `setjmp`, `locale`, `limits`, `stdint`, `assert`, `_swi` / `_swix` with the Acorn flag layout and the Shared C Library's `_kernel_*` calls ([the list](docs/MODULES.md#the-c-library-of-modkit)). `printf` now converts the integers as C99 does (`%lld`, `%#x`, `% d`, precisions, `hh h l ll j z t`; `%lld` printed a wrong value before). `libmodkit.a` is a linker script that names the library and `libgcc.a`: the soft-float and 64 bit helpers need no `-lgcc`. The library is tested against glibc: about 114,000 compared results, equal on the host (AddressSanitizer, UBSanitizer) and on the interpreter running the ARM code.
* **`stdio` files and streams**: `fopen`, `freopen`, `fclose`, `fflush`, `fread`, `fwrite`, `fgetc`, `fgets`, `fputc`, `fputs`, `ungetc`, `fseek`, `ftell`, `fgetpos`, `fsetpos`, `rewind`, `setvbuf`, `remove`, `rename`, `fprintf`, `fscanf`, `scanf` and the rest of the standard set on `OS_Find`, `OS_GBPB`, `OS_Args`, `OS_File` and `OS_FSControl`, with `stdin` (a line at a time with `OS_ReadLine`), `stdout` and `stderr` (the screen); `printf` has `%n` now. With them all 66 C modules of the RISC OS Open sources use only functions that the kit has. **Rules that a program can see** ([the list](docs/MODULES.md#stdio-files-and-streams)): files of 2 GB or more are not opened; a file that is open for writing can have no other stream, and is not deleted or renamed; a read only or locked file is not opened for writing; `rename` does not replace; `errno` is mapped from the OS error, which stays in `_kernel_last_oserror ()`; `exit` and the return from `main` of a runnable module flush and close the files, a module that is killed does not (`__modlib_closeall ()`); files made by `fopen` have the type Data. A review of the first version (46 findings, all taken) changed it before release: the end of a program closes the files (a runnable module that did not `fclose` kept the handle open and lost its output), `ungetc` of another character no longer overwrites the buffered bytes, `0x` that no hex digit follows is used up by `fscanf` as in glibc, `%.Ns` read one byte too many, `putchar` takes an unsigned char, `printf` and `puts` follow `freopen (name, mode, stdout)`. **Run on the Pi (2026-10-07, NVMe)**: the stdio section (21,068 results) gives glibc's hash on the real file system, and so do the other eight compared sections; read only and locked files, a file open twice, a folder, a file left open at the end of a program (run after run) and the keyboard behave as documented. A rename onto a file that is there is checked by FileSwitch before FileCore looks at the source (the test and the model were wrong, the library was right).
* **`cmunge`**: `international-help-file:` and the command options `international:`, `add-syntax:`, `configure:`, `status:` and `fs-command:`; **`module-is-runnable:` is a real start entry** (`*RMRun Module a "b c" d` calls `main` in user mode; `exit`, `atexit` and a return code work, and in SVC mode `exit` is an error); a SWI prefix that differs from the title; service numbers that are no ARM immediate; inline comments, continuation lines, `-apcs` flags that do not matter (accepted), quoted names; the temporary files of `-p` are removed when `cmunge` stops on an error (they were left in the working folder). Of the 66 C modules of the RISC OS Open sources, **58 have a CMHG file that `cmunge` accepts** (27 with 16.2.0-14), and every file that both `cmunge` and the real CMunge accept gives the same module header. Still refused: `event-handler`, `library-enter-code`, `library-initialisation-code`, `help:` and handler options.
* **Changes in behaviour that can affect a CMHG file or a module built with 16.2.0-14**: a CMHG file that says `module-is-runnable:` now gets a real start entry (16.2.0-14 ignored the line with a warning, so the module's start word was 0; a module with no `main` ends with 0 when it is run, `*RMRun` and `*Run` call it); a command without `max-args:` takes no arguments (CMunge's default; it was 255); `min-args:`, `max-args:` and `gstrans-map:` above 255 are refused (they were cut to 255); a SWI chunk must be a non-zero multiple of 64 without bit 17, a `swi-handler-code:` needs a `swi-chunk-base-number:` and the other way round, and a chunk without a `swi-decoding-table:` takes the title as its prefix (CMunge does the same); the handler of a `generic-veneers:` entry returns a `_kernel_oserror *` and does not claim a vector (0: the registers the handler left in the block and the flags come back as they were; else V set, r0 = the error); `finalisation-code:` is optional; the help line, `Module_Help`, `Module_VersionString` and `Module_VersionNumber` are made from the `help-string:` as CMunge makes them, and a help string that CMunge refuses is refused; `time_t` is a 32 bit `long` (as in the Shared C Library); `module.mk` builds the kit's library from the kit's own sources.
* **`libmodkit` fixes found by the new tests**: `malloc` of more than 2 GB wrapped round to a small block (it is `ENOMEM` now); `exit` in a module that had run as a program ended the desktop when a command of that module called it later (it is an error again); an `atexit` function of an aborted program ran in the next run.
* **A figure in the repository's documents after 16.2.0-14 was wrong**: it said that 26 of the 66 C modules of the RISC OS Open sources use nothing that the 30 functions lack. That did not count the RISC OS library calls (`_swix` and the `_kernel_*` functions; 51 of the 66 call `_swix`); the right figure was 4. With the new library it was 51; with the `stdio` files it is 66, and the 58 that `cmunge` accepts pass both the library and the CMHG test (`tools/scan-os-modules.py`).
* **Tests** (`modkit/tests`): `libtest` (the library against glibc), `sim-veneers.py` (generic veneers in SVC, IRQ and USER mode), `sim-runnable.py`, `sim-mk32.py` (international help, `add-syntax`, SWI tables, a module run as a program), `os-cmhg-corpus.py` and `cmhgdiff.py` (the CMHG files of the OS against the real CMunge), and a larger `test-ctools.py` (2956 checks, with the files that the real CMunge also accepts). The interpreter (`tools/a32.py`) ignores a write to the mode bits of the CPSR in USER mode and reports the real mode in `mrs`, as the processor does. **Run on the Pi (2026-10-07)**: the library test (the 8 compared sections equal to glibc's, the heap, `setjmp`, `getenv`, the real SWIs: `TOTAL fail=0`) and `module-is-runnable` (`*RMRun` with arguments, the exit codes). Found there: the help and syntax values of an international Messages file must end with a NUL before the line end (`*Help` printed the rest of the file otherwise; documented in MODULES.md). A second run the same day passed international help (with the NULs), `add-syntax:`, the module's own SWIs with a prefix that differs from the title, the generic veneer in SVC mode, USER mode and in interrupt time (`OS_CallEvery`), `exit` in a command, and 15 checks of a self test in SVC and in USER mode. **Not run on hardware**: floating point in modules (C++ in modules does not work yet). The native build of a module with the new kit is the module check of the self-test of this release (the last item says what was checked with the packages).
* Not changed: the compiler, binutils, UnixLib, the C, C++ and Fortran runtimes and `SharedULibFix`; `Gcc16` 16.2.0-15 needs `SharedLibs-C-armeabihf` 16.2.0-13 or later, as 16.2.0-14 did.
* Checked on the Raspberry Pi with the packages of this release, installed with PackMan: the self-test (twelve of twelve checks in 32 seconds; its module check builds, loads and runs a small module with the new kit, natively), the full regression run (all 54 summary lines identical to 16.2.0-14, none failing), and the network module built on the Pi by the commands of its GCCSDK 4.7.4 makefile (4 seconds; byte for byte the module that the cross compiler makes; loaded and run through 39 network checks). The build instructions ([docs/BUILDING.md](docs/BUILDING.md)) were run again in an empty home directory, and `Gcc16`, `Gcc16SelfTest` and the Linux tarball of this release are the output of that run.

## v16.2.0-14: 2026-10-06, `gcc -mmodule` and `cmunge`: modules the GCCSDK 4.7.4 way

| Asset | Version |
|---|---|
| native compiler `Gcc16` | 16.2.0-14 |
| runtime `SharedLibs-C-armeabihf` | 16.2.0-13 (unchanged) |
| runtime `SharedLibs-C++-armeabihf` | 16.2.0-5 (unchanged) |
| runtime `SharedLibs-Fortran-armeabihf` | 16.2.0-2 (unchanged) |
| self-test `Gcc16SelfTest` | 16.2.0-14 |
| Linux cross compiler | 16.2.0-14 |
| optional `SharedULibFix` (the fixed SharedUnixLibrary) | 1.16-vforkfix3 (new) |

New since 16.2.0-13:

* **`-mmodule`**: the option that GCCSDK 4.7.4 used for relocatable modules works: ARMv6, soft float, ARM state, freestanding, no PIC, the headers of modkit instead of UnixLib's, `__TARGET_MODULE__` defined; the linker gets the module linker script and `libmodkit.a`, and no start files. **`gcc -mmodule -o Module,ffa main.o header.o` writes the module**: after the linker the driver runs `modreloc`, which replaces the ELF file by the flat image (an output named `*.elf`, or a partial link with `-r`, stays what it is).
* **`cmunge`** in `bin/`: CMunge's command line (`-tgcc -32bit -p -D -U -I -d -o -s`) over modkit's header generator; options that do not apply are refused, not ignored.
* **Modules can be built on RISC OS**: the three module tools (`cmunge`, `mkoslib`, `modreloc`) are C programs now (no Python; `modkit/src`, the same source for Linux and RISC OS), and the native `Gcc16` has them with `libmodkit.a`, the linker script and the headers: `gcc -mmodule` works on the Pi as on Linux. The C tools make the same files as the Python versions they replace, byte for byte (`modkit/tests/test-ctools.py`: 2812 comparisons).
* **`mkoslib --from-objects`** writes the SWI veneers of the OSLib functions that your objects use: the replacement for `-lOSLib32` (GCCSDK's archive is for the old ABI). `module.mk` has it all as make rules, needs no list of functions and no RISC OS sources (set `RISCOS_SOURCES` to have the one SWI number of the header read from them).
* The tool chain installs modkit (`libmodkit.a`, `module.ld`, headers, scripts) with `install-modkit.sh` (step 5 of [docs/BUILDING.md](docs/BUILDING.md)). See [docs/MODULES.md](docs/MODULES.md) (with the changes a GCCSDK 4.7.4 module Makefile needs) and [docs/CROSS-COMPILER.md](docs/CROSS-COMPILER.md#risc-os-modules--mmodule-cmunge).
* **`SharedULibFix`** (new, optional): the fixed **SharedUnixLibrary** module 1.16-vforkfix3, which the author's machine has run since 4 Oct 2026, as a PackMan package with an installer. A `vfork` child that ends without `exec` no longer kills its parent, resizes its Wimp slot or (under a parent that was started by `exec`) freezes the machine. **It replaces a system module**, so the package changes nothing until you run its `Install`: that replaces the module only if it is exactly the stock 1.16 (the whole file is compared), refuses a runtime older than fix level 10, backs the stock module up twice, checks every copy and puts the stock module back if a check fails; `Restore` goes back and `Check` tests the result after the reboot. [docs/SHAREDULIB-FIX.md](docs/SHAREDULIB-FIX.md).
* Checked: TickMod and the port of a network module, built through the new commands, are byte for byte the images that ran on the Pi; the real source of that module builds unchanged and passes the host simulation; the 34 breakages of the port's host test are all caught; `cross-smoke.sh` has nine new checks, and the self-test has a twelfth check that builds, loads and runs a module on RISC OS.
* Checked on the Raspberry Pi with the packages of this release, installed with PackMan: the self-test (twelve of twelve checks in 32 seconds; the new one builds, loads and runs a module), the full regression run (all 54 summary lines identical to 16.2.0-13, none failing), and the network module built on the Pi by the commands of its GCCSDK 4.7.4 makefile (3 seconds; byte for byte the module that the cross compiler makes; loaded and run through 39 network checks). The build instructions ([docs/BUILDING.md](docs/BUILDING.md)) were run again in an empty home directory, and `Gcc16`, `Gcc16SelfTest` and the Linux tarball of this release are the output of that run.

## v16.2.0-13: 2026-10-06, gprof, and throwback from the assembler and the linker, UnixLib fix level 15

| Asset | Version |
|---|---|
| native compiler `Gcc16` | 16.2.0-13 |
| runtime `SharedLibs-C-armeabihf` | 16.2.0-13 (UnixLib fix level 15) |
| runtime `SharedLibs-C++-armeabihf` | 16.2.0-5 (unchanged) |
| runtime `SharedLibs-Fortran-armeabihf` | 16.2.0-2 (unchanged) |
| self-test `Gcc16SelfTest` | 16.2.0-13 |
| Linux cross compiler | 16.2.0-13 |

New since 16.2.0-12:

* **`gprof` works** (`-pg`), natively and with the cross compiler. The compiler counts every call (`push {lr}; bl __gnu_mcount_nc` after the prologue, the AAPCS way; the old `mcount` call assumed an APCS frame), the program links `gcrt0.o` and writes `gmon.out` when it ends, and `gprof` (new in the native package and the Linux tarball: `arm-riscos-gnueabihf-gprof`) prints the flat profile and the call graph. The time is sampled 50 times a second by a thread that UnixLib starts behind `profil ()`; it works in a Task window and in the desktop. A program built on Linux ran three times on the Pi with exact call counts. See [USING-NATIVE.md](docs/USING-NATIVE.md#profiling-with-gprof), [CROSS-COMPILER.md](docs/CROSS-COMPILER.md#profiling-with-gprof) and the limits in [KNOWN-ISSUES.md](docs/KNOWN-ISSUES.md).
* **Throwback from the assembler and the linker**: `-mthrowback` gives `--throwback` to `as` and `ld` (they also work on their own); the assembler's errors and the linker's undefined references (with a source file and line in the debug information) go to the editor's throwback window. Tested on the Pi with StrongED: both show up and a double click opens the source.
* **A file name that starts with a path variable** (`<Obey$Dir>.c.main`) is a file for throwback now: the compilers (and the new assembler and linker code) took every name that starts with `<` for a pseudo file such as `<stdin>`, and sent nothing.
* **UnixLib fix level 15** (the runtime package): `__gnu_mcount_nc`, `__gmon_start__` for EABI programs, and the sampler thread behind `profil ()`. One more report for the GCCSDK maintainers (25, [docs/UPSTREAM.md](docs/UPSTREAM.md)).
* **The cross compiler links against the UnixLib of its release**: the sysroot of the tool chain had GCCSDK's 10.2.0 `libunixlib.so`, `crt0.o` and `gcrt0.o`; `docs/BUILDING.md` step 5 now installs the fixed ones (`install-unixlib-sysroot.sh`), which `-pg` programs need to link.
* The self-test (`Gcc16SelfTest`) has eleven checks: a `-pg` program, `gprof`, and a small program that checks the call counts of its report.
* `Gcc16` 16.2.0-13 needs `SharedLibs-C-armeabihf` 16.2.0-13 or later (install the runtime first, then reboot).
* Not changed: the C++ and Fortran runtimes.
* Checked on the Raspberry Pi: the full regression run (all 54 summary lines identical to 16.2.0-12, none failing), the library tests at fix level 15, a program built with `-pg` by the cross compiler and read by the native `gprof`, throwback from the assembler and the linker, and the self-test (eleven of eleven checks). The build instructions ([docs/BUILDING.md](docs/BUILDING.md)) were run again in an empty home directory, and the packages and the Linux tarball of this release are the output of that run.

## v16.2.0-12: 2026-10-06, coverage and profile-guided optimisation, UnixLib fix level 14

| Asset | Version |
|---|---|
| native compiler `Gcc16` | 16.2.0-12 |
| runtime `SharedLibs-C-armeabihf` | 16.2.0-12 (UnixLib fix level 14) |
| runtime `SharedLibs-C++-armeabihf` | 16.2.0-5 (unchanged) |
| runtime `SharedLibs-Fortran-armeabihf` | 16.2.0-2 (unchanged) |
| self-test `Gcc16SelfTest` | 16.2.0-12 |
| Linux cross compiler | 16.2.0-12 |

New since 16.2.0-11:

* **`gcov` and profile-guided optimisation work** (`--coverage`, `-fprofile-generate`, `-fprofile-use`), natively and with the cross compiler. libgcov used to be built without the C library (`inhibit_libc`) and could not be linked; now it is built properly, and `gcov` is part of the native package and the cross tarball. See [USING-NATIVE.md](docs/USING-NATIVE.md#coverage-and-profile-guided-optimisation) and [CROSS-COMPILER.md](docs/CROSS-COMPILER.md#coverage-and-profile-guided-optimisation).
* **UnixLib fix level 14** (the runtime package): the `.fini_array` of a program runs at exit (C destructors; the exit function of libgcov, which is why coverage needed this); `getrlimit (RLIMIT_STACK)` reports the real size of the main stack (it said 512 MB for a 1 MB stack); POSIX semaphores block instead of polling, `sem_timedwait` works and is declared, and `sem_wait` no longer leaks. Three more reports for the GCCSDK maintainers (22 to 24, [docs/UPSTREAM.md](docs/UPSTREAM.md)).
* The binutils programs of the native package have the **8 MB stack** that the documentation always said (the 16.2.0-11 build was made before the request was added: they had 1 MB).
* The compilers' internal-error message points at this repository's issue tracker (it still named the GCCSDK site).
* The self-test (`Gcc16SelfTest`) has ten checks: a coverage run with `gcov` and a profile-guided build were added.
* `Gcc16` 16.2.0-12 needs `SharedLibs-C-armeabihf` 16.2.0-12 or later (install the runtime first, then reboot).
* Not changed: `gprof` (`-pg`) still writes no `gmon.out` ([KNOWN-ISSUES.md](docs/KNOWN-ISSUES.md)); throwback from the assembler and the linker; the C++ and Fortran runtimes.
* Checked on the Raspberry Pi: the full regression run (50 summary lines identical to 16.2.0-11, none failing; four new ones pass), the new library tests (destructors, `RLIMIT_STACK`, semaphores, the `.gcda` files) and the self-test (ten of ten checks). The build instructions ([docs/BUILDING.md](docs/BUILDING.md)) were checked by running every command again in an empty home directory; the Linux tarball of this release is that rebuild.

## v16.2.0-11: 2026-10-05, the first public release

| Asset | Version |
|---|---|
| native compiler `Gcc16` | 16.2.0-11 |
| runtime `SharedLibs-C-armeabihf` | 16.2.0-11 (UnixLib fix level 13) |
| runtime `SharedLibs-C++-armeabihf` | 16.2.0-5 (unchanged) |
| runtime `SharedLibs-Fortran-armeabihf` | 16.2.0-2 (unchanged) |
| Linux cross compiler | 16.2.0-11 |

New since the last internal release (16.2.0-10):

* **`scanf` understands `long long`** (`ll`, `q`, `j`, `hh`, `z`, `t` and `%Lf`): the root cause of the native `-flto` failures. UnixLib fix level 13.
* **`-flto=N`, `-flto=auto` and a make job server work in the native compiler** (the optimisation jobs run one after the other).
* The package metadata names the real maintainer and points at this repository.
* Checked by the full regression run: 50 summary lines identical to the previous release, none failing.
* All five packages were installed with PackMan on the test machine and pass the self-test (8 of 8 checks, 22 seconds).
* The build instructions ([docs/BUILDING.md](docs/BUILDING.md)) were checked by running every command again in a fresh copy of the repository; the Linux tarball of this release is that rebuild.

## The internal releases before it (not published)

| Native compiler | What it added |
|---|---|
| 16.2.0-1 to -5 | the first native compilers, `make` 4.4.1, the heap in dynamic areas, sizes of heaps built in |
| 16.2.0-6 | `gfortran` |
| 16.2.0-7 | 64 MB stacks for `cc1`, `cc1plus` and `f951`, 8 MB for `make` |
| 16.2.0-8 | throwback (`-mthrowback`) |
| 16.2.0-9, -10 | native `-flto` (test packages; `-10` carried a workaround for the `scanf` bug) |

The runtime's fix levels 5 to 14 are described in [docs/RUNTIME.md](docs/RUNTIME.md).
