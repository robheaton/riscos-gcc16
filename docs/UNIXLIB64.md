# UnixLib for RISC OS 64: scope, options and plan (16 October 2026)

**The question:** what would it take to give programs for RISC OS 64 the C library that programs for 32-bit RISC OS have (UnixLib: POSIX, threads, signals, sockets, RISC OS file-name translation), and in what order, testing on Pyromaniac.

## 0. Status (20 October 2026)

UnixLib 64 is in [`riscos64/unixlib/`](../riscos64/unixlib) (`-munixlib`) and in the SDK's POSIX glue ([`riscos64/sdk/crt/posix/`](../riscos64/sdk/crt/posix)), on top of picolibc. About 40 ported programs run on it (the GNU tools, gawk, sed, grep, Lua, SQLite, jq and the compression libraries), see [PORTS-TESTS.md](PORTS-TESTS.md). Everything below was tested on the RISC OS Pyromaniac emulator.

| Component | State |
|---|---|
| 1. File-name translation and directories | **done for the common cases**: UnixLib's `__riscosify`/`__unixify` (Unix names to RISC OS names and back, suffix swapping, `,xyz` file types, the special directories), `opendir`/`readdir`/`closedir` with the suffix directories merged into the listing of their parent, `telldir` and a forward `seekdir`, `chdir`/`getcwd`/`rmdir`, `statvfs`, `stat` (with the blocks of a file), `link` as a copy; new files get a file type. Not yet: `.` and `..` in a listing; no symbolic links (RISC OS has none) |
| 2. Process and memory model | **done for the common cases**: the heap is a dynamic area (the first size that fits of 128 MB down to 4 MB; a program that only waits for others can cap it at link time with `-Wl,--defsym=__ul64_heap_max=16M`), `environ`, `getenv`/`setenv`/`clearenv`, ids, `uname`, `gethostname`, `umask`, sleeping, `getrusage`, `getrlimit`, anonymous `mmap`, `ftruncate`, `chmod`, `utime`, `realpath`, `argv[0]` as typed, `getppid`. Not yet: exit handlers for a killed program |
| 3. Terminal and console | `termios`, `isatty`, `read` of the keyboard without `ICANON`, `poll` and `select`, termcap (`libtermcap.a`) |
| 4. errno, locale, wide characters, `iconv` | **Done in the source tree (22 October 2026; in tool chain 0.12):** `iconv` (UTF-8/16/32, UCS-2/4, ASCII, ISO-8859-1 to 16, Windows and DOS code pages, KOI8, RISC OS Latin1; `//TRANSLIT`, `//IGNORE`), `setlocale` per category, `nl_langinfo`, `localeconv`, `strftime` and `strptime` in 36 locales (en_US, en_GB, de_DE, fr_FR ... from glibc), `strcoll`/`strxfrm` in glibc's ISO 14651 order, `printf` with the locale's decimal point and the `'` flag, and the `iconv` command; all compared with glibc (see [`riscos64/unixlib/tests/glibc`](../riscos64/unixlib/tests/glibc)). **Not yet:** the language specific sort orders (Swedish, Danish, Polish, Czech, Spanish `ñ` ...: all locales use the default order), `std::locale` of C++ for any name but "C", `strptime` with day and month names that are not English |
| 5. Signals | `signal`, `raise`, `sigaction`, `kill` of the process itself, `sigprocmask`, `sigsuspend`, `alarm`, and SIGSEGV, SIGBUS, SIGILL and SIGFPE from the kernel's fault handlers with a backtrace; Escape is SIGINT. They arrive at the places where the program polls, not at any instruction. Not yet: `sigsetjmp`/`siglongjmp` as separate functions, real events from the kernel |
| 6. Threads | `pthread_create`, mutexes, condition variables, thread-local storage, C++ `std::thread` and `std::mutex`; `pthread_cancel`; not yet: some thread attributes (stack address, guard size, scheduling, process-shared) |
| 7. `fork`, `vfork`, `exec`, `system`, `popen` | **done** with `UnixEnv$swap=1`: the parent's image and stack are saved in a dynamic area and put back by an exit handler (`system`, `popen`, `posix_spawn`, `vfork` with `exec*` or `_exit`, `wait*`, pipes between commands); `fork` stays `ENOSYS` |
| 8. Sockets and the resolver | written to the documented Internet module interface (the build service for Pyromaniac has no Internet module, so it cannot test them); `getaddrinfo`, `getnameinfo`, `gethostbyname` and `gethostbyaddr` are there (IPv4 only, through the Resolver module, `riscos64/sdk/crt/posix/netdb.c`); the other DNS resolver functions (`res_*`, `dn_*`) are stubs |
| 9. Wimp support | not started. Added after 17 October: `kernel.h` (the `_kernel_*` calls) and `swis.h` (the SWI numbers, `_swi` and `_swix`) for programs written for the RISC OS C library |
| 10. `dlopen` | not started |
| 12. BSD and UnixLib extras | `err.h`, `readv`/`writev`, `readdir_r`, `lsearch`, `insque`, `ftime`, `wait3`/`wait4`, `getgrouplist`, `strsep` (fixed), `b64_ntop`/`b64_pton`, the large file names (`open64`, `stat64` ...); the 70 resolver names (`res_*`, `dn_*`) are not there (they need the Internet module). The gap check is `tools/gapcheck.sh` |
| 11. Tests | `riscos64/unixlib/tests` (the translation, files, POSIX calls, processes, signals, threads, `wordexp`, timers); the test suites of the ports (gawk, sed, grep, Lua, ...) |

## 1. What UnixLib is (measured on the sources of fix level 17, `libunixlib`)

* about **210,000 lines**: 972 C files, 89 assembler files; without the tests (26,000 lines) and the headers (25,000): about 160,000 lines of library.
* **portable bulk** (written for 32-bit, but plain C): the maths (`libm-dbl64` and friends, 64,000 lines, derived from glibc), `gdtoa` (10,000, `printf`/`scanf` of floating point), `regexp`, `stdlib`, `stdio`, `string`, `time`, `wchar`, `locale`, `search`, `crypt`, `resolv` and `netlib`.
* **RISC OS specific C:** `unix` (11,600: files, directories, `stat`, `select`, `dup`, `fork` emulation, file-name translation `__riscosify`), `sys` (5,700: the OS calls), `kernel`, `crt`, `signal` (event handlers, aborts), `pthread`, `termios`, `resource`, `pwd`/`grp`.
* **ARM assembler: 89 files, about 15,000 lines**; 31 of those files (3,300 lines) are the Shared C Library compatibility (`scl`), which the 32-bit kit dropped by decision. The rest: `sys` 2,600, `string` 2,300 (`memcpy`, `strlen` ... fast paths: C versions exist), `signal` 1,200, `module` 1,500 (the module entry), `gcccompat` 850, `pthread` 860, `incl-local` 680 (macros), `crt` 500, `kernel` 460, `gmon` 450, `common` 150.
* **150 inline-assembler sites** in 30 C files; **225 pointer-to-int or int-to-pointer casts in 66 files** (found by compiling every C file for AArch64: **848 of 876 compile** as they are; the 28 that do not need a header or a feature macro, not a rewrite); 476 distinct OS and module SWI names are mentioned, 68 of the C files use the UnixLib global state directly.

## 2. Three ways to do it

| | Approach | My hours | What you get | Risk |
|---|---|---|---|---|
| **A** | **Port UnixLib whole** to AArch64 | 400-700 | the same library as the 32-bit toolchain, every program that builds there builds here | the assembler (11,000 lines for 64-bit), the signal/pthread/fork machinery, and every 32-bit assumption in 66 files; a long time before anything runs |
| **B** | **"UnixLib 64" = picolibc + RISC OS glue + the parts of UnixLib that programs need**, brought in one component at a time | 150-300 for a useful library, 300-450 for most of POSIX | programs build early and often; each component is a separate, testable step on Pyromaniac | two libraries to keep consistent; picolibc's `FILE`, `locale`, `errno` and thread model are not UnixLib's, so programs that use UnixLib internals do not move over |
| **C** | **Only what a given program needs** (NetSurf, a compiler, a utility) | by program | the ports that matter first | a patchwork |

**Recommendation: B**, then A's pieces only where B's pieces are not enough. The SDK of 16 October is already the first step of B (files, `stat`, `time`, `getenv`, `system`, `unlink`, `rename` on picolibc, tested on Pyromaniac and by zlib). B can start paying back in days, and the ARM assembler of A (the largest cost) is mostly avoided, because AArch64 needs C or new small assembler where UnixLib has ARM-specific fast paths.

## 3. Plan for B: components, in order

| # | Component | What | Hours | Testable on Pyromaniac |
|---|---|---|---|---|
| 1 | **File-name translation and directories** | `__riscosify` (Unix path to RISC OS path, `.`/`/` swap, suffix swapping, `<Var>` expansion), `opendir`/`readdir` (OS_GBPB), `mkdir`/`rmdir`, `chdir`/`getcwd`, `stat` fields, `access`, `utime`, `rename` over folders | 20-35 | yes (file system; its differences from RISC OS are known: `fsdiff`) |
| 2 | **Process and memory model** | the AIF64 start-up with the Wimp slot and dynamic areas, `brk`/`sbrk` growing (OS_ChangeDynamicArea), `mmap` over dynamic areas, `atexit`/`exit`, environment (`environ` from system variables), `argv` with quotes | 15-30 | yes |
| 3 | **Terminal and console** | `termios`, `isatty`, `ioctl`, line editing, `select`/`poll` on the keyboard, `kbhit`; the `OS_ReadC` behaviour | 15-25 | partly (no keyboard on the build service) |
| 4 | **errno, `strerror`, locale, wide characters, `iconv`** | already in the kit's library for modules (`modkit/lib`: 129 sources that pass 35 of 39 test sections): reuse them in the program library | 10-20 | yes |
| 5 | **Signals** | `signal`, `raise`, `sigaction` over RISC OS events/escape/abort handlers (the 32-bit code is about 4,000 lines with assembler: for AArch64 a new design: `SIGINT` from Escape, `SIGALRM` from `OS_CallEvery`, faults from the abort handler) | 30-60 | partly (events need the kernel's conventions, which are not public) |
| 6 | **Threads (pthreads)** | context switch (a new, small AArch64 assembler), mutexes, condition variables, TLS; UnixLib's design uses a timer tick and `Wimp_Poll` | 40-80 | yes (the tick is a SWI/event: depends on the kernel) |
| 7 | **`fork`/`vfork`/`exec`, `system`, `popen`** | `vfork` by saving the stack, `exec` by `OS_Module`/`Wimp_StartTask` | 25-50 | **done for Pyromaniac** with `UnixEnv$swap=1`: the parent's image and stack are saved in a dynamic area and put back by an exit handler (`system`, `popen`, `posix_spawn`, `vfork`+`exec*`/`_exit`, `wait*`); `fork` stays `ENOSYS` |
| 8 | **Sockets** (`netlib`, `resolv`) | wait for a 64-bit Internet/Resolver SWI ABI; the code is plain C once it exists | 15-25 (after the ABI) | no (blocked) |
| 9 | **Wimp support** | `Wimp_Poll` loops need the program's own libraries (DeskLib, OSLib, RISC_OSLib); what UnixLib gives is the task start-up and the polling thread tick | 15-30 for UnixLib's part | partly |
| 10 | **Dynamic loading** (`dlopen`, shared libraries: `ld-riscos.so`) | an ELF loader for AArch64; needs a kernel-side or a program-side design | 60-120 | depends |
| 11 | **Tests** | UnixLib's own `test/` (26,000 lines, 70 files) as the oracle, plus glibc-compared hash tests like `libtest` for the new parts | 30-60 | yes |
| | **Total for B** | | **275-535** | |

Components 1, 2, 4, 11 and the C++ library (below) are the first release; 3, 5, 6, 7 the second; 8-10 depend on what the kernel provides.

## 4. Also needed (not UnixLib itself)

* **A hosted C++ library for programs:** `libstdc++` built for `aarch64-riscos` on top of the program library (20-40 hours; exceptions and RTTI with the unwinder: +20-40).
* **A proper GCC target** `aarch64-riscos` instead of the wrapper over `aarch64-none-elf` (driver specs, `-mmodule`, default flags, multilib, `libgcc`/`libstdc++`/`libgfortran` configuration, `--with-sysroot`): 30-60 hours; the wrapper is enough until then.
* **The ports** that exercise it (zlib done; then libpng, libjpeg, lua/sqlite, GMP/MPFR/MPC in progress, a bigger one such as NetSurf's libraries): 5-15 hours each.

## 5. What I would do first (all on Pyromaniac)

1. **Component 1 (names and directories), 2 (process and memory), 4 (reuse the kit's library)** inside a new `libunixlib64.a` that the driver links for programs: about 50-85 hours, in steps of a few hours, each with a test program and glibc-compared hashes where possible.
2. Port **UnixLib's own test suite** progressively as the oracle.
3. In parallel, the **ports** (GMP/MPFR/MPC, libpng, libjpeg) as the first users: they find the missing functions faster than reading UnixLib does.
4. Components 5-7 after the kernel's conventions for events, callbacks and `Wimp_StartTask` are known (they cannot be guessed; Pyromaniac's may differ).

## 6. Decisions needed

* **A or B** (recommended: B).
* **Where the file-name translation rules come from:** UnixLib's (compatible with every 32-bit port: recommended) or a new, simpler scheme.
* **Keep the Shared C Library compatibility out:** already decided for the 32-bit kit; 31 files and 3,300 lines of assembler are then not ported.
* **Wimp programs:** whether the first goal includes desktop programs (OSLib/DeskLib structures with pointers need a 32-bit layout on a 64-bit machine, or new headers).

## 7. Risks

* UnixLib's design depends on 32-bit RISC OS features whose 64-bit form is not public (callbacks, events, the abort handler, `Wimp_StartTask`, dynamic areas, the stack chunk mechanism): components 5-7 and 10 may have to wait or be built on guesses that Pyromaniac accepts and another kernel does not.
* Differences between Pyromaniac and RISC OS (file system, `*RMReInit`, decoding code) mean that Pyromaniac results are evidence for the library's logic, not for the kernel's behaviour; the library has since also been run on real RISC OS 64 hardware.
