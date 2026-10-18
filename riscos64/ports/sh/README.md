# sh: a shell for RISC OS 64

A POSIX-style shell for UnixLib 64, written for a system with no `fork`: every command runs to its end before the next starts (see the comment at the top of `sh.c`). It is what `system ()`-less
programs such as GNU make need as `SHELL`.

* Language: lists, `&&` `||`, pipelines (through scrap files), `if` `while` `until` `for` `case`, functions with `local`, `{ }` and `( )`, here documents, quoting, `$var ${var...}` `$(...)` `` `...` `` `$((...))`,
  pathname expansion, `set -e -x -u -f -n`, `trap ... EXIT`.
* Redirections: `< > >> << <& >&`, `2>/dev/null`, `2>&1`. A program of the SDK gets its streams from the system variables `Sh$Stdin`, `Sh$Stdout` and `Sh$Stderr` (read by the C library, `riscos_syscalls.c`);
  a command of the operating system is run with `{ > file }` if the system takes it, else `*Spool` (its error output cannot be separated and its input is not available).
* Builtins: `cd echo printf test [ read export readonly set shift unset eval . source exit return break continue : true false local trap wait type command exec umask ulimit sleep expr`, and the utilities RISC OS lacks:
  `rm cp mv mkdir rmdir cat touch chmod basename dirname`.
* Not there: `&` runs the command at once (to its end), no job control, no `getopts`, no aliases, no `$(<file)`, `trap` for signals does nothing.
* `tests/`: `sh_t1` (with `tprog.c`, a program that shows its arguments and input) and `sh_t2` (builtins only; `t2.expected` is the output of the host's `dash`; `cmp2.py` compares), `Makefile.sh` (GNU make with `SHELL = ./sh`).
  Jobs `port_sh`, `port_sh2s` and `port_make_sh` of `tools/run_on_service.py`. Result on Pyromaniac (18 October 2026): `sh_t2` is identical to dash's output; `sh_t1` and the Makefile give the expected results.
* Build: `aarch64-riscos-gcc -munixlib -O2 -o sh,ff8 sh.c` (`ports/build-ports.sh` does it).
