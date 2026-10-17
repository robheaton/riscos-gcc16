# GMP, MPFR and MPC for AArch64 RISC OS, built with the SDK and tested on Pyromaniac

These three libraries are the ones that GCC needs; building them with `sdk/bin/aarch64-riscos-gcc` is a test of the SDK on real, large C programs, and the first step towards a compiler that runs on RISC OS 64. The
test programs of each library are run on RISC OS Pyromaniac (through the hosted RISC OS Build service), as AIF64 programs.

| File | What it does |
|---|---|
| `build-gmp-mpfr-mpc.sh SOURCES WORKDIR` | configures, builds and installs GMP, MPFR and MPC as static libraries, then builds their test programs |
| `run-tests.sh gmp\|mpfr\|mpc WORKDIR` | runs the test programs of one library on Pyromaniac; the ones that time out are run again with fewer repetitions |
| `rofile.h` | a header that the test programs are compiled with (`-include`): it maps the file names, see below |

`SOURCES` is a folder with `gmp-6.1.0`, `mpfr-3.1.4` and `mpc-1.0.3` (the versions that GCC's `contrib/download_prerequisites` fetches; other versions were not tried). The SDK has to be built first (`sdk/build-sdk.sh`) and
the cross compiler of `tools/build-a64-toolchain.sh` has to be in `A64` (default `/var/tmp/a64/prefix/bin`).

## How the libraries are built

* **The compiler** is the SDK driver in a wrapper (`cc64`, written by the script into `WORKDIR`) that adds `RISCOS64_RAW=1` (the file name as asked for: a configure script wants `a.out`, not `a,ff8`) and the C dialect of the
  year the packages were written (`-std=gnu99 -fpermissive` and the warnings off that GCC 16 makes errors: C23 is its default, and these packages use K&R declarations and implicit `int`).
* **`--disable-assembly`** for GMP (its AArch64 assembler is not used; the generic C code is). The libraries are static; the `.la` files of the installed libraries are removed so that libtool does not pick up a `libgmp.la` of the host.
* **The tests** are built last, with a second wrapper (`cc64t`) that also includes `rofile.h`, and stand-ins for the two `.la` files that the test makefiles link with.

## What was needed in the SDK

Running these programs on Pyromaniac found things that the SDK's own tests had not: `fopen ("w+")` did not empty the file, `printf ("%Lf")` of a `long double` gave wrong numbers (picolibc is now built with long double, `%n` and
positional arguments), the heap stopped at the 12 MB application space (`sbrk` now makes a dynamic area), `fclose (stdout)` after `freopen` left the file open (a patch for picolibc, `tools/picolibc-freopen-close.patch`), and `open`
returned `EIO` for every error.

## The file names

On RISC OS the dot is the directory separator: `fopen ("io.tmp", ...)` names the file `tmp` in the directory `io`, which does not exist. The tests of these libraries use names like that, so `rofile.h` makes `fopen` and `freopen`
map a name (`.` becomes `_`, `/` becomes `.`; the source directory the tests were built in, and `./`, are dropped; `/dev/null` is left alone), and the runner puts the data files of the tests next to the programs under the
mapped names (`tmul_dat`). The SDK itself does not map names: that is the job of a C library that follows RISC OS conventions (see `docs/UNIXLIB64.md`).

## Results on Pyromaniac (17 October 2026)

| Library | Programs | Pass | The others |
|---|---|---|---|
| GMP 6.1.0 | 168 | 168 | 6 (`mpz_convert`, `mpz_reuse`, `t-gcd`, `t-powm_ui`, `t-remove`, `t-tdiv`) only with `GMP_CHECK_REPFACTOR=0.05`: the random tests take too long on the emulator at the default |
| MPFR 3.1.4 | 157 | 155 | `tzeta`: not confirmed (see below); `tget_set_d64`: skips itself (exit 77: no decimal64 support). `tadd_ui` and `ttanh` pass only with the 900 s limit, not at 300 s |
| MPC 1.0.3 | 64 | 63 | `texp`: not confirmed (see below). Five others (`tatanh`, `tio_str`, `tpow`, `tpow_fr`, `ttan`) pass only with the 900 s limit, not at 300 s |

No test has given a wrong result. `tzeta` and `texp` are the two that were not seen to finish: the build service closes the connection of a job after about 10 minutes (whatever time limit is asked for; answering its pings, which `tools/ws_build.py` now does, made no difference), and both run longer than that on the emulator. `run-tests.sh` runs the tests that time out again, one at a time, with a limit of 900 s.

Run only one job at a time on the service: with two the connections are dropped (the runner reports rc=255).
