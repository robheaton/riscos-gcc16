# Self-test for the native compiler

Checks a fresh installation of the native GCC 16 tool chain on RISC OS in about half a minute. It compiles and runs

1. a probe of the runtime (UnixLib **fix level 15**),
2. a C program,
3. a C++ program with an exception, a thread and `std::async`,
4. a Fortran program,
5. a two-file project, then `ar` and `nm`,
6. a build with **GNU make**,
7. two **`-flto`** builds (one with `-flto=2`),
8. a file with an error, which must be **reported** (a non-zero return code),
9. a **coverage** run: `--coverage`, the program writes its counts when it ends, `gcov` annotates the source, and a small program checks the counts,
10. a **profile-guided** build: `-fprofile-generate`, run, then `-fprofile-use` (which must find the profile).
11. a **gprof** run: `-pg`, the program writes `gmon.out` when it ends, `gprof` reads it, and a small program checks the call counts of its report.
12. a **module** (new in 16.2.0-14): `cmunge` makes the header and veneers of a CMHG file, `gcc -mmodule` compiles and links it (the driver runs `modreloc`, so the output is the module image), a small program checks the image, the module is loaded with `RMLoad`, its command `*ModHello_Sum 2 3` prints 5, and the module is removed again.

Every program checks itself and returns 0 only when it is right, so a `PASS` means the compiler produced a program that ran correctly.
On the test machine (Raspberry Pi Compute Module 4, RISC OS 5.30) it passes all twelve checks in about half a minute.

## Get it

**The easy way: the `Gcc16SelfTest` package** (on the [releases page](https://github.com/robheaton/riscos-gcc16/releases), next to the compiler). Drag `Gcc16SelfTest_16.2.0-19_arm.zip` onto the PackMan icon like the others; it needs `Gcc16` 16.2.0-17 (`RunSelfTest` stops with a message if the installed `Gcc16` is another release; the module check uses `cmunge` and `gcc -mmodule`) and installs `!GCC16Test`.

Or copy this folder to RISC OS yourself: the files are in the RISC OS layout (directories `c`, `cc`, `f90` and `h`; `RunSelfTest,feb` is the Obey file: on RISC OS it must have the file type Obey, `*SetType RunSelfTest Obey`).

## Run it

1. Install the packages ([INSTALL-RISCOS.md](../../docs/INSTALL-RISCOS.md)), **reboot**, and **double-click `!GCC16`**.
2. Open the folder that contains `!GCC16Test` once (the Filer then sets `GCC16Test$Dir`).
3. Open a Task window (Ctrl-F12) and type:

   ```
   Obey <GCC16Test$Dir>.RunSelfTest
   ```
   (or `Obey <path of this folder>.RunSelfTest` if you copied it by hand).

The work is done on the local disc, in `<Wimp$ScrapDir>.GCC16Test`, so the folder you start it from is not cluttered; the output is also written to `Results` next to `RunSelfTest`.

## What you should see

```
PASS: compile fixlevel.c
PASS: the runtime is at fix level 15
PASS: compile hello.c
PASS: run hello
...
PASS: the error in bad.c was reported (return code 1)
SELFTEST: ALL CHECKS PASSED
```

Between the lines you also see the programs' own output (`hello from C`, `lto: 42`, `2 + 3 = 5, 10 squared = 100`, the error messages of `bad.c`, a warning about serial compilation for `-flto=2`, the percentages that `gcov` prints, `covcheck`'s line about the counts).
If something fails, the last line lists the failed checks (`SELFTEST: FAILED: fixlevel-run ...`): see [INSTALL-RISCOS.md](../../docs/INSTALL-RISCOS.md#if-something-goes-wrong) and send the `Results` file with a bug report.
