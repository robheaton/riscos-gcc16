# Building everything from source

You only need this to change the tool chain or to rebuild it. To **use** it, install the [packages](INSTALL-RISCOS.md) or the [cross compiler tarball](CROSS-COMPILER.md).

> **Status of these instructions.** They are the scripts that built the released binaries, on one machine (Ubuntu 26.04, host GCC 15.2, 22 cores) in September and October 2026.
**This describes the run made for 16.2.0-24.** Before the release every command on this page was run again, in order, from a fresh copy of this repository in an empty home directory (same machine; the GCCSDK of step 1 was reused). The run took 32 minutes of working time and did not stop: binutils, the host libraries and the cross compiler in 14 minutes, UnixLib and the C runtime package in 2 minutes, the native tool chain (GCC, binutils and make built for RISC OS) in 14 minutes, the Linux tarball and its smoke test in 2 minutes. Nothing was built twice.
> **The `Gcc16` package, `Gcc16SelfTest`, the C runtime package and the Linux tarball on the releases page are the output of that run**, and the packages are the files that were installed and tested on the Raspberry Pi. One detail: the C runtime package was made once more from the same library after the run, to add the credit to Andrew Youll to its `RiscPkg/Copyright` (no other file in it differs). The C++ and Fortran runtime packages and `SharedULibFix` are the files of earlier releases: this run built them again, and every file in each zip is byte for byte the same (only the dates in the zip files differ), so the published files were kept.
> 16.2.0-24 is a release of UnixLib: the thread ticker of Andrew Youll's riscos-unixlib (`patches-unixlib/unixlib-thread-ticker.patch`, fix level 17), built by `build-unixlib.sh` (step 4) with its `check-libunixlib.sh`; the module kit and the compiler are those of 16.2.0-23.
> Compared with the `Gcc16` package of 16.2.0-23, 1,144 of its 1,166 files are byte for byte the same; none is new or gone; and 22 differ. They are the new UnixLib (`libunixlib.so`, `crt0` and `gcrt0`), `!Boot`, `!Run`, the `ReadMe` and the two `RiscPkg` files (the version and the text), and `cc1` and `cc1plus`, which differ in 16 bytes each (the checksum that GCC keeps in its own executables); the twelve static libraries (`libmodkit-core.a`, `libgcc.a`, `libstdc++.a` and the others) differ only in the timestamps inside, no member of any of them changed. The regression run passed with them.

## What gets built

```
GCC 16.2.0 + binutils 2.45.1  --->  cross compiler (Linux, x86-64)  --->  UnixLib 5.0 rebuilt (libunixlib, libm)
                                          |                                      |
                                          +--->  libstdc++, libgfortran         +--->  SharedLibs-C-armeabihf package
                                          |
                                          +--->  the module kit (modkit/: libmodkit, cmunge, modreloc, mkoslib), installed by install-modkit.sh
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

Host tools: a C and C++ compiler (GCC 13 or newer; 15.2 was used), `make`, `patch`, `autoconf2.69` (the binary must be called `autoconf2.69`; it regenerates libstdc++'s `configure`), `python3`, `rsync`, `xz`, `tar`, `file`, `subversion` (to fetch GCCSDK). Only for the optional libraries built on top of the tool chain: `bison` and `flex` ([OSLIB.md](OSLIB.md)), `perl` ([RUFL.md](RUFL.md)), `cmake`, `git`, `gperf` and `pkg-config` ([NETSURF.md](NETSURF.md)).

## 0. The upstream sources

Put these in `~/gccsdk-next/src/` (URLs and checksums in [SOURCES.md](../SOURCES.md)): `gcc-16.2.0.tar.xz`, `binutils-2.45.1.tar.xz`, `make-4.4.1.tar.gz`, and extract `gmp-6.1.0`, `mpfr-3.1.4` and `mpc-1.0.3` into `~/gccsdk-next/src/prereq/` (the versions GCCSDK's own recipe uses).

## 1. The GCCSDK seed

The recipe reads two things from a GCCSDK 10.2.0 EABI build: the **sysroot pieces** (the UnixLib headers, `crt0.o`, the dynamic loader and the 10.2.0 `libunixlib`/`libdl`, from `~/gccsdk/env`) and the **UnixLib source tree with its generated build files** (`~/gccsdk/build/gcc/gcc-10.2.0/libunixlib`).
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

`install-modkit.sh` builds `libmodkit-core.a` (one object per source of `modkit/lib`, each function and data item in a section of its own: `-ffunction-sections -fdata-sections`, so that the link of a module, which `-mmodule` runs with `--gc-sections`, drops what the module does not call) with the compiler of step 4 (`-mmodule`), makes `libgcc-mod.a` (`modkit/bin/mklibgcc.sh`: the `libgcc.a` of the tool chain without the members that contain VFP or ARMv7 instructions, which a module must not run), makes `libstdcxx-mod.a` from the `libstdc++.a` of the tool chain the same way (the members that are plain ARMv6 code and not position independent, without the exception runtime: 35 of 199; the red-black tree behind `std::map`, the list nodes ...), writes `libmodkit.a` as a linker script that names the library, `libgcc-mod.a` and `libstdcxx-mod.a` (since 16.2.0-17; the releases before it named `libgcc.a`), and puts them and the linker script into the tool chain. It also compiles `cmunge`, `modreloc` and `mkoslib` from `modkit/src` with the host compiler (`HOSTCC`, default `cc`) and installs them as `bin/cmunge`, `bin/arm-riscos-gnueabihf-modreloc` and `bin/arm-riscos-gnueabihf-mkoslib`, installs `bin/arm-riscos-gnueabihf-asasm-fix` (the shell script that makes the objects of GCCSDK's `asasm` linkable), puts the headers (`include`, and `include-cxx` for C++) where the driver finds them, and copies the library sources, the headers and `module.mk` to `share/riscos-modkit/`. It builds `libOSLib32.a` only when the environment variable `OSLIB` names a folder that has `oslib/os.h`; OSLib is not part of this project's releases, so the tool chains of the releases do not have it (use [`install-oslib.py`](OSLIB.md)). The result is part of the tarball of step 8.

`build-unixlib.sh` refuses to build with a compiler that does not have stack probing on by default, and a patch that does not apply is a hard error. `check-libunixlib.sh` looks for the code of every fix in the built library. `build-pthticker.sh` (run at the end of `build-unixlib.sh`) builds the optional `PThreadTicker` module from the patched source.
(`libunixlib.a` is built too, for `-static`; the SharedUnixLibrary module is built separately by `build-sul.sh`.)

## 6. The runtime packages and the fixed SharedUnixLibrary

```bash
PKG_OUT=~/gccsdk-next/release python3 $R/make-c16-package.py 15 ~/gccsdk-next/unixlib/build               # SharedLibs-C-armeabihf 16.2.0-15 (needs GCCSDK's 10.2.0-1 package as the base)
PKG_OUT=~/gccsdk-next/release python3 $R/make-cxx-package.py 6 ~/gccsdk-next/build-f/arm-riscos-gnueabihf/libstdc++-v3/src/.libs/libstdc++.so.6.0.36
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
PKG_OUT=~/gccsdk-next/release python3 $R/make-native-package.py ~/gccsdk-next/native-tree 24                                           # Gcc16_16.2.0-24_arm.zip
PKG_OUT=~/gccsdk-next/release python3 $R/make-selftest-package.py 24                                                                    # Gcc16SelfTest_16.2.0-24_arm.zip
```

`make-native-tree.sh` and `build-native-lto.sh` take their stage directories as arguments or defaults (read their headers); the LTO build uses `recipe/gcc-16.2.0-riscos/data/no-plugin-ld` so that configure accepts a native `ld` without plugin support.
The native compiler is *cross-built* on Linux with the cross compiler of step 4 (host = target = `arm-riscos-gnueabihf`); the target options are exactly those of the cross compiler, so the native `cc1` is the same compiler.
What the native build needed, each point learned on the hardware, is in `recipe/gcc-16.2.0-riscos/README.md`.

**16.2.0-25 is made from the package of 16.2.0-24, not by a new run of this page.** Its `Gcc16` has the same compiler, binutils, `make` and libraries (every file of the 16.2.0-24 package that is not named here is byte for byte the same, with the same attributes), five files more (the four C++ headers of the kit in `lib/gcc/arm-riscos-gnueabihf/16.2.0/include-cxx`, and the spec file `modcxx` next to them) and four files changed (`RiscPkg/Control`, `RiscPkg/Copyright`, `!Boot` for the version and `docs/ReadMe` for the text). One script does that:

```bash
python3 -I $R/make-packages-cxxmod.py ~/gccsdk-next/release/assets-v16.2.0-24 ~/riscos-gcc16/modkit ~/gccsdk-next/release/out25      # Gcc16_16.2.0-25_arm.zip
PKG_OUT=~/gccsdk-next/release/out25 python3 $R/make-selftest-package.py 25                                                            # Gcc16SelfTest_16.2.0-25_arm.zip (the self-test with check 13)
```

The first argument is the folder with the zip files of the 16.2.0-24 release, the second the module kit of this repository (the script reads `modkit/include-cxx` from it). The Linux tarball, the three runtime packages and `SharedULibFix` of the release are the files of 16.2.0-24 and 16.2.0-15, and the run described above was not repeated for 16.2.0-25.

## 8. The Linux tarball and the release

```bash
$R/package-cross-toolchain.sh ~/gccsdk-next/env-f ~/gccsdk-next/release 16.2.0-24 <a README file>
```

It copies the install tree, replaces the binutils symbolic links by the real files, strips the host programs, adds the licence texts and writes `riscos-gcc16-cross-16.2.0-24-x86_64-linux.tar.xz`. Check the result the way the release was checked:

```bash
cd ~/gccsdk-next/release
tar -xf riscos-gcc16-cross-16.2.0-24-x86_64-linux.tar.xz
~/gccsdk-next/tests/cross-smoke/cross-smoke.sh riscos-gcc16-cross-16.2.0-24-x86_64-linux ~/gccsdk-next/env-f     # works relocated, and gives byte-identical programs
```

## Changing something

* **A change to GCC or libstdc++:** edit or add a patch in `recipe/gcc-16.2.0-riscos/patches*/`, redo steps 4 and 7.
* **A change to UnixLib:** add a patch to `patches-unixlib/`, list it in `build-unixlib.sh` (in order, after the fix-level patch it belongs behind), and add a check for it to `tools/check-libunixlib.sh`. Each fix has a "fix level": a tiny patch (`unixlib-sysconf-fixlevel-N.patch`) makes `sysconf (0x4700)` answer N.
* **Test it** on the host with the cross compiler, then on RISC OS ([TESTING.md](TESTING.md)).
