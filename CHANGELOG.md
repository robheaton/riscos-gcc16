# Changelog

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
