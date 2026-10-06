# What is included

Version 16.2.0-12. "Hardware" means the test was run on the author's Raspberry Pi Compute Module 4 (Cortex-A72) with RISC OS 5.30; "host" means it was checked on the Linux build machine only.
See [KNOWN-ISSUES.md](KNOWN-ISSUES.md) for what is *not* included.

## Components

| Component | Version | Native (RISC OS) | Cross (Linux) |
|---|---|---|---|
| GCC: `gcc`, `g++`, `cpp`, `gfortran` | 16.2.0 | yes | yes |
| binutils: `as ld ar nm objdump objcopy readelf strip ranlib size strings addr2line c++filt elfedit gprof` | 2.45.1 | yes | yes (with the `arm-riscos-gnueabihf-` prefix) |
| `gcov` (coverage reports), `gcov-tool`, `gcov-dump` | 16.2.0 | `gcov` | yes (`arm-riscos-gnueabihf-gcov`: reads the `.gcda` files that RISC OS programs write) |
| GNU make | 4.4.1 | yes | (use the host's) |
| UnixLib (C library) | 5.0 from GCCSDK r7800, rebuilt with GCC 16, fix level 15 | the runtime package | the runtime package, and its headers inside the tool chain |
| libstdc++ | 6.0.36 (GCC 16.2.0) | linked statically | dynamic or static |
| libgfortran | 5.0.0 (GCC 16.2.0) | linked statically | dynamic or static |
| libgcc, libgcc_s | GCC 16.2.0 (static), 10.2.0 (`libgcc_s.so.1` in the runtime package) | yes | yes |
| Target | `arm-riscos-gnueabihf`: ARM EABI5, AAPCS-VFP (hard float), UnixLib, default ARMv7-A with VFPv3 | | |

## Languages

| Feature | Notes | Tested |
|---|---|---|
| C, default `-std=gnu23`; C89, C99, C11, C17, C23 | `constexpr`, `typeof`, `nullptr`, `_BitInt`, `bool` as a keyword ... | hardware: the C regression suite `rotest`, 34,541 checks (integers, 64-bit arithmetic, floating point, conversions, varargs, `alloca`, `setjmp`, unwinding, atomics, PIC data, C23), built at `-O0`, `-O2` and `-O3`, with `-fPIC`, and with and without stack probing |
| C++, default `-std=gnu++20`; C++11 to C++23 | exceptions, RTTI, iostreams, containers, strings, algorithms, smart pointers, lambdas, templates | hardware: `cxxtest` (139 checks) and a C++23 program |
| Newer C++ library features | `<format>`, `<print>`, `<ranges>`, `<expected>`, `<mdspan>`, `<flat_map>`, `<generator>`, `<coroutine>`, `<chrono>`, `<regex>`, `<filesystem>` | host: they compile and link. Their behaviour on RISC OS has not been tested (`<filesystem>` in particular depends on UnixLib's file system semantics) |
| Threads: `std::thread`, `std::mutex`, `std::condition_variable`, `std::async`, `std::call_once`, timed waits, `pthread` | threads live in UnixLib; no `-pthread` option | hardware: the thread test suite. `std::jthread`, `<latch>`, `<barrier>` and `<semaphore>` compile and link on the host only |
| `thread_local` / `__thread` | | hardware: a TLS test; host: compile and link |
| `std::random_device` | reads `/dev/urandom` (the CryptRand module), with a time-seeded fallback | hardware |
| Fortran 2018 (`gfortran`) | modules, derived types, calling C and being called from C (`iso_c_binding`), formatted, unformatted, stream and direct I/O, namelists, scratch files, `inquire`, runtime error exits | hardware: five programs of 122 + 54 + 26 + 30 + 9 checks, plus an error-path test |

## Code generation and optimisation

| Feature | Notes | Tested |
|---|---|---|
| `-O0`, `-O1`, `-O2`, `-O3`, `-Os` (GCC's standard levels) | | hardware: `-O0`, `-O2` and `-O3` in `rotest` and `cxxtest` |
| Default code: ARMv7-A, VFPv3, hard float | | hardware |
| Tuning for the Cortex-A72: `-mcpu=cortex-a72 -mfpu=neon-fp-armv8 -mfloat-abi=hard`; NEON | other `-mcpu`/`-mfpu` values are the stock GCC ones | hardware: A72 correctness and benchmark programs; host: flags accepted |
| **Stack probing on by default** (`-fstack-clash-protection`) | RISC OS maps stack pages when they are first touched, and loses some first touches; probing makes each function touch its pages with an ordinary store. Costs 1 to 4% (7% for code that is nothing but calls). Turn off with `-fno-stack-clash-protection`. | hardware |
| Position-independent code and shared libraries (`-fPIC -shared`), RISC OS GOT/PLT handling in `ld` | | hardware: a shared-library test; the dynamic libstdc++ and libgfortran |
| Debug information (`-g`) | no debugger is included | hardware: `-g` and `-g -flto` build, link and run, and the debug sections are in the program |

## Link time optimisation

| | Cross | Native |
|---|---|---|
| `-flto` (C, C++, Fortran), whole-program inlining across files | yes, through the **linker plugin**: archives (`.a`) are optimised too | yes, through **`lto-wrapper`**: objects are "fat" (they also link without LTO); objects inside an archive are **not** optimised across modules |
| `-flto=N`, `-flto=auto` | accepted | accepted; the jobs run one after the other |
| Tested | hardware: an LTO matrix of the C and C++ test programs | hardware: C, C++ and Fortran of two files, `-g -flto`, an archive in the link, `rotest` and `cxxtest` (the whole programs), zlib 1.3.1 (16 files in one link), and the stock `lto1` against the fixed runtime |

## Coverage and profile-guided optimisation

| | Cross | Native |
|---|---|---|
| `--coverage` (`-fprofile-arcs -ftest-coverage`): the program writes `name.gcda` when it ends; `gcov` reads it with `name.gcno` | yes: `arm-riscos-gnueabihf-gcov` on Linux reads the `.gcda` that the RISC OS program wrote | yes: `gcov` is part of the package |
| `-fprofile-generate`, then `-fprofile-use` (profile-guided optimisation) | yes | yes |
| Tested | hardware: programs built on Linux wrote their `.gcda` files on the Pi; the Linux `gcov` read one (real counts, branch percentages, the unexecuted line marked) and `-fprofile-use` on Linux accepted the profile of the other; host: the link of both flows (`cross-smoke.sh`) | hardware: the self-test compiles with `--coverage -c`, links and runs a program (it writes `cov.gcda`), runs `gcov cov.c` (80.00% of 15 lines) and checks every line count of the annotated source; then `-fprofile-generate`, a run, and `-fprofile-use -Werror=missing-profile` (the profile is found) |

The counts are written by an exit function of libgcov, which runs from the program's `.fini_array`: that needs the runtime 16.2.0-12 (earlier runtimes never ran the `.fini_array`). A program that ends with `_exit`, `abort` or a crash writes nothing, as with glibc.
How to use it: [USING-NATIVE.md](USING-NATIVE.md#coverage-and-profile-guided-optimisation) and [CROSS-COMPILER.md](CROSS-COMPILER.md#coverage-and-profile-guided-optimisation).

## Profiling with gprof (new in 16.2.0-13)

| | Native | Cross |
|---|---|---|
| `-pg`: the compiler counts every call (`push {lr}; bl __gnu_mcount_nc` after the prologue), and the program writes `gmon.out` when it ends | yes (the runtime 16.2.0-13) | yes (the program runs on RISC OS; `gmon.out` comes back to Linux) |
| `gprof`: the flat profile and the call graph | yes (`gprof` in the package) | yes (`arm-riscos-gnueabihf-gprof`) |
| The time | sampled 50 times a second by a thread that UnixLib starts behind `profil ()`; works in a Task window and in the desktop | the same |
| Tested | hardware: the self-test builds a `-pg` program with the native compiler, runs it (it writes `gmon.out`), runs the native `gprof` and checks that the report has exactly 10 calls for each of the program's two functions | hardware: a program built on Linux with `-pg` ran three times on the Pi: a valid `gmon.out`, the call counts equal to the program's own, 237 to 240 samples at 50 a second, the same profile from the Linux and the native gprof, about 2 percent overhead; host: `cross-smoke.sh` (the call, `gcrt0.o`, the link) |

The limits: a sample belongs to a function, not to a line (the processor takes the interrupt at a fixed place of a loop); only the program's own code is profiled, not time inside UnixLib or libstdc++; a tick that falls into a part of UnixLib that may not be interrupted (`malloc`, stdio) is lost; a program with threads of its own is sampled only at every other thread's turn (the profile has the right shape and too little time); the program must end by returning from `main` or by `exit`.
How to use it: [USING-NATIVE.md](USING-NATIVE.md#profiling-with-gprof) and [CROSS-COMPILER.md](CROSS-COMPILER.md#profiling-with-gprof).

## Native tools

| Feature | Notes | Tested |
|---|---|---|
| GNU make 4.4.1 | no shell: recipes cannot use pipes, redirections or `&&`; RISC OS commands work; `-jN` runs one job at a time | hardware: four make tests (make drives gcc, g++, the linker); the native gcc builds make from source and make rebuilds itself |
| Throwback, `-mthrowback` | errors, warnings and notes with a file and line go to the editor's throwback window through DDEUtils; C, C++, Fortran and LTO diagnostics; since 16.2.0-13 also those of the assembler and the linker (`as --throwback`, `ld --throwback`; the driver gives them the option); a file name with a path variable in front (`<Obey$Dir>.c.main`) works | hardware: StrongED (and a test receiver): the compilers, and the native assembler and linker (a double click on an entry opened the source); host: the text handling and the UDP transport of the assembler, the linker and the compilers |
| Real software built natively | | hardware: zlib 1.3.1 (by hand, by make, with `-flto`) and GNU make 4.4.1 itself; the objects are byte-identical to the cross compiler's |
| Big programs | `cc1`, `cc1plus` and `f951` run with 64 MB stacks, so deeply recursive templates and `constexpr` compile | hardware: template depth 100 to 1500 and 6000 nested parentheses |

## The runtime (UnixLib) fixes

15 fix levels, found by running real programs on real hardware: threads, `read()` into fresh stack buffers, `memcpy` on fresh stack pages, big main stacks, heap and `mmap` limits,
`vfork` + `exec` memory, the DDEUtils interaction, `scanf` with `long long`, the `.fini_array` of programs (destructors, the exit hook of libgcov), `getrlimit (RLIMIT_STACK)`, POSIX semaphores and the profiler of `-pg` programs (`gprof`). The list, with symptoms and the matching upstream bug report, is in [RUNTIME.md](RUNTIME.md).

## Experimental: modules

[modkit](MODULES.md) builds relocatable RISC OS modules with the cross compiler, without any C library: a module header generator compatible with CMHG input, a small C library, SWI veneers, vector and callback veneers, and the relocation step.
Hardware: a module with SWIs, a service call handler and static data, and a module that claims `TickerV` and uses callbacks, with `RMKill` and reload.

## Not included

OpenMP, the sanitizers, wide-character iostreams (`std::wcout`), `std::stacktrace`, `REAL(16)` in Fortran, multi-image coarrays, a debugger, OSLib and other RISC OS libraries,
and any machine other than the one it was tested on. Details and workarounds: [KNOWN-ISSUES.md](KNOWN-ISSUES.md).
