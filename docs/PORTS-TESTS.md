# The tests that come with the ports, run on RISC OS Pyromaniac

Date: 18 October 2026. The programs and libraries of [USERLAND64.md](USERLAND64.md) were built with the cross compiler and run on the emulator with the test suites that their authors ship (tests we did not write). A failure here is a bug in the C library, the shell or the port, so each one was looked at.

| Port | Upstream test | Result | Notes |
|---|---|---|---|
| libpng 1.6.44 | `pngtest` | Pass | |
| zlib 1.3.1 | `example`, `minigzip` | Pass | `minigzip` compress and decompress give back the file |
| bzip2 1.0.8 | `sample1` to `sample3` | Pass | Compressing gives the bytes of the reference `.bz2` files, and decompressing gives the reference text |
| expat 2.6.4 | `runtests` | 4,380 of 4,392 | All 12 misses are `test_buffer_can_grow_to_max` in its 12 modes: it needs a buffer larger than the heap limit of 1 GB |
| jq 1.7.1 | `jq.test` | 447 of 447 | |
| | `onig.test`, `base64.test`, `optional.test` | 40, 7 and 3 of 40, 7 and 3 | |
| | `man.test` | 222 of 224 | The two misses read `$ENV.PAGER`: a program sees the variables its parent exported, not one set with `Set` |
| PCRE2 10.44 | `testinput1` | Identical output (10,506 lines) | |
| | `testinput2` | Identical, 18,061 lines | The 9 lines that `RunTest` appends from separate runs with special arguments were not run |
| | `testinput4`, `5`, `6`, `7`, `8`, `9` | Identical | `8` compared with `testoutput8-8-2` (link size 2) |
| xz 5.4.7 | the 98 files of `tests/files` | 0 failures | 24 good `.xz` decompress; 42 bad and 5 unsupported are refused; 3 good and 5 bad `.lzma` |
| | round trips, levels 1, 3 and 6 | 0 failures | text, random, zero and mixed data |
| lz4 1.10.0 | round trips, levels 1, 3, 9, dependent blocks, `--fast` | 0 failures | the same four kinds of data |
| zstd 1.5.6 | round trips, levels 1, 3, 9, `-T1`, a pipe | 0 failures | |
| libjpeg-turbo 3.0.4 | 21 of its md5 tests | All 21 give its official md5 | `cjpeg`, `djpeg` (PPM, BMP, 565, grey, progressive) and `jpegtran`; the float DCT and arithmetic coding tests were not run |

Not run: PCRE2's locale, 16-bit, 32-bit, JIT and POSIX-interface tests (3, 10 to 26); the zstd and lz4 `make check` suites (long shell scripts); SQLite's Tcl suite; the tests of GNU sed, grep and gawk (the next step).

## What the runs found

- **A request for more memory than the heap area can hold aborted the program** in the emulator instead of failing with `ENOMEM` (expat's `test_get_buffer_3_overflow` asks for about 2 GB). The SDK now refuses a request above the maximum size of the heap area before it asks the system. Fixed in `sdk/crt/riscos_syscalls.c`.
- **`environ` holds only what the parent exported** (`getenv` also sees any system variable). This is by design here; it makes `$ENV` of jq and `env` list less than on Linux when a variable was set with `Set`.
- **Emulator limit:** two runs of `bzip2 -N` in one job once ran the emulator out of memory ("No memory available") while one run per job was fine; the scripts here run each tool in its own job when a long list of commands is involved.

The scripts are in [`riscos64/ports/userland-tests/b1`](../riscos64/ports/userland-tests/b1).

## gawk 5.3.1 test directory (in progress, 18 October 2026)

The 274 tests of gawk's `test/` directory that need no locale (20 more need `en_US` and are skipped) are turned from the Makefile recipes into shell scripts that the port's own `sh` runs, with `cmp`, `diff`, `sed`, `grep` and the coreutils from the packages. A first pass gave 251 of 274. After the library fixes below, the first 72 tests run again as short jobs (the build service ends a job after about 60 seconds): 71 pass; the one miss is an unexpanded Makefile variable in the generated script (`awkpath`), not a fault of the port. The rest is not run yet. Known not to work: `sigpipe1` (`yes | true`: a program that never reads stops only when its reader says so; planned), `poundbang` (the shell does not run `#!` scripts), `clobber` (opens a file for reading and for writing at once; RISC OS allows one writer).

Found by these tests and fixed in the SDK and UnixLib 64 (not yet in a published tool chain): `dup2` onto descriptors 0 to 2; a child program's output into a file the parent has redirected to; standard output redirected to a file is a regular file and fully buffered (one write per line made gawk about six times slower); a newline or a quote in the command of `system ()` or `popen ()` was lost on its way to `sh -c`; the exit status of `pclose` through gawk's simulated pipes.

