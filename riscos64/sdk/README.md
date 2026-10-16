# aarch64-riscos-gcc: C programs for RISC OS 64 (experimental, started 16 October 2026)

`aarch64-riscos-gcc hello.c -o hello` makes `hello,ff8`, an AIF64 absolute that runs on RISC OS 64 (and on RISC OS Pyromaniac). It is the AArch64 GCC 16.2 of `tools/build-a64-toolchain.sh` with:

* the AIF64 header and the start-up code (`crt/aifhdr.S`, `crt/crt0.S`, `aif64.ld`): linked at `&8000`, the stack at the RAM limit that `OS_GetEnv` gives;
* **picolibc** as the C library (`tools/build-picolibc.sh`: stdio with `printf` and `scanf` for `double`, `long long`, files; string; `malloc`; `qsort`; `libm`; `setjmp`), built with `-ffixed-x18`;
* the system calls that picolibc wants, made with SWIs (`crt/riscos_syscalls.c`): `OS_WriteN`/`OS_NewLine`, `OS_ReadC`, `OS_Find`, `OS_GBPB`, `OS_Args`, `OS_Exit`; `sbrk` from the end of the program to the RAM limit minus 128 KB of stack;
* `argc` and `argv` from the command line (`crt/riscos_start.c`; double quotes keep spaces in a word);
* `bin/mkaif64.py`, which makes the AIF64 file from the linked image.

```bash
tools/build-picolibc.sh        # once: picolibc into /var/tmp/a64/picolibc
sdk/build-sdk.sh               # the support objects into sdk/lib
sdk/bin/aarch64-riscos-gcc -O2 sdk/tests/hello.c -o hello
```

Tested on RISC OS Pyromaniac (`tests/hello.pyromaniac.txt`: `printf` with `%d %ld %lld %u %x %c %s %f %e`, `argv`, an aligned `malloc`; `tests/libctest.pyromaniac.txt`: 36 checks of files, `fseek`/`ftell`, `strtod`, `sscanf`, `qsort`, `malloc`, `libm`, `setjmp`). Since 16 October 2026 the system calls also have `unlink` (so `remove` works), `rename`, `mkdir`, `stat`/`lstat`/`access`, `time`/`gettimeofday`/`clock_gettime` (the real time clock, OS_Word 14), `times`/`clock` (OS_ReadMonotonicTime, microseconds), `getenv` (the system variables, OS_ReadVarVal) and `system` (OS_CLI): `tests/sdktest.c` passes its 20 checks on Pyromaniac (`tests/sdktest.pyromaniac.txt`). Known gaps: `stdin` (`OS_ReadC`) is untested (the build service has no keyboard); the heap is the slot; no C++; no Wimp (desktop) programs.
