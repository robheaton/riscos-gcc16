# Known issues and limits

## Tested on one machine

Everything was tested on **one** machine: a Raspberry Pi Compute Module 4 (Cortex-A72) with RISC OS 5.30, ARMEABISupport 1.08, Shared Object Manager 3.04 and SharedUnixLibrary 1.16 (the final runs with the fixed 1.16-vforkfix3, see below).
The programs are 32-bit ARMv7 with VFPv3: other ARMv7 machines should work, but have not been tried. Machines without ARMv7 and VFP (old ARMv5 and ARMv6 RISC OS machines) cannot run them.

## Not included, and not working

| What | State |
|---|---|
| OpenMP (`-fopenmp`) | not built: the driver stops with an error |
| Sanitizers (`-fsanitize=...`) | not built |
| `gprof` (`-pg`) | the program links and runs but writes **no** `gmon.out`: UnixLib's profiler is compiled out for EABI programs (its source takes over the interrupt vector, and was not tried). `gcov` and profile-guided optimisation work (see below) |
| `-ftime-report` (native compiler) | prints, but the times are wrong (cause not investigated) |
| Wide-character iostreams (`std::wcout`, `std::wcin` ...) | libstdc++ is built without them |
| `std::stacktrace` | does not link |
| C++17 parallel algorithms (`std::execution::par`) | compile; this build has no threading back end for them, so they should run in sequence (not tested) |
| Fortran `REAL(16)`, multi-image coarrays | not available (`libquadmath` and the coarray library are not built); single-image coarrays compile |
| A debugger | none |
| `-static` | links; not tested on RISC OS (the tested configuration is the shared UnixLib) |
| OSLib, DeskLib and other RISC OS libraries | not part of this release (the Linux tarball has UnixLib's headers only) |
| `-pthread` | not an option: threads are in UnixLib and need no flag |
| `-static-libgcc` | the link fails unless you add `-Wl,--allow-shlib-undefined`: UnixLib refers to libgcc symbols |

## The native compiler

* **make** never uses a shell: no pipes, redirections or `&&` in recipes; `-jN` runs one job at a time (a `vfork` child runs to completion before `vfork` returns).
* **`-flto`**: the native linker has no plugin support, so objects inside an archive (`libfoo.a`) are not optimised across modules (give the object files to the link), and the optimisation jobs run one after the other. The Linux cross compiler uses the linker plugin and does not have these limits.
* **Coverage**: `gcov` and the `--coverage` and `-fprofile-generate` programs need the runtime 16.2.0-12 (the counts are written by an exit function that earlier runtimes never ran). A program that ends with `_exit`, `abort` or a crash writes no counts. A program built by the *cross* compiler names its `.gcda` file with the Linux path it was built at: set `GCOV_PREFIX` and `GCOV_PREFIX_STRIP` on RISC OS ([CROSS-COMPILER.md](CROSS-COMPILER.md#coverage-and-profile-guided-optimisation)). `-fprofile-update=atomic` and the coverage of threads were not tested.
* **Throwback**: the assembler and the linker send none. It needs the DDEUtils module and an editor that has registered with it (tested with StrongED), and works only in the desktop.
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
* **UnixLib guesses whether a relative file name is a Unix or a RISC OS name**, and takes a name such as `foo.c.gcov` or `foo.h.bak` (a suffix from the swap list in the middle) for a RISC OS path. A program that opens such a name fails or finds nothing; the tools of this package ask for Unix names, a program of your own does the same with `int __riscosify_control = __RISCOSIFY_STRICT_UNIX_SPECS;` ([USING-NATIVE.md](USING-NATIVE.md#file-names-unix-names-risc-os-directories)).

## Bugs in RISC OS modules that this release does not change

The runtime packages fix UnixLib. They do not replace system modules. These bugs are in the modules RISC OS ships (details and patches: [UPSTREAM.md](UPSTREAM.md)):

* **SharedUnixLibrary 1.16 (3 Apr 2020)**: a `vfork` child that ends **without** `exec` (the usual reaction to an `exec` that failed: `if (vfork () == 0) { execv (...); _exit (127); }`) frees its parent's main stack, so the parent dies; grows its parent's Wimp slot to the maximum; and, when the parent was itself
  started by `exec` (everything under `make` or a shell), deregisters the parent's Shared Object Manager client, which **froze the machine** in a loop test. Ordinary compiles and `make` runs are not affected (the regression suites, including chains of `make` -> `gcc` -> `cc1` -> `as`, pass with the stock module), but a program that forks children that fail to `exec` can be. A fixed module (1.16-vforkfix3) exists and the
  author's machine runs it; the patches are in [`docs/upstream/patches/`](upstream/patches), and it is not part of this release because it replaces a system module.
* **ARMEABISupport 1.08**: a failed `mmap` leaves its `mmap#N` dynamic area (and the memory it had claimed) until the next reboot; UnixLib now refuses the requests that can never succeed (2 GB or more, or over the OS clamp) before the module is asked, but other failures can still leak. The memory of a `vfork` + `exec` child
  stays allocated until the process at the root of its family ends. `*RMKill ARMEABISupport` while the Shared Object Manager holds one of its handles leaves every EABI program unable to start until a reboot.
* **!Iris** (the browser) carries its own SharedLibs and redirects `SharedLibs:` to them when it runs; two sets cannot both be in use, so reboot after running it before using this tool chain.

## Packaging

* The packages are **not in a PackMan repository**: install the zips from the releases page by hand (drag them onto PackMan).
* The compilers' internal-error message points at this repository's [issue tracker](https://github.com/robheaton/riscos-gcc16/issues) (releases before 16.2.0-12 pointed at `http://gccsdk.riscos.info/`: ignore that and report problems here; GCCSDK does not maintain this port).
* The loader, `libgcc_s.so.1` and `libdl` in the C runtime package are GCCSDK's 10.2.0 files, unchanged.

## Reporting a problem

Open an [issue](https://github.com/robheaton/riscos-gcc16/issues) with: what you ran (the exact command), what you expected, what happened, `Echo <GCC16$Version>`, the output of `fixlevel`, your RISC OS version and machine, and (for a compiler crash) the source that triggers it.
