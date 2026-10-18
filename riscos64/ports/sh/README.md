# sh: a shell for RISC OS 64

A POSIX-style shell for UnixLib 64, written for a system with no `fork`: every command runs to its end before the next starts (see the comment at the top of `sh.c`). It is what `system ()`-less
programs such as GNU make need as `SHELL`.

* Language: lists, `&&` `||`, pipelines (through scrap files), `if` `while` `until` `for` `case`, functions with `local`, `{ }` and `( )`, here documents, quoting, `$var ${var...}` `$(...)` `` `...` `` `$((...))`,
  pathname expansion, `set -e -x -u -f -n`, `trap ... EXIT`.
* Redirections: `< > >> << <& >&`, `2>/dev/null`, `2>&1`. A program of the SDK gets its streams from the system variables `Sh$Stdin`, `Sh$Stdout` and `Sh$Stderr` (read by the C library, `riscos_syscalls.c`);
  a command of the operating system is run with `{ > file }` if the system takes it, else `*Spool` (its error output cannot be separated and its input is not available).
* Builtins: `cd echo printf test [ read export readonly set shift unset eval . source exit return break continue : true false local trap wait type command exec umask ulimit sleep expr`, and the utilities RISC OS lacks:
  `rm cp mv mkdir rmdir cat touch chmod basename dirname`.
* Scripts: a file that is text (or starts with `#!`) found in PATH is run by this shell (`sh script args`). A command that is not a file in PATH is passed to the operating system only if it is a RISC OS command (a list in `is_ros_command`, an `Alias$` variable, or a name starting with `*`); anything else is "not found" (an unknown command can hang the operating system waiting for the keyboard).
* Not there: `&` runs the command at once (to its end), no job control, no `getopts`, no aliases, no `$(<file)`, `trap` for signals does nothing.
* `tests/`: `sh_t1` (with `tprog.c`, a program that shows its arguments and input) and `sh_t2` (builtins only; `t2.expected` is the output of the host's `dash`; `cmp2.py` compares), `Makefile.sh` (GNU make with `SHELL = ./sh`).
  Jobs `port_sh`, `port_sh2s` and `port_make_sh` of `tools/run_on_service.py`. Result on Pyromaniac (18 October 2026): `sh_t2` is identical to dash's output; `sh_t1` and the Makefile give the expected results.
* Build: `aarch64-riscos-gcc -munixlib -O2 -o sh,ff8 sh.c` (`ports/build-ports.sh` does it).

## What a configure script needs (zlib 1.3.1, 18 October 2026)
`./configure` of zlib ran to its end under this shell with GNU sed, grep and gawk and a stand-in compiler (`CC=./mockcc`; the compiler is not on the machine). It called, and did not find: `uname`, `date`, `tee` (coreutils),
`ar`, `ranlib`, `nm` (binutils) and `gcc` / `cc`. So coreutils is the next port.

## Conformance tests against dash (18 October 2026)
`tests/conf/s01`-`s08` are scripts of the shell language (descriptors above 2 with `exec`, getopts, special parameters, every form of `${...}`, control flow, functions, quoting, test and arithmetic, builtins);
`s0N.expected` is what the host's `dash` prints for them; `cmpconf.py` compares the output of the job `port_shconf` with it. All eight are identical. `runcfgs` runs the real autoconf-generated `configure` scripts of
GNU make and gawk with `--version`, `--help` and a bad option (job `port_shcfg2`): the 144 lines are identical to dash's.
Found with them: descriptors 3-19 (`exec 5>file`, `>&5`, `<&6`, `N>&-`), `getopts`, `$LINENO` (without it configure runs a `sed` over itself, which took four minutes on the emulator), a `case` item with no commands,
`"$*"` joined with the first character of `IFS`, `printf %b`, `read` trimming, one file written by two sinks (`>f 2>&1`), error messages going through `2>`, and syntax errors that name the line.
`SH_PROFILE=1` prints the commands that take more than 0.2 s.
