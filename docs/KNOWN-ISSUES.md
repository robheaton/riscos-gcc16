# Known issues and limits

## Tested on one machine

Everything was tested on **one** machine: a Raspberry Pi Compute Module 4 (Cortex-A72) with RISC OS 5.30, ARMEABISupport 1.08, Shared Object Manager 3.04 and SharedUnixLibrary 1.16 (the final runs with the fixed 1.16-vforkfix3, see below).
The programs are 32-bit ARMv7 with VFPv3: other ARMv7 machines should work, but have not been tried. Machines without ARMv7 and VFP (old ARMv5 and ARMv6 RISC OS machines) cannot run them. Modules (`-mmodule`) are ARMv6 and soft float: they need an ARMv6 or newer CPU, and have been run on the Cortex-A72 only.

## Not included, and not working

| What | State |
|---|---|
| OpenMP (`-fopenmp`) | not built: the driver stops with an error |
| Sanitizers (`-fsanitize=...`) | not built |
| `-ftime-report` (native compiler) | prints, but the times are wrong (cause not investigated) |
| Wide-character iostreams (`std::wcout`, `std::wcin` ...) | libstdc++ is built without them |
| `std::stacktrace` | does not link |
| C++17 parallel algorithms (`std::execution::par`) | compile; this build has no threading back end for them, so they should run in sequence (not tested) |
| Fortran `REAL(16)`, multi-image coarrays | not available (`libquadmath` and the coarray library are not built); single-image coarrays compile |
| A debugger | none |
| `-static` | links; not tested on RISC OS (the tested configuration is the shared UnixLib) |
| OSLib, DeskLib, FlexLib, SFLib, RUfl | not shipped (licences): built from your own copy by `install-oslib.py`, `install-desklib.py`, `install-flexlib.py`, `install-sflib.py`, `install-rufl.py` ([OSLIB.md](OSLIB.md), [DESKLIB.md](DESKLIB.md), [FLEXLIB.md](FLEXLIB.md), [RUFL.md](RUFL.md)); the Linux tarball has the headers of UnixLib and of the module kit only |
| Applications for the Shared C Library (`-mlibscl`, AIF, `elf2aif`) | not supported: `-mlibscl` without `-mmodule` is ignored (you get a UnixLib ELF program); keep GCCSDK 4.7.4 for those |
| `-pthread` | not an option: threads are in UnixLib and need no flag |
| `-static-libgcc` | the link fails unless you add `-Wl,--allow-shlib-undefined`: UnixLib refers to libgcc symbols |

## The native compiler

* **make** never uses a shell: no pipes, redirections or `&&` in recipes; `-jN` runs one job at a time (a `vfork` child runs to completion before `vfork` returns).
* **`-flto`**: the native linker has no plugin support, so objects inside an archive (`libfoo.a`) are not optimised across modules (give the object files to the link), and the optimisation jobs run one after the other. The Linux cross compiler uses the linker plugin and does not have these limits.
* **Coverage**: `gcov` and the `--coverage` and `-fprofile-generate` programs need the runtime 16.2.0-12 (the counts are written by an exit function that earlier runtimes never ran). A program that ends with `_exit`, `abort` or a crash writes no counts. A program built by the *cross* compiler names its `.gcda` file with the Linux path it was built at: set `GCOV_PREFIX` and `GCOV_PREFIX_STRIP` on RISC OS ([CROSS-COMPILER.md](CROSS-COMPILER.md#coverage-and-profile-guided-optimisation)). `-fprofile-update=atomic` and the coverage of threads were not tested.
* **Throwback** needs the DDEUtils module and an editor that has registered with it (tested with StrongED), and works only in the desktop. Since 16.2.0-13 the assembler and the linker send theirs too (`-mthrowback` gives them `--throwback`); an error of the linker is reported with the source file and line that the object's debug information gives (compile with `-g`), otherwise there is no line and no entry.
* **Modules** (`-mmodule`, new in 16.2.0-14; [MODULES.md](MODULES.md); moving from GCCSDK 4.7.4, and who should stay with it: [PORTING-FROM-4.7.4.md](PORTING-FROM-4.7.4.md)). Run on one machine, a Compute Module 4 (Cortex-A72), never on an ARMv6 CPU. The limits:
  * **CPU and floating point.** A module is ARMv6 code (it uses `uxtb`, `uxth`, `rev`): it does not run on ARMv5 and older machines, and `-mmodule` silently replaces any `-march=` or `-mcpu=`. It is soft float whatever `-mfpu=` says; `printf`, `scanf` and `strtod` are exact, `<math.h>` is within 1 to 2.2 units in the last place. There is no hardware floating point in a module.
  * **C library.** Its own, about 590 functions ([list](MODULES.md#the-c-library-of-modkit)). `<fenv.h>` is software only (the arithmetic sets no flags, the rounding mode is always to nearest); `long double` is `double`; no wide-character input and output (`fwprintf`, `fgetwc`, `swprintf`, `swscanf`) and no `wcsftime`. `stdio` files have [their own rules](MODULES.md#stdio-files-and-streams): files of less than 2 GB, one stream for a file that is written, a file made by `fopen` has type Data, nothing is closed when a module is killed (call `__modlib_closeall ()`). `time_t` is 32 bit and the time zone is UTC. Sockets: BSD calls on the Internet module, file descriptors of the same table as `open`'s since 16.2.0-23 (the number is not the Internet module's handle: `__modlib_socket_handle (fd)`); no `gethostbyaddr`, IPv6 or `getaddrinfo`; `fdopen` on a socket fails. The values of `EDOM`, `ERANGE`, `BUFSIZ` and `FOPEN_MAX` are UnixLib's, not the Shared C Library's ([the differences](PORTING-FROM-4.7.4.md#what-behaves-differently)).
  * **C++** ([MODULES.md](MODULES.md#c-in-modules)): constructors, `new`, templates and the containers work; no exceptions, RTTI, streams or threads. C++ modules are built on Linux.
  * **Stack.** No private stack and no stack checking: the C code of a module runs on the kernel's 32 KB SVC stack, as 4.7.4's did (`__modlib_stack_left ()` says how much is left).
  * **CMHG.** `cmunge` refuses `library-enter-code`, `library-initialisation-code`, the `help:` flag of a command (CMunge refuses it too), `swi-handler-code: f (flags-capable:)` (CMunge too) and `module-is-runnable: simple-app:`; every other directive of CMunge 0.76 is accepted (`vector-traps:` and `pdriver-handler:` since 16.2.0-22).
  * **Size.** The C library is linked into the module (the linker drops the functions that nothing uses since 16.2.0-23): SysLogD is 11,108 bytes with 4.7.4 and 38,780 here ([PORTING-FROM-4.7.4.md](PORTING-FROM-4.7.4.md#why-move-and-who-should-not)).
  * **Faults of the first releases, fixed.** 16.2.0-14 and -15: a SWI handler that returned `error_BAD_SWI` made the SWI return with V set and R0 = -1 (a program that read the error block crashed), and a command handler that returned `configure_BAD_OPTION` (-1) was taken for success; rebuild with 16.2.0-16 or later ([OS-MODULES.md](OS-MODULES.md#what-it-found)). 16.2.0-14 to -16: `libmodkit.a` named the whole `libgcc.a`, built for ARMv7 with the VFP; a module that converted a `double` or `float` to a `long long`, or used `__builtin_popcount`, `__builtin_parity`, `__builtin_ffs`, `__builtin_ctzll` or `__builtin_powi` (also complex multiply and divide, `_BitInt` wider than 64 bits, half precision), contained VFP, `movw`/`movt` or `rbit` instructions (an undefined instruction on an ARMv6 CPU; on a Pi 4 it used the VFP registers of the program that called it); fixed in 16.2.0-17 by `libgcc-mod.a` ([MODULES.md](MODULES.md#floating-point)). To see whether a module was affected, link it with `-Wl,-Map=x.map` and look for `_fixdfdi.o`, `_fixunsdfdi.o`, `_fixsfdi.o`, `_fixunssfdi.o`, `_popcountsi2.o`, `_popcountdi2.o`, `_paritysi2.o`, `_paritydi2.o`, `_ffssi2.o`, `_ffsdi2.o`, `_ctzdi2.o`, `_powidf2.o` or `_powisf2.o` under `libgcc.a`.
  * **Save your work before loading a module that is new.**
* **gprof** (`-pg`, new in 16.2.0-13): the calls are counted exactly, but the time is only sampled, 50 times a second: a program has to run for some seconds, a sample belongs to a function and not to a line, and only the program's own code counts (time inside UnixLib and libstdc++ is not in the profile). A tick that falls into a part of UnixLib that may not be interrupted (`malloc`, stdio) is lost, and a program with threads of its own is sampled only at every other thread's turn (the shape of the profile is right, the time too small). The program has to end by returning from `main` or by `exit`. The sampler is a second thread that UnixLib starts: a program that counts its threads will see it.
* **Memory**: the Task window needs at least 48 MB. RISC OS may clamp the maximum size of a dynamic area (128 MB was seen), so the compilers get less than the 512 MB they ask for; that is enough for ordinary sources, and a compile that ends with `out of memory` or `virtual memory exhausted` has hit it.
* **Stack**: the compilers run with 64 MB stacks, so deep template or `constexpr` recursion works to the compiler's own limits; `make` and the binutils programs have 8 MB; every other EABI program has a 1 MB stack unless it sets `__stack_size` (and `getrlimit (RLIMIT_STACK)` reports what it has).
* **Speed** (one run on the Compute Module 4, to give an idea): `gcc -O2` compiles and links a "hello world" in about 1 second, `g++` in about 7 seconds (the C++ headers and the static libstdc++), and the four-file, 34,541-check C test program in about 12 seconds. Keep temporary files on a local disc (`TMPDIR`): on a network share the compiler was about 20% slower.
* `!GCC16` is not a desktop program: double-clicking it only sets system variables (it opens no window). Do it after every boot and work in a Task window.

## RISC OS behaviour that programs can trip over

* **The OS loses stores to unmapped stack pages.** RISC OS maps a program's stack one 4 KB page at a time, when it is first touched. When *the OS* (in supervisor mode) is the first to touch a page, for example a SWI that fills a big local buffer
  (`OS_GBPB`, `OS_GSTrans` ...), the store that takes the page fault is **lost**: the buffer comes back with wrong bytes, and the SWI reports success. This tool chain avoids it for your compiled code (stack probing, on by default) and for UnixLib (`read`, `fread`, `recv`, `recvfrom` touch the buffer first), but
  **your own SWI calls** can still hit it: pass such SWIs `malloc`'d or static memory, or write one byte to each 4 KB page of the buffer first.
* **A 64-byte `vstm`** (a store of eight D registers) that is the first access to a stack page is not restarted and the program dies with `SIGSEGV`. UnixLib's `memcpy` was changed to avoid it; hand-written NEON code should too.
* Whether the two problems above are bugs in RISC OS, in ARMEABISupport or by design is not known; a reproducer and the questions are in [`docs/upstream/07-...`](upstream/07-RISCOS-svc-mode-store-to-unmapped-stack-page-lost.txt).
* **UnixLib before the runtime 16.2.0-14 has no `EPROTO`** (a POSIX error code) and six more; programs that use them, such as OpenSSL, need `-DEPROTO=<a value that does not occur>` with an older runtime or tool chain ([NETSURF.md](NETSURF.md)); the runtime 16.2.0-14 and the tool chains of 16.2.0-22 and later have them ([RUNTIME.md](RUNTIME.md)).
* **A program linked statically with a `libunixlib.a` older than 16.2.0-24 has the old thread ticker** (a second thread made before `Wimp_Initialise` can make other tasks abort and lock the machine; [RUNTIME.md](RUNTIME.md), fix level 17): link it again with the new tool chain. A dynamically linked program takes the fix from the runtime package 16.2.0-15.
* **UnixLib guesses whether a relative file name is a Unix or a RISC OS name**, and takes a name such as `foo.c.gcov` or `foo.h.bak` (a suffix from the swap list in the middle) for a RISC OS path. A program that opens such a name fails or finds nothing; the tools of this package ask for Unix names, a program of your own does the same with `int __riscosify_control = __RISCOSIFY_STRICT_UNIX_SPECS;` ([USING-NATIVE.md](USING-NATIVE.md#file-names-unix-names-risc-os-directories)).

## Bugs in RISC OS modules that this release does not change

The runtime packages fix UnixLib. They do not replace system modules. These bugs are in the modules RISC OS ships (details and patches: [UPSTREAM.md](UPSTREAM.md)):

* **SharedUnixLibrary 1.16 (3 Apr 2020)**: a `vfork` child that ends **without** `exec` (the usual reaction to an `exec` that failed: `if (vfork () == 0) { execv (...); _exit (127); }`) frees its parent's main stack, so the parent dies; grows its parent's Wimp slot to the maximum; and, when the parent was itself
  started by `exec` (everything under `make` or a shell), deregisters the parent's Shared Object Manager client, which **froze the machine** in a loop test. Ordinary compiles and `make` runs are not affected (the regression suites, including chains of `make` -> `gcc` -> `cc1` -> `as`, pass with the stock module), but a program that forks children that fail to `exec` can be. A fixed module (1.16-vforkfix3) exists and the
  author's machine runs it; the patches are in [`docs/upstream/patches/`](upstream/patches). It is an **optional package**, `SharedULibFix`, and not part of the runtime package, because it replaces a system module: [SHAREDULIB-FIX.md](SHAREDULIB-FIX.md) says what it does, how to install it and how to go back.
* **ARMEABISupport 1.08**: a failed `mmap` leaves its `mmap#N` dynamic area (and the memory it had claimed) until the next reboot; UnixLib now refuses the requests that can never succeed (2 GB or more, or over the OS clamp) before the module is asked, but other failures can still leak. The memory of a `vfork` + `exec` child
  stays allocated until the process at the root of its family ends. `*RMKill ARMEABISupport` while the Shared Object Manager holds one of its handles leaves every EABI program unable to start until a reboot.
  There is no fixed module for it yet: the patch in [`docs/upstream/patches/`](upstream/patches) is against the 1.05 sources, the only ones that are published, and 1.08 is a rewrite of the memory handling (it also has file descriptors, file locks and IPC), so the patch does not apply to it. The source of 1.08 has been requested from its author.
* **!Iris** (the browser) carries its own SharedLibs and redirects `SharedLibs:` to them when it runs; two sets cannot both be in use, so reboot after running it before using this tool chain.

## Packaging

* The packages are **not in a PackMan repository**: install the zips from the releases page by hand (drag them onto PackMan).
* The compilers' internal-error message points at this repository's [issue tracker](https://github.com/robheaton/riscos-gcc16/issues) (releases before 16.2.0-12 pointed at `http://gccsdk.riscos.info/`: ignore that and report problems here; GCCSDK does not maintain this port).
* The loader, `libgcc_s.so.1` and `libdl` in the C runtime package are GCCSDK's 10.2.0 files, unchanged.

## Reporting a problem

Open an [issue](https://github.com/robheaton/riscos-gcc16/issues) with: what you ran (the exact command), what you expected, what happened, `Echo <GCC16$Version>`, the output of `fixlevel`, your RISC OS version and machine, and (for a compiler crash) the source that triggers it.
