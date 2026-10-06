# The runtime packages and the UnixLib fixes

Every program built by this tool chain runs on **UnixLib**, RISC OS's C library (a shared library, `libunixlib.so`, from GCCSDK), and, for C++ and Fortran programs that do not link them statically, on `libstdc++` and `libgfortran`.
This release ships a UnixLib that was **rebuilt with GCC 16 and fixed**: running real programs on real hardware found a series of bugs, in UnixLib itself and in how RISC OS maps stacks and memory, and each fix is a small patch in
[`recipe/gcc-16.2.0-riscos/patches-unixlib/`](../recipe/gcc-16.2.0-riscos/patches-unixlib). The bugs are written up as draft reports for the GCCSDK maintainers ([UPSTREAM.md](UPSTREAM.md)).

## The packages

| Package | Version | Contains | Needed for |
|---|---|---|---|
| `SharedLibs-C-armeabihf` | **16.2.0-12** | `libunixlib.so.5.0.0` and `libm.so.1.0.0` (UnixLib 5.0 rebuilt with GCC 16 and fixed); the dynamic loader `ld-riscos-eabihf.so`, `libgcc_s.so.1` and `libdl` of GCCSDK's 10.2.0-1 package | **every** program |
| `SharedLibs-C++-armeabihf` | 16.2.0-5 | `libstdc++.so.6.0.36` | C++ programs that link it dynamically |
| `SharedLibs-Fortran-armeabihf` | 16.2.0-2 | `libgfortran.so.5.0.0` | Fortran programs that link it dynamically |

Install them with PackMan, then reboot ([INSTALL-RISCOS.md](INSTALL-RISCOS.md)). They replace GCCSDK's packages of the same names; programs built by the GCCSDK 10.2.0 compilers keep working on them (checked with a C++ and a thread test).
`libunixlib`, `libm`, `libstdc++` and `libgfortran` in these packages are built with `-fstack-clash-protection` (the loader, `libgcc_s` and `libdl` are GCCSDK's files).

## Which library is loaded? The fix level

`sysconf (0x4700)` is a private query of this runtime: it returns the **fix level** (the stock UnixLib answers `EINVAL`). [`tests/fixlevel/fixlevel.c`](../tests/fixlevel/fixlevel.c) prints it.
The library file's name does not change, and the Shared Object Manager keeps the copy it has loaded until a reboot, so ask the running library, not the file.

| Fix level | Package | What changed | The symptom it cured | Report |
|---|---|---|---|---|
| none | GCCSDK 10.2.0-1 to -4 | (the stock library) | | |
| 5 | 16.2.0-1, -2 | `pthread_once` without a global lock and exception safe; `pthread_cond_timedwait` with centisecond deadlines; `sleep`/`usleep`/`nanosleep` with several threads; `sysconf (_SC_NPROCESSORS_ONLN)` is 1; `fread`/`fwrite` after a short `read`/`write`; five implicit declarations that GCC 14+ rejects | `std::async` deadlocked; timed waits overran by up to a second; a second sleeper woke the first early; `hardware_concurrency()` said 2048; data written or read twice after a short transfer | 09, 10, 11, 12, 13, 14 |
| (none) | 16.2.0-3 | stack buffers are touched before `read`/`fread`/`recv`/`recvfrom` | wrong bytes in a fresh stack buffer: RISC OS (SVC mode) loses the store that takes the page fault of a lazily mapped stack page | 15 (a workaround for 07) |
| 6 | 16.2.0-4 | the same code as -3, now answering the fix level query (-3 did not, through a packaging error) | | |
| 7 | 16.2.0-5 | built with stack probing; `memcpy`/`memmove` without the 64-byte store-multiple | `SIGSEGV`: a 64-byte aligned `vstm` that is the first access to a stack page is not restarted | 16 (a workaround for 07) |
| 8 | 16.2.0-6 | the main stack of an EABI program is as big as its `__stack_size` says (at least the 1 MB it always was), with a fallback to half the size; a heap dynamic area that does not fit is retried at half the maximum, down to 2 MB | deep recursion (the compilers); `Unable to allocate logical address space` when programs that are alive together reserve too much | 17, 18 |
| 9 | 16.2.0-7 | `mmap`/`mremap` refuse a request that can never be served (2 GB or more, or over the OS clamp); the one-page signal stack is freed when a process ends | `new char[SIZE_MAX / 2]` left two 100 MB areas behind per call until the next reboot; 4 KB of the shared stack range lost per process | 03, 04 |
| 10 | 16.2.0-8 | the `_exit` of a `vfork` child that ends without `exec` no longer frees the program image's RMA block that it shares with its parent | a loop of such children corrupted the RMA heap and froze the machine | 02 |
| 11 | 16.2.0-9 | the heap of a program started by `vfork` + `exec` never grows over the copy of its parent that SharedUnixLibrary keeps | the parent died with `abort on data transfer` after a native C++ compile although the child ended normally | 08 |
| 12 | 16.2.0-10 | the inline SWI wrappers no longer read register variables after the `asm` statement; `__get_dde_prefix ()` terminates | with the DDEUtils module loaded (every text editor that does throwback loads it) arguments longer than the program name were cut off, so the native compilers could not compile; a hang when a DDEUtils prefix was set | 19, 20 |
| 13 | 16.2.0-11 | `scanf` understands `ll`, `q`, `j`, `hh`, `z`, `t` and `%Lf` | `%llx` of a 16-digit number stored only 32 bits (it made the native LTO linker fail), `%hhd` wrote two bytes, `%jd %zu %td %qd` were not understood, `%Lf` stored a float | 21 |
| **14** | **16.2.0-12** | the `.fini_array` functions of a program run at exit (after the `atexit` functions, last entry first); `getrlimit (RLIMIT_STACK)` is the size of the main stack of an EABI program; POSIX semaphores block on a condition variable, `sem_timedwait` works and is declared | no C destructor ever ran, and a program built with `--coverage` or `-fprofile-generate` never wrote its `.gcda` file (the exit function of libgcov is a destructor); `RLIMIT_STACK` said 512 MB for a 1 MB stack, so a recursion sized by it overflowed; `sem_timedwait` was `ENOSYS` and not declared, `sem_wait` polled and leaked 16 bytes per call | 22, 23, 24 |

Report numbers refer to the draft mails in [`docs/upstream/`](upstream/00-INDEX.txt); report 01 (SharedUnixLibrary) and the other module-side bugs are described in [KNOWN-ISSUES.md](KNOWN-ISSUES.md#bugs-in-risc-os-modules-that-this-release-does-not-change).
Nothing has been sent to the GCCSDK maintainers yet.

## Verifying a build of the library

`tools/check-libunixlib.sh <libunixlib.so>` looks for the code of every fix in a built library (and runs the start-up and exit logic of the changed functions on an ARM interpreter, `tools/a32.py`), so a patch that silently failed to apply cannot ship.
`recipe/gcc-16.2.0-riscos/scripts/build-unixlib.sh` builds the library from the GCCSDK sources and the patches ([BUILDING.md](BUILDING.md)).

## What did not change

The dynamic loader, `libgcc_s.so.1` (GCC 10.2.0's) and `libdl` are GCCSDK's files from the 10.2.0-1 package. `libunixlib.a` (static UnixLib) is not rebuilt: the cross compiler's tree still carries the 10.2.0 one.
The SharedUnixLibrary module (`System:Modules.SharedULib`) is the one that RISC OS ships; its known bugs are in [KNOWN-ISSUES.md](KNOWN-ISSUES.md).
