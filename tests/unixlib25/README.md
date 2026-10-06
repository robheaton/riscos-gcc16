# Tests of libunixlib 16.2.0-12 (fix level 14)

Five small programs for what UnixLib 16.2.0-12 changed. Each prints what it checks and ends with a `SUMMARY [name]: N checks, M failed -> PASS` line (or `FAIL`), and returns 0 only when it passes. They are built for RISC OS (UnixLib) and for a Linux host (glibc): the
checks are the same, so a program that fails on RISC OS and passes on the host points at the library. `build.sh` builds them with the cross compiler.

| Program | What it checks | Before 16.2.0-12 (fix level 13) | Now |
|---|---|---|---|
| `finitest.c` | the `.fini_array` of a C program runs at exit, in the order glibc uses: two constructors, `atexit` functions, three destructors with different priorities; the last destructor compares the sequence and calls `_exit` | prints nothing: no destructor ever ran | 8 checks, 0 failed |
| `rlimtest.c` | `getrlimit (RLIMIT_STACK)` is the size of the main stack (`rlimtest 1048576` for a plain program: 1 MB; the `-DBIGSTACK` build, `rlimtest64 67108864`, sets `__stack_size` to 64 MB), a recursion to 75% of it works, the soft limit can be lowered | answers 536,838,144 (the largest Wimp slot); a 6 MB recursion then overflows the real 1 MB stack | 6 checks, 0 failed |
| `semtest.c` | POSIX semaphores with threads: counting, `sem_wait` blocks, `sem_timedwait` (timeout, early post, past deadline, bad `tv_nsec`), several waiters, `sem_destroy` with a waiter (RISC OS only), ping-pong between two threads, no leak over 100,000 calls (RISC OS only) | 11 of 20 fail: `sem_timedwait` is `ENOSYS`, the heap grows by 1,597,440 bytes in the leak test | 20 checks (18 on glibc), 0 failed |
| `covtest.c` | built with `--coverage`: when it ends it must write `covtest.gcda` (the exit function of libgcov runs from the `.fini_array`) | no `.gcda` file | the file is written; the cross `gcov` reads it |
| `covtest.c` again | built with `-fprofile-generate`: it must write `pgotest.gcda`, which `-fprofile-use` accepts | no file | the file is written and accepted |

`fixlevel` (in `../fixlevel`) tells which library is in use: `fixlevel 14` returns 0 only for 16.2.0-12.

## Build and run

```bash
./build.sh /some/directory         # needs the cross compiler of docs/BUILDING.md step 4 (env-f)
```

Copy the programs to RISC OS (the `,e1f` suffix gives them the file type ELF on a Samba share) and run them in a Task window. The two coverage programs write their `.gcda` file to the absolute path of the object file they were compiled from, a Linux path: tell libgcov where to write
instead, then read the file with the cross `gcov` on Linux:

```
*Set GCOV_PREFIX .
*Set GCOV_PREFIX_STRIP 6           the number build.sh printed: the directories of the Linux path to cut off
*covtest 100
```
```bash
arm-riscos-gnueabihf-gcov -b -c covtest.gcda        # next to covtest.gcno and covtest.c; prints the executed lines
```

(The native compiler needs none of this: `gcc --coverage` on RISC OS writes the file next to the program, and `gcov` is part of the `Gcc16` package: see [USING-NATIVE.md](../../docs/USING-NATIVE.md).)

The results on the test machine, before and after the new library, are in [TESTING.md](../../docs/TESTING.md), and the bug reports behind the three library fixes are `docs/upstream/22-...`, `23-...` and `24-...`.
