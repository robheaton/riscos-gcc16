# aarch64-riscos-gcc: C programs for RISC OS 64 (experimental, started 16 October 2026)

`aarch64-riscos-gcc hello.c -o hello` makes `hello,ff8`, an AIF64 absolute that runs on RISC OS 64 (and on RISC OS Pyromaniac). It is the AArch64 GCC 16.2 of `tools/build-a64-toolchain.sh` with:

* the AIF64 header and the start-up code (`crt/aifhdr.S`, `crt/crt0.S`, `aif64.ld`): linked at `&8000`, the stack at the RAM limit that `OS_GetEnv` gives;
* **picolibc** as the C library (`tools/build-picolibc.sh`: stdio with `printf` and `scanf` for `double`, `long double`, `long long`, `%n` and positional arguments, files; string; `malloc`; `qsort`; `libm`; `setjmp`), built with `-ffixed-x18`;
* the system calls that picolibc wants, made with SWIs (`crt/riscos_syscalls.c`): `OS_WriteN`/`OS_NewLine`, `OS_ReadC`, `OS_Find`, `OS_GBPB`, `OS_Args`, `OS_Exit`; `sbrk` from the end of the program to the RAM limit minus 128 KB of stack;
* `argc` and `argv` from the command line (`crt/riscos_start.c`; double quotes keep spaces in a word);
* `bin/mkaif64.py`, which makes the AIF64 file from the linked image.

```bash
tools/build-picolibc.sh        # once: picolibc into /var/tmp/a64/picolibc
sdk/build-sdk.sh               # the support objects into sdk/lib
sdk/bin/aarch64-riscos-gcc -O2 sdk/tests/hello.c -o hello
```

Tested on RISC OS Pyromaniac (`tests/hello.pyromaniac.txt`: `printf` with `%d %ld %lld %u %x %c %s %f %e`, `argv`, an aligned `malloc`; `tests/libctest.pyromaniac.txt`: 36 checks of files, `fseek`/`ftell`, `strtod`, `sscanf`, `qsort`, `malloc`, `libm`, `setjmp`). Since 16 October 2026 the system calls also have `unlink` (so `remove` works), `rename`, `mkdir`, `stat`/`lstat`/`access`, `time`/`gettimeofday`/`clock_gettime` (the real time clock, OS_Word 14), `times`/`clock` (OS_ReadMonotonicTime, microseconds), `getenv` (the system variables, OS_ReadVarVal) and `system` (OS_CLI): `tests/sdktest.c` passes its 20 checks on Pyromaniac (`tests/sdktest.pyromaniac.txt`). Known gaps: `stdin` (`OS_ReadC`) is untested (the build service has no keyboard); the heap is the slot; no C++; no Wimp (desktop) programs.

**Real code: zlib** (16 October 2026): `tests/build-zlib.sh ZLIBDIR` builds zlib 1.2.12 with the SDK. `tests/zlibtest.c` (checksums, `compress2` at levels 0-9, streaming `deflate`/`inflate` in small pieces with every strategy, a damaged stream) passes 9 checks on Pyromaniac, and the compressed sizes are the same as on the host (1420459 bytes for the ten levels). zlib's own `example` gives the same output as on the host, including the test that fails there too (`inflate should report DATA_ERROR`, a flaw of that test with current compilers). Note that a RISC OS file name with a dot is a folder separator: `example foo.gz` fails, `example foogz` works.

## C++ programs (`aarch64-riscos-g++`)

`aarch64-riscos-g++` (a link to the driver) compiles and links C++ with GCC 16.2's libstdc++ and libsupc++: the containers, `std::string`, iostream, `std::unique_ptr`, exceptions (the unwind tables are kept in the
read-only part of the AIF64 image), the constructors and destructors of static objects, and `atexit`. `RISCOS64_CXX=1` makes `aarch64-riscos-gcc` do the same. The library is built once with
`build-libstdcxx.sh GCC-SOURCE-DIR PREFIX NEW-BUILD-DIR` (it configures libstdc++ with the SDK driver itself, as a cross build with picolibc);
`tests/cxxtest.cc` is the test; its output on Pyromaniac is `tests/cxxtest.pyromaniac.txt`.

Limits: no threads, no `std::filesystem` (directory listing and free space fail with `ENOSYS`), no locales other than "C", and the programs are large (an iostream "hello" is about 1.1 MB).
