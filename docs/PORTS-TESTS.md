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

Not run: PCRE2's locale, 16-bit, 32-bit, JIT and POSIX-interface tests (3, 10 to 26); the zstd and lz4 `make check` suites (long shell scripts); SQLite's Tcl suite; the tests of GNU sed, grep and gawk are in the section below.

## What the runs found

- **A request for more memory than the heap area can hold aborted the program** in the emulator instead of failing with `ENOMEM` (expat's `test_get_buffer_3_overflow` asks for about 2 GB). The SDK now refuses a request above the maximum size of the heap area before it asks the system. Fixed in `sdk/crt/riscos_syscalls.c`.
- **`environ` holds only what the parent exported** (`getenv` also sees any system variable). This is by design here; it makes `$ENV` of jq and `env` list less than on Linux when a variable was set with `Set`.
- **Emulator limit:** two runs of `bzip2 -N` in one job once ran the emulator out of memory ("No memory available") while one run per job was fine; the scripts here run each tool in its own job when a long list of commands is involved.

The scripts are in [`riscos64/ports/userland-tests/b1`](../riscos64/ports/userland-tests/b1).

## gawk 5.3.1 test directory, SQLite and Lua (19 October 2026)

**gawk.** The 274 tests of gawk's `test/` directory that need no locale (20 more need `en_US` and are skipped) are turned from the Makefile recipes into shell scripts that the port's own `sh` runs, with `cmp`, `diff`, `sed`, `grep` and the coreutils from the packages. The build service ends a job after about 60 seconds, so they run in jobs of four. A first pass gave 251 of 274; after the fixes below **270 pass**. The four that do not: `printf-corners` (the C library's `printf` has no `%a`, and loses the sign of NaN and infinity), `poundbang` (the interpreter named on a `#!` line is ignored: the shell runs the file itself), `sigpipe1` (`yes | true`: a producer that never ends is stopped only by the `head` of the shell) and `clobber` (opens a file for reading and for writing at once; RISC OS allows one writer).

**SQLite 3.47.2.** 28 statements and queries (aggregates, window functions, recursive CTE, JSON functions, triggers, views, transactions and rollback, `vacuum`, `integrity_check`, date and string functions) give the output of Python's SQLite 3.46 apart from the layout of `EXPLAIN QUERY PLAN` and a random value. The Tcl suite and FTS5 are not part of this build.

**Lua 5.4.7 `locals.lua`.** Run in three parts because of the 60 second limit: before and after the stack overflow test both pass. The stack overflow test (`overflow` recurses to Lua's stack limit) does not finish in 60 seconds on the emulator and has to be run on a real machine.

**sed 4.9 test suite** (`testsuite/*.sh`, 67 shell tests; they run through the port's own `sh`, which is a test of the shell as well): **40 pass**, 16 are skipped by the suite itself (they need a multibyte locale, `valgrind`, SELinux or a system with text and binary modes), 7 are too long for the 60 second limit of the build service (`binary`, `bsd`, `bsd-wrapper`, `command-endings`, `compile-errors`, `dc`, `sandbox`: they work, they do not finish in time), 2 cannot run here (`follow-symlinks`: no symbolic links; `panic-tests`: no `mkfifo`), and 2 fail for reasons of the file system: `in-place-suffix-backup` (a backup name such as `a.bak` is a file in a folder on RISC OS), `temp-file-cleanup` (a file that is still open cannot be deleted, so `sed` cannot clean up its temporary file after an error); `in-place-hyphen` (`sed -i ... -`) failed until the `getopt` change below.

Found by these tests and fixed in the SDK and UnixLib 64: `dup2` onto descriptors 0 to 2; a child program's output into a file the parent has redirected to; standard output redirected to a file is a regular file (with its own inode, so `cat < a > b` works) and fully buffered (one write per line made gawk about six times slower); a newline, a quote or a tab in the command of `system ()` or `popen ()` or in an argument; `getppid ()`; and the exit status of `pclose` through gawk's simulated pipes.; the commands of a group `( a ; b ) < file` share one file position, as on Unix (a program says where it stopped reading, the shell hands that on); `lseek` on a redirected standard stream; the name a program was started with is its `argv[0]` (`sed: ...`, not `/work/sed/sed: ...`); a lone `-` argument (standard input) in the C library's `getopt`; `getppid ()`.

## gawk, sed, grep and Lua again, with tool chain 0.10 (20 October 2026)

The same suites were run again on Pyromaniac without the 60 second limit of the build service (so the long tests finish and the jobs run ten at a time), with the tools of GNU tools 11 and the C library of tool chain 0.10.

| Suite | Result |
|---|---|
| **gawk** `test/` (81 chunks of the tests that need no locale) | **282 pass, 2 do not**: `printf-corners` (the C library's `printf` has no `%a` and loses the sign of NaN and infinity), `poundbang` (the interpreter of a `#!` line is ignored) |
| **sed 4.9** `testsuite/` (67) | **47 pass**, 16 skipped by the suite (multibyte locale, `valgrind`, SELinux, text and binary modes), 4 do not: `in-place-suffix-backup` and `temp-file-cleanup` (reasons of the file system, as above), `follow-symlinks` and `panic-tests` (no symbolic links, no `mkfifo`). The seven tests that were too long for the service (`binary`, `bsd`, `bsd-wrapper`, `command-endings`, `compile-errors`, `dc`, `sandbox`) all pass. |
| **grep 3.11** `tests/` (126 jobs) | **50 pass**, 71 skipped by the suite (locale, PCRE, tools that are not here), 4 do not: `symlink` (no symbolic links); `stack-overflow` (a very deep recursion: it needs a machine with the real stack and time); `spencer1-run` and `spencer1-locale-run` (two cases of Henry Spencer's list put a control character, code 1 to 3, into a command line argument, which a RISC OS command line cannot hold) |
| **Lua 5.4.7** `locals.lua`, before and after `overflow` | both parts pass; `overflow` itself does not finish in 20 minutes |

Found by these runs and fixed in the shell and the library:

* **`exec` with only redirections, inside a function, a group (`{ ...; }`), an `if`, a loop or a `case`, stays in force after it** (it was undone when the compound command ended, so the rest of the output went to the console). `sed`'s `bsd-wrapper` test needed it.
* **`printf -- format ...`** printed the `--`.
* **A relative name in a redirection is made absolute when the redirection is set up**, so that `( cd d && cmd ) > out` writes `out` where it was meant; `2>&1` and the other duplications are not touched by that.
* **`eval cmd > file`**: the shell no longer holds the file open (RISC OS lets a file be open for writing once), so the command it runs can write it.
* **`cp /dev/null file`** is no longer refused (the descriptor of `/dev/null` answers `fstat` as a character device).
* **`stat` and `fstat` give `st_blksize` and `st_blocks`** (the blocks were always 0, which GNU `grep` reads as a sparse file) and **`lseek` returns `EINVAL` for `SEEK_HOLE` and `SEEK_DATA`** (it took them for `SEEK_SET`, so `grep` found a "hole" in every file): a file with a NUL byte after the first buffer was reported as "binary file matches" instead of printing its matching lines.
* Earlier the same day: a `close ()` of a standard descriptor in a `vfork` child no longer closes the parent's (`xargs` stopped the emulator).

## Releases 0.11, GNU tools 12 and CoreUtils 9.5-9 (21 October 2026)

Tool chain 0.11 and tools built with it. The locale-dependent tests were skipped until now because the helper `get-mb-cur-max` of the test suites was not built; with it they run.

| Suite | 0.10 | 0.11 |
|---|---|---|
| gawk (81 chunks) | 282 items, 2 not passing | the same |
| sed 4.9 | 46 pass, 16 skipped, 4 not passing | **54 pass**, 9 skipped, the same 4 not passing |
| grep 3.11 | 50 pass, 71 skipped, 4 not passing | **74 pass**, 46 skipped, 5 not passing: `symlink`, `stack-overflow`, `equiv-classes` (needs glibc's collation tables), the two Spencer tests |
| UnixLib's own tests ([`riscos64/unixlib/oracle`](../riscos64/unixlib/oracle)) | 30 pass of 41 that build | **45 pass** of 57 that build |

Found by these runs and fixed: `MB_LEN_MAX` was 1 while `MB_CUR_MAX` is 6 in the UTF-8 locales (`sed` with `\b` ended with "overflow detected"); a program started by another (`env sed`) was given its parent's name; `readdir` gave inode numbers that were not those of `stat`; `strsep` lost the empty field after a final delimiter; picolibc printed nothing for `%#.0o` of 0 and truncated instead of rounding in `long double` conversions (patches in `tools/`); with threads, a loop that waits by polling the clock stopped the other threads; in the shell, `local` and `declare` expanded their values a second time and `printf` had no `\xHH`. Not fixed: `printf` of a `double` gives at most 17 significant digits (glibc gives the exact expansion).

## Locales, `iconv` and `printf` (22 October 2026, tool chain 0.12)

Tested on Pyromaniac with the C library as it is in the source tree after the work on `iconv`, locales, collation and `printf`. The gawk tests that need `en_US.UTF-8` (29 of them) could not run before; they do now.

| Suite | 0.11 | now |
|---|---|---|
| gawk, normal run (68 chunks) | 282 items, 2 not passing | the same: `poundbang`, `printf-corners` |
| gawk, tests that need `en_US.UTF-8` | not run | **29 of 29 pass** (`mbstr1`, `wideidx`, `mbprintf*`, `backsmalls*`, `dfamb1`, `lc_num1`, `unicode1`, `commas` ...) |
| sed 4.9 | 54 pass, 9 skipped, 4 not passing | the same |
| grep 3.11 | 74 pass, 46 skipped, 5 not passing | 74 pass, 46 skipped; not passing: `symlink`, `stack-overflow`, `equiv-classes`, `spencer1-run`, and `c-locale` which takes longer than the 15 minutes of the job |
| Lua `l1`, `l3` | pass | pass |

The new functions are compared with glibc by programs that print the same text on both ([`riscos64/unixlib/tests/glibc`](../riscos64/unixlib/tests/glibc)): 1.4 million `iconv` conversions, every item of `nl_langinfo` and `localeconv` in 9 locales, 190 strings sorted by `strcoll`, 23 `strftime` formats, 49 `printf` lines in 6 locales. The differences that remain are in the results file ([`unixlib/oracle/RESULTS-2026-10-21.md`](../riscos64/unixlib/oracle/RESULTS-2026-10-21.md), last section).
