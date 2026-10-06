# Tests

[docs/TESTING.md](../docs/TESTING.md) explains the kinds of test and what to expect. This folder in one table:

| Folder | What | Needs |
|---|---|---|
| [`selftest/`](selftest) | **start here on RISC OS**: checks a fresh installation of the native compiler (about half a minute) | the packages installed |
| [`cross-smoke/`](cross-smoke) | **start here on Linux**: checks the cross compiler in a directory, and that it finds everything inside it | the cross toolchain |
| [`fixlevel/`](fixlevel) | `fixlevel N`: is the UnixLib in use at fix level N? | any compiler |
| [`rotest/`](rotest), [`cxx/`](cxx), [`fortran/`](fortran), [`shlib/`](shlib), [`random/`](random), [`bench/`](bench) | the compiler regression suites (34,541 C checks, 139 C++ checks, Fortran, threads, shared libraries, benchmarks): built on Linux, run on RISC OS | the cross toolchain; GCCSDK 10.2.0 for the `g10`/`cx10` comparison variants |
| top-level `*.c`, `ulinfo.c`, [`unixlib16/`](unixlib16) | small code-generation probes and the library checks (`ulinfo`: 48 checks) | the cross toolchain |
| [`unixlib17/`](unixlib17), [`unixlib18/`](unixlib18), [`unixlib24/`](unixlib24), [`unixlib25/`](unixlib25), [`upstream20/`](upstream20) | stack and heap sizes, `mmap` limits, process exit, `scanf`, `vfork` heap, the `.fini_array`, `getrlimit`, semaphores, `.gcda` files: the runtime tests | the cross toolchain |
| [`gprof/`](gprof) | `-pg` and gprof: `pgtest` (heavy and light functions for a fixed time) must write `gmon.out` and show its own call counts | the cross toolchain, then RISC OS |
| [`unixlib19/`](unixlib19), [`sulfix/`](sulfix) | `vfork` children and SharedUnixLibrary. **Can freeze a machine that has the stock module** | the fixed SharedUnixLibrary |
| [`throwback/`](throwback) | host test of the throwback code (66 checks, 27 mutations) and the programs of the hardware test | a host C++ compiler |
| [`armeabisupport-model/`](armeabisupport-model), [`unixlib-fix/`](unixlib-fix) | host models of ARMEABISupport's memory code and of UnixLib fixes | a host C compiler |

The build scripts take the compiler from variables (`NEW=` for GCC 16, `OLD=` for GCCSDK's 10.2.0) and default to the author's directory layout (`~/gccsdk-next/env-f`, `~/gccsdk/env`).
Files ending `,e1f` on a Samba share become ELF programs on RISC OS, `,feb` Obey files. The Obey runners write their results next to themselves with `Spool`.
