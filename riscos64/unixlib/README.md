# UnixLib 64: Unix programs on RISC OS 64

The 32-bit port gives programs UnixLib: Unix file names (`foo/bar.c` for the file `c.bar` in the directory `foo`), directories, the current directory, `UnixEnv$...` settings. This folder is the beginning of the same for
64-bit programs: `libunixlib64.a`, on top of the SDK's picolibc (option B of [`docs/UNIXLIB64.md`](../../docs/UNIXLIB64.md): picolibc plus RISC OS glue plus the parts of UnixLib that programs need).

A program uses it with **`-munixlib`** (`aarch64-riscos-gcc -munixlib prog.c -o prog,ff8`, with the real target of [`../target/`](../target/) or the SDK's wrapper `../sdk/bin/aarch64-riscos-gcc`). Without the option nothing
changes: names are used as they are given.

| File | What |
|---|---|
| `src/riscosify.c`, `src/unixify.c` | the file name translation of UnixLib, Unix to RISC OS and back (`__riscosify`, `__unixify`, `__sfixinit`, the special directories); the sources of UnixLib (revised BSD licence, [`licenses/UnixLib-COPYING.txt`](../../licenses/UnixLib-COPYING.txt)) changed for 64-bit registers and without the Shared C Library and SOLoader variants |
| `src/control.c` | `__get_riscosify_control`, `__set_riscosify_control`: the flags that tell the translation what to do |
| `src/init.c` | the start-up: `UnixEnv$<program>$nonametrans`, `UnixEnv$<program>$sfix` (default `c:cc:f:h:i:ii:l:o:p:s:y`) and the `UnixFS$/name` special directories; called by the SDK's start-up code (`__ul64_init`) |
| `src/dirent.c` | `opendir`, `readdir`, `rewinddir`, `closedir` over `OS_GBPB 12` |
| `src/cwd.c` | `chdir`, `getcwd`, `rmdir` |
| `src/statvfs.c` | `statvfs` (`OS_FSControl 55`) |
| `include/unixlib/local.h` | the flags and declarations of the translation (from UnixLib) |
| `build.sh` | builds `out/libunixlib64.a` |
| `tests/t_riscosify.c`, `tests/expected-riscosify.pyromaniac.txt` | the translation of 40 names in both directions |
| `tests/t_files.c` | creating, reading, renaming and removing files and directories by Unix names, the current directory, `statvfs` (22 checks) |

How it fits in: the SDK's system calls (`sdk/crt/riscos_syscalls.c`: `open`, `unlink`, `rename`, `mkdir`, `stat`) call `__riscosify_std` if the library is linked in (a weak reference) and set the file type of a new
file (Text, or the `,xyz` of its name). `sdk/crt/stubs/` has the functions that fail with `ENOSYS` without UnixLib 64 (`opendir`, `chdir` ...), so that programs that mention them still link.

Not done yet: the merging of suffix directories into the listing of their parent (`src/c/main` as `src/main.c`), `.` and `..` in directory listings, `seekdir`/`telldir`, symbolic links, the environment and the process
model (`environ`, `exit` handlers), `termios`, signals, threads, `fork`. See `docs/UNIXLIB64.md` for the plan.

Tested on RISC OS Pyromaniac (17 October 2026): `tests/t_files.c` 22 of 22 checks.
