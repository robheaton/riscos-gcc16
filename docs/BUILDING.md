# Building everything from source

You only need this to change the tool chain or to rebuild it. To **use** it, install the [packages](INSTALL-RISCOS.md) or the [cross compiler tarball](CROSS-COMPILER.md).

> **Status of these instructions.** They are the scripts that built the released binaries, on one machine (Ubuntu 26.04, host GCC 15.2, 22 cores) in September and October 2026.
> Before the release every command on this page was run again, in order, from a fresh copy of this repository in an empty home directory (same machine; the GCCSDK of step 1 was reused). All eight steps took about 33 minutes. The checks at the end of step 8 stopped the run once, because one of their patterns did not allow for the form of path that a relocated tool chain prints (the compiler was right); they passed when the pattern was corrected and the step was run again.
> **The `Gcc16` and `Gcc16SelfTest` packages and the Linux tarball on the releases page are the output of that run**, and the two packages are the files that were installed and tested on the Raspberry Pi. Compared with the first test build of the same sources, 1104 of the 1116 files of the `Gcc16` package are byte for byte the same; the others are the 16-byte checksums inside `cc1` and `cc1plus` and the timestamps inside static libraries. Every file of the C runtime package of step 5 came out byte for byte the same as in the published 16.2.0-13 package. The C++ and Fortran runtime packages are not rebuilt for this release: today's compiler puts a stack probe in a few small functions (the constructors of `std::ios_base::failure`, for example), so a rebuild gives libraries that behave the same but are not byte for byte the published ones. The run passed the checks of the steps: the library check of step 5 (the profiler routines among them) and [`cross-smoke.sh`](../tests/cross-smoke/cross-smoke.sh) on the cross compiler of step 4 and on the unpacked tarball of step 8 (48 checks, four of them compare its programs, byte for byte, with those of step 4's compiler; the other 44 were also run with the directory the tarball was built in hidden from it).
>
> Every binary contains the path it was built in (assertion messages, debug information), so a rebuild in another directory differs in those strings. The least reproducible part is step 1 (GCCSDK itself).

## What gets built

```
GCC 16.2.0 + binutils 2.45.1  --->  cross compiler (Linux, x86-64)  --->  UnixLib 5.0 rebuilt (libunixlib, libm)
                                          |                                      |
                                          +--->  libstdc++, libgfortran         +--->  SharedLibs-C-armeabihf package
                                          |
                                          +--->  the native compiler (cc1, cc1plus, f951, lto1 ... for RISC OS)  +  native binutils  +  GNU make
                                                                                        |
                                                                                        +--->  Gcc16 package (!GCC16)
```

## Layout the scripts assume

The scripts take their defaults from two directories under your home directory. **Clone this repository as `~/gccsdk-next`** (every script finds its patches relative to its own place, but they put work directories next to it):

| Path | What |
|---|---|
| `~/gccsdk-next` | this repository; the scripts also create `src/`, `build*/`, `env-f/`, `hostlibs*/`, `binutils-*-install/`, `unixlib/`, `native-*` and `release/` here (all in `.gitignore`) |
| `~/gccsdk` | GCCSDK: an svn checkout of trunk r7800 **and** a built GCCSDK 10.2.0 EABI tool chain (see step 1) |

```bash
git clone https://github.com/robheaton/riscos-gcc16 ~/gccsdk-next
```

Host tools: a C and C++ compiler (GCC 13 or newer; 15.2 was used), `make`, `patch`, `autoconf2.69` (the binary must be called `autoconf2.69`; it regenerates libstdc++'s `configure`), `python3`, `rsync`, `xz`, `tar`, `file`, `subversion` (to fetch GCCSDK).

## 0. The upstream sources

Put these in `~/gccsdk-next/src/` (URLs and checksums in [SOURCES.md](../SOURCES.md)): `gcc-16.2.0.tar.xz`, `binutils-2.45.1.tar.xz`, `make-4.4.1.tar.gz`, and extract `gmp-6.1.0`, `mpfr-3.1.4` and `mpc-1.0.3` into `~/gccsdk-next/src/prereq/` (the versions GCCSDK's own recipe uses).

## 1. The GCCSDK seed

The recipe reads two things from a GCCSDK 10.2.0 EABI build: the **sysroot pieces** (UnixLib and OSLib headers, `crt0.o`, the dynamic loader and the 10.2.0 `libunixlib`/`libdl`, from `~/gccsdk/env`) and the **UnixLib source tree with its generated build files** (`~/gccsdk/build/gcc/gcc-10.2.0/libunixlib`).
Build GCCSDK as its own instructions say ([GCCSDK on riscos.info](https://www.riscos.info/index.php/GCCSDK)); the author used svn trunk r7800 (`svn://svn.riscos.info/gccsdk/trunk`) and its autobuilder recipe for GCC 10.2.0. This is the least reproducible step.
Two packages made by GCCSDK's autobuilder are also read, from `~/gccsdk/autobuilder/autobuilder_packages/arm/Development/`: `SharedLibs-C-armeabihf_10.2.0-1_arm.zip` (the base of the C runtime package: its loader, `libgcc_s` and `libdl` are used unchanged) and `gcc_10.2.0-1_arm.zip` (the icon sprites of `!GCC16`).
Nothing under `~/gccsdk` is modified by the scripts here.

## 2. binutils 2.45.1 (cross)

```bash
cd ~/gccsdk-next/src && tar -xf binutils-2.45.1.tar.xz
~/gccsdk-next/recipe/binutils-2.45.1-riscos/scripts/apply-port.sh  ~/gccsdk-next/src/binutils-2.45.1
~/gccsdk-next/recipe/binutils-2.45.1-riscos/scripts/build-binutils.sh ~/gccsdk-next/src/binutils-2.45.1 ~/gccsdk-next/build-binutils ~/gccsdk-next/binutils-2.45.1-install
```

The RISC OS changes (5 patches) are in `recipe/binutils-2.45.1-riscos/patches/`: the EABI/RISC OS ELF handling in BFD, gas and ld, the ELF file type &E1F for programs and, since 16.2.0-13, `--throwback` for gas and ld. gprof is built too (`arm-riscos-gnueabihf-gprof`).

## 3. The host libraries

```bash
~/gccsdk-next/recipe/gcc-16.2.0-riscos/scripts/build-host-prereqs.sh       # static GMP, MPFR, MPC for the Linux host -> ~/gccsdk-next/hostlibs
```

## 4. The GCC cross compiler (about 13 minutes on 22 cores)

```bash
R=~/gccsdk-next/recipe/gcc-16.2.0-riscos/scripts
cd ~/gccsdk-next/src && mkdir -p clean && tar -xf gcc-16.2.0.tar.xz -C clean
$R/apply-port.sh         ~/gccsdk-next/src/clean/gcc-16.2.0      # patches/ + new-files/: the RISC OS target (backend, libgcc, throwback, stack probing by default)
$R/apply-port-cxx.sh     ~/gccsdk-next/src/clean/gcc-16.2.0      # libstdc++ (regenerates its configure)
$R/apply-port-fortran.sh ~/gccsdk-next/src/clean/gcc-16.2.0      # libgfortran: no shared-memory coarray library
$R/prepare-sysroot.sh    ~/gccsdk-next/env-f                     # the install prefix, with the GCCSDK sysroot pieces and the binutils entries
$R/configure-gcc16-full.sh ~/gccsdk-next/src/clean/gcc-16.2.0 ~/gccsdk-next/build-f ~/gccsdk-next/env-f     # C, C++, Fortran, LTO; shared libgcc, libstdc++, libgfortran
$R/make-gcc16-full.sh ~/gccsdk-next/build-f ~/gccsdk-next/env-f all
$R/make-gcc16-full.sh ~/gccsdk-next/build-f ~/gccsdk-next/env-f install
export PATH=~/gccsdk-next/env-f/bin:$PATH
~/gccsdk-next/tests/cross-smoke/cross-smoke.sh ~/gccsdk-next/env-f
```

`recipe/gcc-16.2.0-riscos/README.md` describes what each patch and new file does.

## 5. UnixLib (about 15 seconds)

```bash
$R/build-unixlib.sh                                              # UnixLib 5.0 from the GCCSDK tree + patches-unixlib/ -> ~/gccsdk-next/unixlib/build/.libs/libunixlib.so.5.0.0
~/gccsdk-next/tools/check-libunixlib.sh ~/gccsdk-next/unixlib/build/.libs/libunixlib.so.5.0.0
$R/install-unixlib-sysroot.sh ~/gccsdk-next/env-f ~/gccsdk-next/unixlib/build   # the cross compiler links against this UnixLib from now on (and uses its crt0.o and gcrt0.o)
$R/install-modkit.sh ~/gccsdk-next/env-f                                 # gcc -mmodule and cmunge: libmodkit.a, module.ld, the headers and scripts of modkit (see MODULES.md)
```

`install-modkit.sh` builds `libmodkit.a` with the compiler of step 4 (`-mmodule`) and puts it, the linker script, the headers and the scripts into the tool chain; it is part of the tarball of step 8.

`build-unixlib.sh` refuses to build with a compiler that does not have stack probing on by default, and a patch that does not apply is a hard error. `check-libunixlib.sh` looks for the code of every fix in the built library.
(`libunixlib.a` is built too, for `-static`; the SharedUnixLibrary module is built separately by `build-sul.sh`.)

## 6. The runtime packages and the fixed SharedUnixLibrary

```bash
PKG_OUT=~/gccsdk-next/release python3 $R/make-c16-package.py 13 ~/gccsdk-next/unixlib/build               # SharedLibs-C-armeabihf 16.2.0-13 (needs GCCSDK's 10.2.0-1 package as the base)
PKG_OUT=~/gccsdk-next/release python3 $R/make-cxx-package.py 5 ~/gccsdk-next/build-f/arm-riscos-gnueabihf/libstdc++-v3/src/.libs/libstdc++.so.6.0.36
PKG_OUT=~/gccsdk-next/release python3 $R/make-fortran-package.py 2
$R/build-sul.sh ~/gccsdk-next/unixlib/root ~/gccsdk-next/sul-build                         # SharedUnixLibrary 1.16: the stock module and the fixed 1.16-vforkfix3, with GCCSDK 10.2.0's tool chain
PKG_OUT=~/gccsdk-next/release python3 $R/make-sul-package.py ~/gccsdk-next/sul-build       # SharedULibFix_1.16-vforkfix3_arm.zip (optional; see SHAREDULIB-FIX.md)
```

The metadata of the packages (maintainer, licence, copyright text) is in `scripts/pkgmeta.py`.

`build-sul.sh` assembles `module/sul.s` of the UnixLib tree of step 5 with the tool chain of GCCSDK 10.2.0 (the seed of step 1), because `sul.s` uses FPA instructions that binutils 2.45.1 no longer assemble. It builds the stock module first and checks that it is **byte for byte GCCSDK's own build**, then applies the three patches one after the other. `make-sul-package.py` compiles the programs of `recipe/gcc-16.2.0-riscos/sulfix` with the cross compiler of step 4 and refuses a module that is not the one that passed the hardware tests (3228 bytes, FNV-1a `efc7dfea`). The package replaces nothing by itself: [SHAREDULIB-FIX.md](SHAREDULIB-FIX.md).

## 7. The native compiler (about 15 minutes on 22 cores, 13 of them for the compilers)

```bash
$R/build-native-prereqs.sh                                       # GMP, MPFR, MPC cross-built for RISC OS -> ~/gccsdk-next/hostlibs-riscos
$R/prepare-native-src.sh                                         # a fresh tree from the tarball: apply-port, apply-port-cxx, apply-port-native (-> src/native2)
$R/build-native-lto.sh                                           # cc1, cc1plus, f951, lto1, lto-wrapper, collect2 ... compiled for RISC OS with LTO support (-> native-stage3-lto)
# without LTO:  $R/build-native-all.sh O2
B=~/gccsdk-next/recipe/binutils-2.45.1-riscos/scripts
$B/build-binutils-native.sh ~/gccsdk-next/src/binutils-2.45.1 ~/gccsdk-next/build-binutils-native ~/gccsdk-next/binutils-native-install   # as, ld, ar, nm ... for RISC OS (8 MB stacks)
~/gccsdk-next/recipe/make-4.4.1-riscos/scripts/build-make.sh                                                                           # GNU make 4.4.1 for RISC OS -> make-riscos/install/bin/make
$R/make-native-tree.sh ~/gccsdk-next/native-tree ~/gccsdk-next/native-stage3-lto ~/gccsdk-next/binutils-native-install ~/gccsdk-next/make-riscos/install/bin/make
PKG_OUT=~/gccsdk-next/release python3 $R/make-native-package.py ~/gccsdk-next/native-tree 14                                           # Gcc16_16.2.0-14_arm.zip
PKG_OUT=~/gccsdk-next/release python3 $R/make-selftest-package.py 14                                                                    # Gcc16SelfTest_16.2.0-14_arm.zip
```

`make-native-tree.sh` and `build-native-lto.sh` take their stage directories as arguments or defaults (read their headers); the LTO build uses `recipe/gcc-16.2.0-riscos/data/no-plugin-ld` so that configure accepts a native `ld` without plugin support.
The native compiler is *cross-built* on Linux with the cross compiler of step 4 (host = target = `arm-riscos-gnueabihf`); the target options are exactly those of the cross compiler, so the native `cc1` is the same compiler.
What the native build needed, each point learned on the hardware, is in `recipe/gcc-16.2.0-riscos/README.md`.

## 8. The Linux tarball and the release

```bash
$R/package-cross-toolchain.sh ~/gccsdk-next/env-f ~/gccsdk-next/release 16.2.0-14 <a README file>
```

It copies the install tree, replaces the binutils symbolic links by the real files, strips the host programs, adds the licence texts and writes `riscos-gcc16-cross-16.2.0-14-x86_64-linux.tar.xz`. Check the result the way the release was checked:

```bash
cd ~/gccsdk-next/release
tar -xf riscos-gcc16-cross-16.2.0-14-x86_64-linux.tar.xz
~/gccsdk-next/tests/cross-smoke/cross-smoke.sh riscos-gcc16-cross-16.2.0-14-x86_64-linux ~/gccsdk-next/env-f     # works relocated, and gives byte-identical programs
```

## Changing something

* **A change to GCC or libstdc++:** edit or add a patch in `recipe/gcc-16.2.0-riscos/patches*/`, redo steps 4 and 7.
* **A change to UnixLib:** add a patch to `patches-unixlib/`, list it in `build-unixlib.sh` (in order, after the fix-level patch it belongs behind), and add a check for it to `tools/check-libunixlib.sh`. Each fix has a "fix level": a tiny patch (`unixlib-sysconf-fixlevel-N.patch`) makes `sysconf (0x4700)` answer N.
* **Test it** on the host with the cross compiler, then on RISC OS ([TESTING.md](TESTING.md)).
