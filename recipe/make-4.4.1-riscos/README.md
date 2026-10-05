# GNU make 4.4.1 for RISC OS (arm-riscos-gnueabihf)

Cross-built with the GCC 16 EABI tool chain, to run natively on RISC OS next to the native compiler (`bin/make` of the `Gcc16` package).

    scripts/build-make.sh [TARBALL] [WORKDIR]     # make-4.4.1.tar.gz (https://ftp.gnu.org/gnu/make/) -> WORKDIR/install/bin/make (stripped, 200 KB)

What matters (all hardware-proven on a Cortex-A72, 2026-10-03):
* make 4.4.1 already supports `__riscos__`: no shell is used (every recipe line is split into words and exec'd directly; UnixLib's `execve` makes a RISC OS command line of it, so programs
  are found through `Run$Path` and RISC OS commands such as `Echo` work; no pipes, redirections or `&&` in recipes).
* `patches/01-default-cc-gcc.patch`: the default C compiler is `gcc` (there is no `cc`).
* `-std=gnu17`: gnulib's fnmatch.c has `extern char *getenv ();`, which GCC 15+'s default (gnu23) rejects.
* `--disable-job-server`: `-jN` cannot run anything in parallel (a vfork child runs to completion before vfork returns) and the job server's blocking token pipe would be a deadlock risk;
  `--disable-posix-spawn` keeps make on vfork.
* Linked like the native compiler: the empty libm placeholder and `riscos-da.o` (heap in a dynamic area, UnixLib's default maximum of 32 MB: make, gcc, collect2 and ld are alive together
  and the heap maxima, which are only reserved address space, add up).
* UnixLib's `readdir` presents the files of the c/h/o suffix directories as `main.c`, `util.h`, ..., which make's directory cache needs: makefiles use Unix names.
* Tests (NAS `Development/GCC/tests/native3/`): `maketest` (RunMake1-4), `zlibtest` (RunZlib2: zlib built by make), `makebuild` (RunMakeBuild: the native gcc builds make, and make rebuilds make;
  the objects and the loadable sections of the result are identical to the cross build).
