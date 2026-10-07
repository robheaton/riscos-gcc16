# Installing the native compiler on RISC OS

This page installs the compiler that runs **on RISC OS**. For the Linux cross compiler see [CROSS-COMPILER.md](CROSS-COMPILER.md).

## What you need

| | |
|---|---|
| **Machine** | A 32-bit ARM machine with an ARMv7-A (or later) processor and VFPv3, running RISC OS 5. Tested on a Raspberry Pi Compute Module 4 (Cortex-A72) with RISC OS 5.30. Other ARMv7 machines (Pi 2, 3, 4 ...) should work; they have not been tried. |
| **System modules** | The EABI support modules: *ARMEABISupport*, *Shared Object Manager* and *SharedUnixLibrary* 1.12 or later (`!GCC16` checks that one). The test machine had ARMEABISupport 1.08, Shared Object Manager 3.04 and SharedUnixLibrary 1.16, in `System:Modules`, and the system loaded them when the first EABI program ran. A machine without them has not been tried; these packages do not contain them. |
| **PackMan** | The RISC OS package manager, to install the zip files. |
| **Disk space** | 174 MB for the compiler (`Gcc16`, 70 MB to download), 6.9 MB for the C runtime, 1.8 MB and 1.2 MB for the optional C++ and Fortran runtimes (1 MB = 1,000,000 bytes). |
| **Memory** | The compiler needs a Task window (or other task) with an application space of **at least 48 MB**: the C++ compiler's program image is 30 MB and the driver that starts it is saved next to it. |

## 1. Get the files

Download from the [releases page](https://github.com/robheaton/riscos-gcc16/releases) (check them against `SHA256SUMS` if you can):

* `SharedLibs-C-armeabihf_16.2.0-13_arm.zip` (always needed)
* `Gcc16_16.2.0-14_arm.zip` (the compiler)
* `Gcc16SelfTest_16.2.0-14_arm.zip` (optional: a self-test of the installation, see section 3, "Check it")
* `SharedULibFix_1.16-vforkfix3_arm.zip` (optional: a fixed SharedUnixLibrary module, which **replaces a system module**: see "The fixed SharedUnixLibrary" below before you install it)
* `SharedLibs-C++-armeabihf_16.2.0-5_arm.zip` and `SharedLibs-Fortran-armeabihf_16.2.0-2_arm.zip` (optional: only for programs that link libstdc++ or libgfortran dynamically, which is the cross compiler's default; the native compiler links them statically)

Use a RISC OS browser, or download on another computer and copy the files over (USB stick, network share).
A package zip must have the file type **Zip (&A91)**. If it arrives as Text or Data, set the type in a Task window:

```
*SetType <path of the zip> &A91
```

(If you copy from Linux to a Samba share that RISC OS mounts, name the file `SharedLibs-C-armeabihf_16.2.0-13_arm.zip,a91`: the `,a91` becomes the file type.)

## 2. Install

1. **Drag `SharedLibs-C-armeabihf_16.2.0-13_arm.zip` onto the PackMan icon on the icon bar** and confirm the install.
2. **Drag `Gcc16_16.2.0-14_arm.zip` onto the PackMan icon** and confirm. PackMan insists on this order, because `Gcc16` depends on `SharedLibs-C-armeabihf` 16.2.0-13 or later.
3. Optionally do the same for the C++ and Fortran runtime zips.

   `SharedLibs-C-armeabihf` 16.2.0-13 **takes the place of** GCCSDK's own package of that name (10.2.0-1): it has the same set of files, with `libunixlib` and `libm` (UnixLib) rebuilt and fixed. Programs built with GCCSDK's 10.2.0 compilers keep working on it (checked with a C++ and a thread test).
   PackMan will not offer GCCSDK's older version as an upgrade over it.
4. **Reboot the machine.** PackMan replaces the files on the disk, but the library manager keeps the copy of UnixLib it has already loaded, so the old code keeps running until the machine restarts.
5. **Double-click `!GCC16`** (PackMan puts it in the `Apps.Utilities` directory of your Apps location). Its `!Run` sets up the search path (`Run$Path`), the return-code limit and the filename suffix swapping that the compiler relies on. These are system variables, so **do this again after every reboot** (or run `!GCC16.!Run` from your own boot sequence).

## 3. Check it

Open a Task window (Ctrl-F12) and type:

```
Echo <GCC16$Version>
gcc --version
```

You should see `16.2.0-14` and `gcc (GCCSDK GCC 16.2.0 (experimental forward-port)) 16.2.0`.

To check that the **loaded** UnixLib is the new one, compile and run the small probe [`tests/fixlevel/fixlevel.c`](../tests/fixlevel/fixlevel.c) (put it in a `c` directory as `c.fixlevel`):

```
gcc -O2 -o fixlevel fixlevel.c
fixlevel 14
```

It should print `fixlevel: libunixlib fix level 14` and `asked for 14: yes`. (`*Info` on the library file cannot tell the versions apart: it shows only the size in megabytes.)
Fix levels and what they mean are listed in [RUNTIME.md](RUNTIME.md).

To check the whole installation, install the optional `Gcc16SelfTest` package the same way (after `Gcc16`), open the folder that contains `!GCC16Test` once, and type in a Task window `Obey <GCC16Test$Dir>.RunSelfTest`. It compiles and runs twelve small tests (C, C++, Fortran, `make`, `-flto`, a compile error, coverage with `gcov`, profile-guided optimisation, `gprof`, and a module that is built, loaded and run) in about half a minute and ends with `SELFTEST: ALL CHECKS PASSED` ([tests/selftest](../tests/selftest/README.md)).

Then try the [first program](../README.md#quick-start-compile-on-risc-os), or read [USING-NATIVE.md](USING-NATIVE.md).

## The fixed SharedUnixLibrary (optional)

The stock SharedUnixLibrary 1.16 that RISC OS ships has bugs that show when a program starts a child with `vfork` and the child ends without `exec` (the parent dies, or in a bad case the machine freezes). Compiles and `make` runs do not meet them. If you run programs that do, the optional package `SharedULibFix` has the fixed module with an installer that replaces the system module only when it is exactly the stock 1.16, backs it up twice, checks every copy and can put the stock module back. Installing the package changes nothing until you run its `Install`. Read [SHAREDULIB-FIX.md](SHAREDULIB-FIX.md) first.

## Upgrading, going back, removing

* **Upgrade:** drag the newer zips onto PackMan the same way (runtime first), then reboot.
* **Go back:** install the older package from its release (remove the newer one first in PackMan if it refuses to go back), then reboot. `Gcc16` 16.2.0-14 needs a runtime of 16.2.0-13 or later (16.2.0-13 needs 16.2.0-13 or later; `Gcc16` 16.2.0-8 to -11 need 16.2.0-10 or later).
* **Remove:** remove `Gcc16` in PackMan. `SharedLibs-C-armeabihf` is the runtime of **every** program built by this tool chain (and of other EABI programs): remove it only if nothing needs it.

## If something goes wrong

| Symptom | Cause and fix |
|---|---|
| `gcc` (or `make`, `ld` ...) is not found, or `GCC16$Version` is not set | Double-click `!GCC16` (after every reboot), then try again. |
| A program says `can't load library 'libstdc++.so.6'` or `'libgfortran.so.5'` | The program was linked dynamically (the cross compiler's default). Install the C++ or Fortran runtime package, or link statically (see [CROSS-COMPILER.md](CROSS-COMPILER.md)). |
| The old behaviour comes back after an install | Reboot. Also, the **!Iris** browser carries its own SharedLibs and redirects `SharedLibs:` to them when it runs: two sets cannot both be in use, so reboot after running it before using this tool chain. |
| `Unable to allocate logical address space` | A program asked for more address space than is free (it reserves a heap of up to 512 MB, which is only used as needed). Runtimes from 16.2.0-6 retry with half the size by themselves; with an older one set a lower size, e.g. `SetEval cc1plus$HeapMax 256` (MB). |
| The compilers do not run while a text editor such as StrongED is running (arguments cut off) | Your runtime is older than 16.2.0-10: update `SharedLibs-C-armeabihf` (the editor loads the DDEUtils module, which exposed a bug fixed in 16.2.0-10). |
| `abort on data transfer` after a C++ compile | Your runtime is older than 16.2.0-9: update it. |
| A program built with `--coverage` or `-fprofile-generate` runs but leaves no `.gcda` file, or a program's destructors never run | Your runtime is older than 16.2.0-12: update it (earlier runtimes never ran the `.fini_array` of a program). |
| A program built with `-pg` fails to link (`undefined reference to __gnu_mcount_nc`) or runs but leaves no `gmon.out` | Your runtime is older than 16.2.0-13 (the cross compiler's own libraries are the new ones; on RISC OS install the new runtime and reboot) |

More: [KNOWN-ISSUES.md](KNOWN-ISSUES.md). Bug reports and questions: [GitHub issues](https://github.com/robheaton/riscos-gcc16/issues); please include `Echo <GCC16$Version>`, the output of `fixlevel`, your RISC OS version and the command that failed.
