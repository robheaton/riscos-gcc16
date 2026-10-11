# NetSurf, built as a test of the tool chain

[NetSurf](https://www.netsurf-browser.org/) is a web browser with a RISC OS front end. It is a good test of a C compiler and a C library: about 500 source files, a dozen libraries of its own, and a long list of third-party libraries (TLS, HTTP, image formats, XML). `recipe/gcc-16.2.0-riscos/scripts/build-netsurf.py` fetches everything, builds it with this tool chain and makes an application folder.

**Nothing of NetSurf or of the libraries is in the releases of this project**: the script downloads the sources and checks the third-party ones against the SHA-256 that were recorded when it was written. NetSurf is under the GNU General Public License (version 2); its libraries and the third-party ones have their own licences (MIT, BSD-like, Apache 2.0 for OpenSSL 4, the curl and zlib licences).

## What was built, and what it did

On 2026-10-11 the development version (NetSurf git `0b57c39`) built and linked with the GCC 16 cross compiler, and **ran on the Raspberry Pi** (RISC OS 5.30): the application started, put its icon on the icon bar and displayed pages. This is a test of the build, not a release of NetSurf: no systematic test of the browser was run.

| Part | Version | Notes |
|---|---|---|
| NetSurf and its libraries | git, latest commit (recorded in `out/BUILD-INFO.txt`) | `libwapcaplet`, `libparserutils`, `libhubbub`, `libcss`, `libdom`, `libnsbmp`, `libnsgif`, `libnsutils`, `libnslog`, `libnspsl`, `libsvgtiny`, `librufl` ([RUFL.md](RUFL.md)), `libpencil`, and the host tool `nsgenbind` (the JavaScript bindings); Duktape JavaScript is part of NetSurf |
| curl | 8.22.0 | with OpenSSL |
| OpenSSL | 4.0.3 | static, no assembler, no threads |
| zlib, libpng, libjpeg-turbo, expat | 1.3.2, 1.6.59, 3.2.0, 2.9.0 | libjpeg-turbo without SIMD unless the flags give NEON |
| `ccres` | svn trunk | the template compiler, built for the host |

It needs [OSLib](OSLIB.md) in the tool chain (`install-oslib.py`) and, on the build machine, git, svn, make, cmake, perl, flex, bison, gperf, pkg-config and a host compiler.

```bash
recipe/gcc-16.2.0-riscos/scripts/build-netsurf.py --toolchain <the tool chain> --work ~/netsurf-build [--opt "-O2 -mcpu=cortex-a72 -mfpu=neon-fp-armv8"]
```

Everything goes under `--work`: `src/` (the downloads), `env/` (the install prefix), `build/` (logs), `out/!NetSurf` (the application, with the program stripped: about 9 MB) and `out/BUILD-INFO.txt` (the commits and flags). The tool chain is not changed. A full run takes a few minutes.

## What had to be handled

* **NetSurf's build system already knows `arm-riscos-gnueabihf`** (its RISC OS front end picks `-elfeabihf` from the name of the compiler), so no change to NetSurf was needed. Its libraries build with their own makefiles; the script only overrides their `-Werror` (`WARNFLAGS=-Wall`), because GCC 16 has new warnings.
* **curl must have a TLS backend even to start.** NetSurf sets `CURLOPT_CAINFO` at start-up and stops with *NetSurf failed to initialise core* when libcurl refuses the option, which a curl without TLS does. The first test build was exactly that; the log in the application folder said so.
* **OpenSSL needs a source of random numbers, and UnixLib has one**: `/dev/urandom` reads the `CryptRandom` module of RISC OS. OpenSSL's generic Unix target uses it; expat is built with `-DXML_DEV_URANDOM` for the same reason (not with its "poor entropy" setting).
* **UnixLib has no `EPROTO`** (it is in POSIX). OpenSSL only uses it to recognise a socket error that is not fatal, so the build defines it as a value that never occurs (`-DEPROTO=9999`). Adding it to UnixLib would be better.
* **The slot of `!Run`.** GCCSDK's `makerun` works out `WimpSlot` from the size of the program file (5.5 MB), which is far too small for a program that has UnixLib's heap in its slot. The script writes `WimpSlot -min 32000k -max 32000k`, and starts the program with `-v` and its standard error in `<NetSurf$Dir>.Log`, so that a failure leaves a log next to the application.
* **c-ares**: the front end's Makefile asks for it but no code calls it (curl is built without it); an empty `libcares.pc` stands in for it.
* **libpng's CMake build names its library `libpng.a`**; NetSurf asks for `-lpng16`: the script links the name. **libjpeg-turbo's test programs do not build** (`fmemopen` is not in UnixLib), so only its library is built.
* **The last step of the Makefile fails** (`elf2aif -e`: GCCSDK 4.7.4's `elf2aif` has no such option). The ELF program is complete before it, and a RISC OS 5 machine with ARMEABISupport runs it as it is (file type &E1F), so the script runs `make -k` and checks that the program is there. `squeeze` (compression of the AIF) is not used.
* **RUfl's debug log** is hard-wired on and prints to the screen: the script applies [`patches-rufl/rufl-log-off.patch`](../recipe/gcc-16.2.0-riscos/patches-rufl/README.md).

## Making it faster

The default build uses the tool chain's defaults: `-O2` for generic ARMv7-A, which does not use the hardware divide or NEON of the Cortex-A72 of a Pi 4 or a Compute Module 4. `--opt` passes other flags to everything that is built from source (the libraries and NetSurf), and two more options make a profile-guided build. A program built for the Cortex-A72 does **not run on older machines** (Pi 1, Pi 2, Iyonix, BeagleBoard).

| Build | Flags (`--opt`) |
|---|---|
| base | `-O2` |
| A72 | `-O2 -mcpu=cortex-a72 -mfpu=neon-fp-armv8` (also switches libjpeg-turbo's NEON code on) |
| A72 + LTO | the same and `-flto` (with LTO the script adds `-ffat-lto-objects` and gives the flags to the link) |
| profile-guided (PGO) | the same with `-ffp-contract=off` and `--pgo-dir` (below) |

`-ffp-contract=off` stops the compiler fusing a multiply and an add into one instruction (`vfma`), which rounds once instead of twice: NetSurf's JavaScript engine does strict IEEE double arithmetic. In the benchmark below it made no difference to a result and none to the speed.

**How it was measured.** [`modkit/tests/nsbench`](../modkit/tests/nsbench/nsbench.c) is a program for a Task window that times the parts of NetSurf that take the time: it parses a 550 KB HTML page (hubbub and libdom), a 1.2 MB style sheet (libcss, the CSS Zen Garden sheet of libcss's tests), decodes a JPEG and a PNG, inflates 8 MB, and runs an integer-divide loop and a floating-point loop. Each test runs for at least 4 seconds; the program first prints checksums of everything that it computed (the HTML node count, the style sheet size, hashes of every decoded pixel and of the inflated data, the results of the loops), which **must be the same in every build**. `build-nsbench.sh` builds it against the libraries of a given `--work` folder; `make-data.py` makes the input files. On the Raspberry Pi (Compute Module 4, RISC OS 5.30, 2026-10-11; two rounds of each build, alternating; the rounds agree to within 4%):

| Test (speed-up against base) | A72 | A72 + LTO | PGO | **PGO + LTO** |
|---|---|---|---|---|
| HTML parse (hubbub, libdom) | 1.03x | 1.06x | 1.12x | **1.24x** |
| CSS parse (libcss) | 1.02x | 1.06x | 1.08x | **1.10x** |
| JPEG decode | 2.12x | 2.15x | 2.14x | 2.09x |
| PNG decode | 1.00x | 1.00x | 1.00x | 1.02x |
| zlib inflate | 1.00x | 0.99x | 0.95x | 0.98x |
| Integer divide loop | 7.7x | 7.7x | 7.7x | 7.7x |
| Floating-point loop | 1.00x | 1.00x | 1.00x | 1.00x |

All the builds printed the same checksums. The JPEG gain is libjpeg-turbo's NEON assembler; the integer loop is the hardware divide against the library's software one (libcss and the layout code use fixed-point division); the floating-point loop was already on the VFP. The program is smaller too: 9.06 MB (base), 9.5 MB (A72), 8.9 MB (A72 + LTO), 9.2 MB (PGO), 8.4 MB (PGO + LTO). This is a benchmark of the parsers and decoders: **layout, rendering and JavaScript were not measured**. The default, the profile-guided and the profile-guided LTO builds of NetSurf itself were all run on the Pi and worked; the author's impression, not a measurement, was that the profile-guided LTO build was the quickest, which agrees with the benchmark. (The cmake-built libraries, libjpeg-turbo and libpng, are compiled at `-O3` in every build: cmake appends it after the flags.)

### A profile-guided build

1. Build with instrumentation in a work folder of its own: `build-netsurf.py --toolchain T --work ~/nsb-gen --opt "-O2 -mcpu=cortex-a72 -mfpu=neon-fp-armv8 -ffp-contract=off -fprofile-generate"`. Build `nsbench` against it too (`build-nsbench.sh`).
2. **Make the folders for the profile files** with `pgo-dirs.py --work ~/nsb-gen --out gcda <the instrumented programs>`: libgcov of this tool chain cannot create directories (its `create_file_directory` is compiled out for this target), so a `.gcda` file whose folder is missing is skipped (`profiling:...:Skip`).
3. Copy the programs and the `gcda` folder to RISC OS. In the folder that holds `gcda`: `*Set GCOV_PREFIX gcda`, `*Set GCOV_PREFIX_STRIP 4` (the number of directories of the work folder, as `pgo-dirs.py` says), run `nsbench` and **use the instrumented NetSurf** on a variety of pages, then **quit it properly** (the profile is written when the program ends; a crash or a kill writes nothing, and the quit takes a while because it writes some 1,500 files). The profile data are merged from run to run.
4. Copy `gcda` back and run `pgo-collect.py gcda profile`; it renames each file the way `-fprofile-use` looks for it (the path of the object relative to the work folder, with `#` for `/`).
5. Build again, in any folder: `build-netsurf.py ... --opt "<the same flags without -fprofile-generate>" --pgo-dir profile`. The flags must be the same as in step 1 (the profile of a function is only used if its control flow is the same).

For a profile that serves you, browse the way you do. In the test above one session of ten minutes and one run of the benchmark gave the profile of 1,575 object files.

## Handing a build to someone else

A build for other people should be quiet and small. `build-netsurf.py --release` does the following, and `make-netsurf-zip.py` then packs the result:

* **No log.** NetSurf logs at run time, steered by the system variable `NetSurf$Logging` (`1` or unset: verbose; `0`: warnings and errors only), and the front end's own `!Run` sets it to `1`. With `--release` the `!Run` sets `0`, passes no `-v`, and keeps the log where NetSurf's own `!Run` has it, in `<Wimp$ScrapDir>.WWW.NetSurf.Log`. NetSurf is also compiled with `NETSURF_LOG_LEVEL := WARNING`, so the calls that log at INFO (the ones that fill the log of a normal start) are not in the program at all: the strings of those messages are not in the file.
* **No debug information.** The build uses `-g` and the pack step strips the program (about 9 MB instead of 20 MB; this changes the size of the file, not the speed). Assertions are off (`-DNDEBUG`), and RUfl's own log is off ([RUFL.md](RUFL.md)).
* **Never the instrumented program.** A build made with `-fprofile-generate` is about twice the size, slower, and writes profile files when it ends: hand out the build made with `--pgo-dir`, or without profile flags.
* **Kept on purpose:** `-mpoke-function-name` (the name of each function in the code, a word apiece, which gives RISC OS's backtrace of a crash its names), `-funwind-tables`, and the stack probing that this compiler turns on.
* A profile that was collected with the logging compiled in matches NetSurf's own functions less well once the logging is compiled out (a function whose calls changed has a different control flow, and gcc ignores its profile): the libraries are not affected. For the full gain, collect the profile with an instrumented build that has `--release`'s settings.

`make-netsurf-zip.py OUTDIR ZIP [README]` makes a zip of `out/!NetSurf` (with the RISC OS file types in the zip, as the packages of this project have them), a `ReadMe` and `BUILD-INFO`, ready to unpack on RISC OS. NetSurf is free software under the GNU General Public License version 2: whoever receives the program is entitled to the source (`BUILD-INFO` names every commit and version; this recipe builds it).

## Not done

`utf8proc`, WebP and JPEG XL are switched off. libjpeg-turbo has no NEON code unless the flags ask for it. NetSurf's own tests were not run. A package for others to install (a `!NetSurf` that is more than a test) is a different job from this one. How the browsers of the builds feel in use was not compared in detail: all three (default, profile-guided, profile-guided with LTO) were run on the Pi and worked, and the profile-guided LTO build seemed the quickest (an impression, not a measurement).
