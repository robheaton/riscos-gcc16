# Bugs found along the way: reports for the GCCSDK maintainers

Running real programs on real hardware found bugs in UnixLib, in the SharedUnixLibrary and ARMEABISupport modules, and one question about RISC OS itself. Each one is written up as a **draft mail** for the GCCSDK mailing list,
with a patch where there is one, a reproducer, and an honest statement of what was measured on the machine and what was only read from the code or run on an interpreter. The drafts, patches, reproducers and the script that re-checks them are in
[`docs/upstream/`](upstream/00-INDEX.txt).

> **Status:** drafts. They have **not** been sent to the GCCSDK maintainers yet and may be revised before they are. The patches are against the GCCSDK svn trunk r7800 (`patch -p1` from the top of the tree) and are *already in the packages of this release* (UnixLib) or in the fixed module that the optional `SharedULibFix` package of this release ships (SharedUnixLibrary: [SHAREDULIB-FIX.md](SHAREDULIB-FIX.md)).

| # | Component | Problem | Patch |
|---|---|---|---|
| 01 | SharedUnixLibrary 1.16 | a `vfork` child that ends without `exec` kills its parent, resizes its parent's Wimp slot and, when the parent was started by `exec`, frees the parent's SOManager client: **a hard freeze** | yes |
| 02 | UnixLib | the `_exit` of such a child frees the RMA block of the program image it shares with its parent | yes |
| 03 | UnixLib | the signal stack (one page per program) is never freed | yes |
| 04 | ARMEABISupport | a failed `mmap` leaves its `mmap#N` dynamic area (and the memory it claimed) until the next reboot | yes, against 1.05 (1.08 is a rewrite of the memory code: the patch has to be written again for it) |
| 05 | ARMEABISupport + SharedUnixLibrary | the `mmap` memory of a `vfork` + `exec` child stays allocated until the root process ends | idea only |
| 06 | ARMEABISupport | `*RMKill` succeeds while SOManager holds a handle of a global allocator; every EABI program then fails to start | no |
| 07 | RISC OS 5.30 | a store made by the OS in SVC mode to a not-yet-mapped page of an EABI stack is lost; a 64-byte `vstm` into such a page is not recovered | no (questions) |
| 08 | UnixLib + SharedUnixLibrary | the heap of a `vfork` + `exec` child grows over the saved copy of its parent: the parent resumes destroyed | yes |
| 09 | UnixLib | `fread`/`fwrite` after a short `read`/`write` write the data twice or over itself | yes |
| 10 | UnixLib | `pthread_once` holds one global lock across the init routine: `std::async` deadlocks, and an exception from the routine terminates the program | yes |
| 11 | UnixLib | `pthread_cond_timedwait` takes its deadline from whole seconds: timed waits overrun by up to a second | yes |
| 12 | UnixLib | `sleep`/`usleep`/`nanosleep` with several threads share one alarm and flag | yes |
| 13 | UnixLib | `sysconf (_SC_NPROCESSORS_ONLN)` returns 2048: the enum numbering restarts after `_SC_PAGE_SIZE` | yes |
| 14 | UnixLib | five implicit function declarations that GCC 14 and later reject | yes |
| 15 | UnixLib | touch stack buffers before `read`/`fread`/`recv`/`recvfrom` (a workaround for 07) | yes |
| 16 | UnixLib | split the 64-byte `vstm` of the NEON `memcpy`/`memmove` (a workaround for 07) | yes |
| 17 | UnixLib | a main stack bigger than 1 MB for EABI programs (`__stack_size`), with a fallback | yes |
| 18 | UnixLib | a heap dynamic area that does not fit into the address space is retried at half the maximum size | yes |
| 19 | UnixLib | the inline SWI wrappers read their results from register variables after the `asm`; with GCC 16 the arguments of every program are cut to the length of its name while the DDEUtils module is loaded | yes (generated) |
| 20 | UnixLib | `__get_dde_prefix ()` never returns when a DDEUtils prefix is set | yes (one word) |
| 21 | UnixLib | `scanf` has no `long long` conversions: `%llx` is cut to 32 bits, `%hhd` writes two bytes, `%jd %zu %td %qd` are not understood, `%Lf` stores a float; this made the native `lto1` unable to read its own objects | yes |
| 22 | UnixLib | the `.fini_array` of a program is never run: C destructors do not run, and no `.gcda` file is written by a `--coverage` program (its exit function is a destructor) | yes |
| 23 | UnixLib | `getrlimit (RLIMIT_STACK)` of an EABI program answers the largest Wimp slot (512 MB), not the size of its main stack (1 MB): a recursion sized by it overflows the real stack | yes |
| 24 | UnixLib | POSIX semaphores: `sem_timedwait` is `ENOSYS` (and not declared), `sem_wait` polls and leaks a queue entry per call, `sem_destroy` never says `EBUSY` | yes |
| 25 | UnixLib, and the target header of the compiler | `gprof` (`-pg`) does not work for EABI programs: the compiler emits the legacy `mcount` call and links `crt0.o`, UnixLib's `mcount` is the APCS one, the profiler (`__gmon_start__`) is compiled out, and `setitimer` is refused in a Task window; the patch profiles with a sampler thread (no HAL timer) | yes (library; the compiler side is described) |

Everything the reports claim is re-checked by [`docs/upstream/verify/run-verify.sh`](upstream/verify/run-verify.sh): every patch applies to pristine upstream, the patched files compile with GCC 10.2.0 and GCC 16.2.0, the host models give the numbers in the reports,
and the machine code of the changed functions behaves on an ARM interpreter. The last runs: 245 checks in the author's work area and 244 in a fresh copy of this repository built as described in [BUILDING.md](BUILDING.md) (the last one compares a build of the fixed SharedUnixLibrary module, made by `build-sul.sh`; it is skipped when that build is absent), 0 failed. It needs a GCCSDK svn checkout, the cross compiler (BUILDING.md step 4) and the UnixLib build (step 5): see the script's header.

If you maintain UnixLib, SharedUnixLibrary or ARMEABISupport and would like these as proper mails, merge requests or in another form, please open an [issue](https://github.com/robheaton/riscos-gcc16/issues).

## For RISC OS Open Ltd

The survey of the OS's own C modules ([OS-MODULES.md](OS-MODULES.md)) found one bug in the OS sources. It belongs to RISC OS Open, not to the GCCSDK list, so it is not in the bundle above. It was **reported to RISC OS Open by the author on 8 October 2026** (their bug tracker, https://www.riscosopen.org/tracker/, needs an account; the GitLab project of the module, `RiscOS/Sources/Programmer/Squash`, shows no issue tracker). The text that was sent follows.

> **Squash: `Squash_Compress` with an input of length 0 runs away in the fast compressor**
>
> *Component:* Programmer/Squash, module `Squash` 0.31 (11 Feb 2023); the same code is in the ROM module, so far as the source goes (not run on a RISC OS machine: see below).
>
> *What happens:* `SWI Squash_Compress` with `R0 = 0` (no flag bits), `R3 = 0` (no input) and an output buffer of at least 3 bytes goes to the fast compressor. `c/compress`, `Squash_swi`: `fast = (r->r[0] & (SquashContinue | SquashMoreInput)) == 0 && 3 + r->r[3]*3/2 <= r->r[5]` is true for `r3 = 0`, so `compress_store_ass (input, output, 0, workspace)` is called. In `s/comp_ass` the routine reads the first input byte and subtracts 1 from the length *before* the main loop looks at it:
>
> ```
>         LDRB    previous, [input], #1                   ; First byte
>         SUB     input_length, input_length, #1
>         ...
> main_loop
>         SUBS    input_length, input_length, #1
>         BCC     finished
> ```
>
> With a length of 0 the first `SUB` gives &FFFFFFFF, the `SUBS` that follows gives &FFFFFFFE with the carry set, and the loop goes on for about four billion bytes: it reads the input far beyond the caller's buffer and writes compressed output far beyond the output buffer.
>
> *How it was found:* the module was built with GCC and run on an ARM interpreter (the C and assembler of the sources, nothing changed); the call with an empty input never returned. The restartable code (`R0` bit 1 set, then a call with `R0 = 1`, `R3 = 0`) handles an empty input: it gives no output at all (not even the three header bytes, which `Squash_Decompress` would then refuse as corrupt). It was **not** run on a RISC OS machine, because the failure overwrites memory.
>
> *A fix:* add `&& r->r[3] > 0` to the condition for the fast case in `Squash_swi` (an empty input then takes the restartable code); or test the length before the first `SUB` in `compress_store_ass`. The documentation (`Doc/interface`) does not say what an empty input should give.
>
> *An aside, not a bug:* for an input long enough to fill the code table (24000 bytes of mixed data) the fast code and the restartable code give different streams (19169 and 17078 bytes). Both decompress to the input, and `gzip -dc` accepts both (the format is that of `compress -b 12`).
