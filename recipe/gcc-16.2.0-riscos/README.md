# GCC 16.2.0 for RISC OS (arm-riscos-gnueabihf) -- forward port of the GCCSDK 10.2.0 EABI toolchain

Experimental.  Everything here was built and tested on one machine (Ubuntu 26.04, host GCC 15.2, 22 cores) and the results were run on a
real RISC OS machine (Cortex-A72).  This file describes the recipe; the user documentation is in `docs/` (start with the top-level `README.md`), the build instructions in `docs/BUILDING.md`.
Nothing under `~/gccsdk` is modified by any of this: it is only read (UnixLib sources, the 10.2.0 install used as sysroot, GMP/MPFR/MPC).

## What is here
    patches/            GCC core: arm backend, config.gcc, dwarf2cfi, ira, libgcc, libgcov, and -fstack-clash-protection on by default (10 patches, ~270 lines)
    new-files/          the RISC OS target files (gcc/config/arm/riscos-elf.h, riscos.opt, t-riscos..., libgcc crt files)
    patches-cxx/        libstdc++ (8 patches: crossconfig, cmath long-double guards, EH personality, timed-wait and hardware_concurrency fixes)
    new-files-cxx/      libstdc++ os/riscos glue
    patches-unixlib/    UnixLib 5.0 fixes (applied by scripts/build-unixlib.sh to a copy; the originals in ~/gccsdk are never touched)
    patches-native/     the compiler as a program that runs ON RISC OS: host config, libiberty (response files, RAM size), libcpp (see "The native compiler" below)
    new-files-native/   the driver hooks of that compiler (riscos-gcc.c)
    data/               riscos-da.c (every native program's heap in a dynamic area) and nolibm/libm.so (an empty libm)
    scripts/            apply / configure / make / package scripts (see below)
    tests/              rotest (C, ~34.5k checks), cxx (C++ suite, threadtest), bench, shlib, unixlib-fix (host models), fortran (see tests/fortran)

## Reproduce (about 15 minutes on 22 cores)
Needs: the binutils 2.45.1 port installed (`recipe/binutils-2.45.1-riscos`), a GCCSDK 10.2.0 install (for the UnixLib headers/libs used as
sysroot), autoconf 2.69 as `autoconf2.69` (regenerates libstdc++-v3/configure), GMP/MPFR/MPC (`scripts/build-host-prereqs.sh`).

    tar -xf gcc-16.2.0.tar.xz                                   # https://ftp.gnu.org/gnu/gcc/gcc-16.2.0/
    scripts/apply-port.sh      gcc-16.2.0                       # patches/ + new-files/
    scripts/apply-port-cxx.sh  gcc-16.2.0                       # patches-cxx/ + new-files-cxx/ + regenerate libstdc++-v3/configure
    scripts/prepare-sysroot.sh PREFIX                           # UnixLib/OSLib headers + libs from the 10.2.0 install, binutils 2.45.1 symlinks
    scripts/configure-gcc16-full.sh gcc-16.2.0 BUILD PREFIX     # C, C++, Fortran, LTO; shared libgcc/libstdc++/libgfortran
    scripts/make-gcc16-full.sh BUILD PREFIX all
    scripts/make-gcc16-full.sh BUILD PREFIX install
    export PATH=PREFIX/bin:$PATH ; arm-riscos-gnueabihf-gcc -O2 hello.c -o hello,e1f

The early step-by-step scripts of the bring-up (C only, then C++, then LTO) are not published: `configure-gcc16-full.sh` does everything.

## The native (RISC OS-hosted) compiler  (experimental)
GCC 16.2.0 as programs that run ON RISC OS -- `gcc`, `g++`, `cpp`, `cc1`, `cc1plus`, `collect2`, and binutils 2.45.1 `as`, `ld`, `ar`, `nm`, ... -- cross-built on Linux with the
cross compiler above (host = target = arm-riscos-gnueabihf; the driver then uses the native branch of `riscos-gnueabihf.h`).  Hardware-proven on the Cortex-A72 test machine
(2026-10-03): it compiles, assembles, links and runs the 34541-check C regression suite and the 139-check C++ suite (the objects are byte-identical to the cross compiler's).

    scripts/build-native-prereqs.sh                  # GMP/MPFR/MPC for RISC OS (hostlibs-riscos)
    scripts/prepare-native-src.sh                    # a fresh source tree from the pristine tarball: apply-port, apply-port-cxx, apply-port-native (patches-native/, new-files-native/)
    scripts/build-native-all.sh O2                   # about 13 minutes: cc1, cc1plus, gcc, g++, cpp, collect2, `make install-gcc` into native-stage2-O2.  `Os` builds a 21% smaller
                                                     # compiler (cc1plus 24 MB instead of 30) that compiles 7-28% slower (measured on the machine); the generated code is identical
    ../binutils-2.45.1-riscos/scripts/build-binutils-native.sh SRC BUILD PREFIX     # as, ld, ar, nm, objdump, ... about a minute
    scripts/make-native-tree.sh OUTDIR               # OUTDIR/gcc16: the tree (stripped, executables named ,e1f for a Samba share)

What the native build needed, each learned on the hardware:
  * `patches-native/gcc.config.host.patch`, `gcc.config.gcc-native-objs.patch` (driver hooks object, `NATIVE_SYSTEM_HEADER_DIR`/`STANDARD_INCLUDE_DIR` relative to the prefix), `libcpp.off_t.patch`
    (`off_t*` vs `__off_t*`), `new-files-native/.../riscos-gcc.c` (the driver finds its files relative to the place of `gcc`; `riscos_multilib_dir` adds `-L/SharedLibs:lib.armeabihf`).
  * `libiberty.pex-unix-riscos-response-file.patch`: a command line of 1024+ characters needs the DDEUtils module (UnixLib exec -> set_dde_cli), which may not be loaded, and the failure
    destroys the parent; argument lists of 700+ characters are passed in an `@file` (gcc, cc1, collect2, as and ld all read them).
  * `data/nolibm/libm.so`: an EMPTY libm.so first in the link path.  libm.so.1 is an empty stub (the math functions are in libunixlib), but a program with it in DT_NEEDED hangs or aborts
    as a vfork child, and every compiler pass is one.
  * `data/riscos-da.c` (linked into every native program through LDFLAGS): the HEAP IN A DYNAMIC AREA.  A UnixLib program keeps its heap in the Wimp slot unless `<program>$Heap` is set; a
    vfork+exec child's heap then grows over the copy of the parent that SharedUnixLibrary keeps at the top of the slot, and the parent dies when the child exits (the C++ driver, after a
    big cc1plus compile: "abort on data transfer").  See `docs/upstream/08-UnixLib-vfork-exec-child-heap-grows-over-saved-parent.txt`.  `<program>$HeapMax` (MB) overrides the 512 MB.
  * `patches-native/libiberty.physmem-riscos.patch`: UnixLib's `sysconf (_SC_PHYS_PAGES)` answers 0, so GCC's garbage collector ran at its minimum heap sizes; the RAM size now comes from OS_ReadMemMapInfo.
  * binutils `patches/04-bfd-riscos-elf-filetype.patch`: `ld` (and objcopy/strip) give an ELF executable or shared object the file type &E1F (otherwise it is Text and cannot be run).
  * LINK TIME OPTIMISATION (`-flto`, Gcc16 16.2.0-9 .. -11): the native `ld` has no plugin support (`HAVE_LTO_PLUGIN 0`), so `-flto` writes fat objects and `collect2` runs `lto-wrapper` at the link
    (objects in an archive are not optimised across modules).  Three programs differ from stock GCC for this: `patches-native/gcc.collect2.lto-wrapper.riscos-list-file.patch` (collect2 started lto-wrapper with its stdout redirected to a temp
    file; under UnixLib the whole chain's messages (stderr is a dup of stdout) then ended up in that file, which collect2 reads as a list of file names and unlinks: the failure was SILENT, and with `-v` the unlinking of ~70 long lines
    probably broke a LanMan98 share.  Now the list goes through a file named by `COLLECT_LTO_OUTPUT_LIST`; and lto-wrapper runs the LTRANS jobs serially: a process cannot run alongside another inside a task, and its makefile's recipe starts with the
    `$` of `/NVMe::NVMe.$/...`, which make reads as a variable), and `patches-native/gcc.lto.lto-common.riscos-section-id.patch` (lto1 reads the 64 bit id of its section names with `sscanf (".%llx")`, which UnixLib's scanf could not do: see
    below; `strtoull` instead).
  * UnixLib's `scanf` had no long long conversions (`patches-unixlib/unixlib-scanf-long-long.patch`, libunixlib 16.2.0-11 = fix level 13, upstream report 21): `%llx` was converted by strtoul and stored as a long (a 16 digit id gave 0xffffffff in the low
    word, the high word untouched), `%hhd` stored a short, `%jd %zu %td %qd` were not understood, `%Lf` stored a float.  That was the ROOT CAUSE of every LTO link failing ("bytecode stream ... generated with GCC compiler older than 10.0"); with the
    fixed library the UNPATCHED lto1 links (RunLto6 step 1).  The lto-common patch above stays as a safety net for the older libraries (Gcc16 -10 and -11 both run on libunixlib 16.2.0-10 or later).

Running it: the tree is Unix-named, so switch UnixLib's filename suffix swapping off per program (`Set UnixEnv$gcc$sfix xyzzy`, same for g++ cpp cc1 cc1plus collect2 as ld ar ...), `Set Sys$RCLimit 65536`,
and give the task an application space of at least 48 MB (`WimpSlot -min 48M -max 48M`: cc1plus is 30 MB and the vfork saves the driver next to it).  Stack: the main stack of an EABI
program is 1 MB unless the program defines `__stack_size` (runtime 16.2.0-6 or later): `cc1`, `cc1plus` and `f951` ask for 64 MB and `make` for 8 MB, so deep template recursion works. `data/riscos-da-big.c` gives the binutils programs 8 MB too when they are built with it (`build-binutils-native.sh` does); the binutils in the released package were built before that was added and have the default 1 MB.  The tests are in `tests/` (see `tests/README.md`).

## Throwback (`-mthrowback`)
The option of the GCCSDK compilers, accepted and ignored by the port until 2026-10-04, now works: every diagnostic that has a file and a line (errors, warnings, notes, fatal errors; C, C++, Fortran, LTO) is ALSO sent to a RISC OS
text editor.  `new-files/gcc/config/arm/riscos-throwback.cc` is an additional output sink of GCC 16's diagnostic machinery (`diagnostics::sink`), added by `arm_option_override` when the option is given
(`patches/gcc.config.arm.arm.cc.throwback.patch`; `riscos-throwback.o` in `extra_objs`, rule in `t-riscos-gnueabihf`); the text on stderr does not change and the generated code is identical (37 C / C++ and 14 Fortran
objects compared byte for byte with and without the patch).  Two transports, chosen at build time: the NATIVE compiler (CROSS_DIRECTORY_STRUCTURE not defined) calls the SWIs of the DDEUtils module (ThrowbackStart,
ThrowbackSend reasons 0 / 1 / 2, ThrowbackEnd; the protocol is that of DDEUtils 1.75, read from its ROOL source); a CROSS compiler sends one syslog UDP datagram per message to `$THROWBACK_HOST` (port 514 or `$THROWBACK_PORT`),
which the GCCSDK module SysLogD turns into DDEUtils throwback.  `THROWBACK_DEBUG` (any value) says on stderr why throwback does not work (no DDEUtils, no editor registered, not in the desktop ...).  A Fortran peculiarity
is handled (the front end's %C / %L formats are not repeatable for a second sink: they come out as "(2)").  Tests: `tests/throwback` (host: `build-and-run.sh`: the text cutting and both transports, 66 checks, 27 mutations
of the source all caught; `hw/`: the programs of the hardware pack `throwback22`: tbprobe, tbtest (the same source as a program), tbsink (a Wimp receiver), build-ddeutils.sh (DDEUtils 1.75 built from the RISC OS sources with asasm)).

## Coverage and profile-guided optimisation (16.2.0-12)
`--coverage`, `gcov` and `-fprofile-generate` / `-fprofile-use` needed two repairs, one in the compiler's libraries and one in UnixLib:
  * `patches/gcc.libgcc.libgcov-with-libc.patch`: the cross compiler is configured without `--with-sysroot` or `--with-headers`, so GCC's build compiles all of libgcc with `-Dinhibit_libc` (`INHIBIT_LIBC_CFLAGS` in
    `gcc/libgcc.mvars`), which for libgcov means "no C library": the `.a` had no `__gcov_exit`, `__gcov_dump` and the file functions, and every `--coverage` link failed. libgcov is now compiled without that flag (the port's target
    has UnixLib, and the headers are there when libgcc is built).  The native compilers' `libgcov.a` is the cross build's.
  * `patches-unixlib/unixlib-fini-array.patch` (fix level 14, upstream report 22): UnixLib never ran the `.fini_array` of a program, and the exit function of libgcov is a static destructor that GCC puts into every instrumented object
    (`coverage.cc`, `build_gcov_exit_decl`), so no `.gcda` file was ever written. `__main` now registers one `atexit ()` function that runs the array, last entry first, after the program's own `atexit ()` functions (glibc's order).
The `.gcda` file of a cross-built program is named with the absolute Linux path it was compiled at: `GCOV_PREFIX` and `GCOV_PREFIX_STRIP` redirect it ([docs/CROSS-COMPILER.md](../../docs/CROSS-COMPILER.md#coverage-and-profile-guided-optimisation)).
`gcov` is one of the programs of the native tree (`scripts/make-native-tree.sh`) and of the cross tarball.  `gprof` (`-pg`) is not done: UnixLib's profiler (`gmon/_profile.s`) is compiled out for EABI, takes over the IRQ vector, and `STARTFILE_SPEC` links
`crt0.o` also for `-pg`.

## Stack probing (`-fstack-clash-protection` is the default)
The stack of an EABI program (1 MB, made by ARMEABISupport) is mapped one 4 KB page at a time, from a data abort handler, when a page is first touched.  On the Cortex-A72
machine this was tested on, that goes wrong when the first touch of a page is not an ordinary store: RISC OS itself (supervisor mode: OS_GBPB filling the buffer of `read()`, OS_GSTrans, ...)
loses the store that takes the page fault, and a 64-byte aligned `vstmia {d0-d7}` (what UnixLib's NEON `memcpy` uses) ends in SIGSEGV.  Touching every page of a stack frame with an ordinary
store when the function starts avoids both, whatever the program does, so the port turns `-fstack-clash-protection` on by default for arm-riscos (`patches/gcc.config.arm.arm.cc.stack-clash-default.patch`,
in `arm_option_override`; `-fno-stack-clash-protection` turns it off again).  Cost: three instructions (`mov ip,#4096; sub ip,sp,ip; str r0,[ip,#N]`) in every function that has a stack
frame and is not a leaf, a probe loop for big frames and `alloca`; measured in `tests/bench/callbench.c` (worst case: nothing but calls).  Everything in the runtime packages 16.2.0-5 (libunixlib,
libstdc++, libgfortran) is built with it, and `scripts/build-unixlib.sh` refuses to build UnixLib with a compiler that does not have it.  Test programs that DEPEND on touching fresh stack pages
(`tests/unixlib16/readtest*.c`, `ulinfo.c`) switch the probes off for themselves.  Details, the hardware measurements and the open question why the OS misbehaves: `docs/upstream/07-RISCOS-svc-mode-store-to-unmapped-stack-page-lost.txt`.

## UnixLib
`scripts/build-unixlib.sh` rebuilds UnixLib 5.0 with the new compiler (out of tree, from a `cp -rL` copy of the 10.2.0 sources) and applies
`patches-unixlib/`: GCC 14+ implicit-declaration fixes, `pthread_once` (no global lock across the init routine; exception-safe; compiled with
-fexceptions), `pthread_cond_timedwait` (centisecond-accurate deadlines), `sleep`/`usleep`/`nanosleep` with several threads, the
`_SC_NPROCESSORS_ONLN` enum fix, the `.fini_array` (`unixlib-fini-array.patch`), `getrlimit (RLIMIT_STACK)` (`unixlib-getrlimit-stack.patch`: the EABI main stack is fixed, so that is the limit), POSIX semaphores (`unixlib-sem-blocking-timedwait.patch`: a mutex and a condition variable instead of polling, a real `sem_timedwait`; `unixlib-semaphore-timedwait-decl.patch` declares it, also in the cross sysroot) and `fread()`/`fwrite()` (after a short `read()`/`write()` the direct-transfer loops never advanced the data pointer: right byte count,
wrong data; an upstream UnixLib bug; the fread/fwrite fix is only in the GCC-16-built package) and `read`/`fread`/`recv`/`recvfrom` touching a stack buffer first (`unixlib-touch-stack-buffers.patch`: RISC OS loses the store that first touches a lazily mapped EABI stack page; see `docs/upstream/15-UnixLib-touch-stack-buffers-before-handing-them-to-the-OS.txt`), and `memcpy`/`memmove` without the 64-byte store-multiple (`unixlib-memcpy-split-vstm.patch`: each `vstm` of 8 d registers becomes two of 4).  The runtime packages 10.2.0-N carry a libunixlib built by the OLD compiler
(GCC 10.2/binutils 2.30) so that every behavioural difference comes from the source changes alone;
16.2.0-N carry one built by the new compiler (`make-c16-package.py`; hardware-proven with 16.2.0-1 .. -4: -2 adds the stdio fix, -3/-4 the stack-buffer touch and the fix level; 16.2.0-5 adds the split memcpy and is itself built with the probing default, `tools/check-libunixlib.sh` verifies every fix in the built library).

## Running things on RISC OS
Test programs and Obey files for a real machine are in `tests/` (see `tests/README.md`); they all follow the pattern
`Set RoTest$Dir <Obey$Dir>` first (the first EABI program changes `<Obey$Dir>`), `Spool` to a results file, and a SUMMARY line per program.
