# The shell and the GNU tools on RISC OS 64 (18 October 2026)

This page is for people who **use** the programs in the two packages [`!GNU`](https://github.com/robheaton/riscos-gcc16/releases/tag/riscos64-gnu-tools-2) and [`!CoreUtils`](https://github.com/robheaton/riscos-gcc16/releases/tag/riscos64-coreutils-9.5-2), and for people who want to **port** another Unix program to RISC OS 64 with the GCC 16 tool chain. They are built with `aarch64-riscos-gcc` and UnixLib 64 ([UNIXLIB64.md](UNIXLIB64.md)), run on RISC OS 64 and on RISC OS Pyromaniac, and are **pre-releases: tested on Pyromaniac only so far**.

* [1. What is in the packages](#1-what-is-in-the-packages)
* [2. Installing and setting up](#2-installing-and-setting-up)
* [3. How things work on RISC OS: the rules that differ from Unix](#3-how-things-work-on-risc-os)
* [4. The shell `sh`](#4-the-shell-sh)
* [5. The tools, one by one](#5-the-tools-one-by-one)
* [6. What does not work](#6-what-does-not-work)
* [7. Variables](#7-variables)
* [8. Porting a Unix program](#8-porting-a-unix-program)
* [9. Testing and building](#9-testing-and-building)
* [10. Troubleshooting](#10-troubleshooting)

## 1. What is in the packages

| Package | Contents |
|---|---|
| **`!GNU`** (`gnutools_2_aarch64.zip`, 5 MB) | `sh` (a POSIX shell written for this system), GNU `sed` 4.9, `grep` 3.11 (`egrep` and `fgrep` as aliases), `gawk` 5.3.1 (`awk`), `make` 4.4.1, `diff` `cmp` `diff3` `sdiff` (diffutils 3.10), `find` `xargs` (findutils 4.10.0), `patch` 2.7.6, `tar` 1.35, `gzip` 1.13 (`gunzip`, `zcat`), Lua 5.4.7, `bzip2` 1.0.8, `sqlite3` 3.47.2 |
| **`!CoreUtils`** (`coreutils_9.5-2_aarch64.zip`, 22 MB) | GNU coreutils 9.5: 102 programs, one file each, in the way of the 32-bit package `coreutils_8.30-2_arm.zip`: `ls cp mv rm mkdir cat sort uniq tr cut wc head tail tee seq expr date uname env printenv md5sum sha256sum cksum stat basename dirname fold nl od ...` (`install` is `ginstall`) |

Each application is a folder `Apps.Utilities.!GNU` / `!CoreUtils` with one program per tool (file type &FF8, an AIF64 program), a `!Boot` that sets `GNU$Dir` / `CoreUtils$Dir` and one `Alias$NAME` per program, `!Run`, `!Help` and the licences. Both are meant to be used together: the shell finds both.

## 2. Installing and setting up

1. Unzip the packages so that `Apps.Utilities.!GNU` and `Apps.Utilities.!CoreUtils` are in your Apps directory (or anywhere).
2. **Run each application once** (double-click, or `Obey` its `!Boot`) in every session in which you want to use them. `!Boot` sets:
   * `GNU$Dir` and `CoreUtils$Dir`, the folders of the programs;
   * `UnixEnv$swap` to 1: **a program can only start other programs (`system`, `popen`, `posix_spawn`, `vfork` and `exec`, `make`, `xargs`, `find -exec`, the shell) with this set** (see [3.2](#32-programs-that-start-programs));
   * `Alias$NAME` for each program, so that `sort`, `sed`, `tar` ... can be typed on the command line, used in Obey files and started by RISC OS itself.
3. Run a program: `sort -r data` on the command line, or in the shell: `Run <GNU$Dir>.sh` and type commands, or `sh script args`.

The shell needs no setup of its own. Its default `PATH` is `.:/bin:/usr/bin` followed by `/<CoreUtils$Dir>` and `/<GNU$Dir>` (when those variables are set), and `HOME` is `/` unless it is set.

Memory: each program asks for its own application space with the `WimpSlot` in its alias (about 600 KB to 1.6 MB); big inputs live in the heap, which is a dynamic area that grows as far as the machine allows.

## 3. How things work on RISC OS

### 3.1 File names

The programs are Unix programs: they use Unix names. UnixLib 64 translates them to RISC OS names ([UNIXLIB64.md](UNIXLIB64.md) has the full rules). The ones that matter in daily use:

| Unix name | RISC OS name | Note |
|---|---|---|
| `/` | `$` | the **root of the disc** that is current: `/name` is `$.name`, not the current directory |
| `./name`, `name` | `@.name` | relative to the current directory; use `./name` when you mean it |
| `../x` | `^.x` | |
| `/tmp/x` | `<Wimp$ScrapDir>.x` | `UnixLib` makes `$.tmp` for `Wimp$ScrapDir` when the desktop has not set it |
| `dir/file.c` | `dir.c.file` | a name with a **suffix** in the list `c cc f h i ii l o p s y` is swapped: the suffix becomes a folder |
| `file.txt`, `Makefile.in` | `file/txt`, `Makefile/in` | any other dot becomes a slash in the leaf name |
| `/<Wimp$ScrapDir>/x` | `<Wimp$ScrapDir>.x` | a system variable in a name |
| `file,fff` | the file with type &FFF | a `,xyz` ending (hex) sets or shows the file type |

Things to know:

* **The suffix list can be changed** for a program: `Set UnixEnv$sh$sfix c:h:o` (the program's name between the `$`s), or for all programs `Set UnixEnv$sfix ...`. `Set UnixEnv$nonametrans 1` switches the translation off.
* A name that has a suffix of the list in the **middle** (`zconf.h.in`, `foo.o.bar`) is taken for a RISC OS name already and left as it is: such a file cannot be opened by that name. This is the behaviour of UnixLib on 32-bit RISC OS too. Packages that have such names (zlib's `zconf.h.in`) need a change of the name, or `UnixEnv$...$sfix` without the suffix.
* **Names are not case sensitive**, so `Makefile` and `makefile` are the same file, and `libtest64` and `LibTest64` too.
* Directories are listed with the suffix folders merged (`c.main` is listed as `main.c`). `.` and `..` are not listed.
* There are **no links**: `ln` makes a copy where it can, `ln -s` fails; `readlink` fails.
* Inode numbers are made from the full name of the file (so two names of one file are the same inode); owner and group are always `user`/`root` (uid 1).

### 3.2 Programs that start programs

RISC OS has no `fork`. A program that starts another (`system ()`, `popen ()`, `posix_spawn`, `vfork` and `exec`, `exec` alone) **runs it to its end** and then goes on: the caller's memory is saved in a dynamic area, the other program is loaded and run, and its exit puts the caller's memory back. This needs `UnixEnv$swap` set to 1 (the `!Boot` of both packages does that). The consequences:

* **Everything is sequential.** `a | b` runs `a` to its end with its output in a scrap file, then `b` with that file as input. `cmd &` runs `cmd` to its end first. A program that never ends in a pipeline (`yes | head`) hangs. Programs that need two processes at once (a server and a client) do not work.
* A program started by another gets the **environment** of the caller (the variables it exported) and the **current directory** it had before; the directory is put back when the command has ended, so `cd` in a child never changes the parent's.
* **Standard streams.** A program of this tool chain reads `Sh$Stdin`, `Sh$Stdout`, `Sh$Stderr` (file name, `null`, or for stderr `stdout`) at its first use of descriptor 0, 1 or 2; that is how the shell redirects them. A command of the operating system (`Cat`, `Copy` ...) can have its screen output redirected only with `{ > file }` on a RISC OS that takes it, or `*Spool`; its input cannot be redirected.
* Each start of a program costs about 0.4 s on the Pyromaniac emulator (much less on a real machine); the shell and `make` are slower on it than they will be.
* Command lines are RISC OS command lines: an argument with a space or a quote is put in double quotes (a quote inside is doubled), a line feed is written `DEL n`. Long command lines work, but RISC OS limits what `OS_CLI` takes (1024 bytes on RISC OS 5); `sysconf (_SC_ARG_MAX)` says 32768 so that `xargs` and `make` build long lines.

### 3.3 Other differences

* A write to a file that is open for writing elsewhere fails (`Device or resource busy`): a file can be open for writing once. The shell takes care of that for its own redirections.
* Time stamps of files come from the file system (centiseconds); the Pyromaniac build service stamps uploaded files an hour in the future, which `make` and `tar` complain about.
* `/dev/null` is an empty scrap file. `/dev/tty` and the other devices do not exist.
* Sockets exist (BSD sockets on the Internet module, IPv4) but have not been tried on a real module; none of these tools uses them.

## 4. The shell `sh`

A POSIX-style shell written for this system (`ports/sh/sh.c`, about 2700 lines of C). It is tested against the output of `dash` on eight sections of language tests (identical) and on the real `configure` scripts of GNU make and gawk.

```
sh [-ex] [-c command [name arg ...]] [script arg ...]
sh -n script          read the commands, do not run them (syntax check)
```

**Language:** lists with `;` `&` `&&` `||`, pipelines, `!`, `if/elif/else`, `while`, `until`, `for`, `case` (with `|` and empty items), functions (`name () command`, with `local` and `return`), `{ ...; }` and `( ... )` (a subshell keeps its variables and directory to itself), here documents (`<<`, `<<-`, quoted or not), quoting (`'...'`, `"..."`, `\`), comments, line continuation, `$var`, `${var}` with all of `:-` `-` `:=` `=` `:+` `+` `:?` `?` `#` `##` `%` `%%` and `${#var}`, `$(...)`, `` `...` ``, `$((...))` (the C operators on integers, variables by name, `?:`, `=`), `$@ $* $# $? $$ $! $- $0 $1...`, `$LINENO`, tilde (`~`), pathname expansion (`*` `?` `[...]`, `set -f` switches it off), `IFS` splitting.

**Redirection:** `< > >> << <& >& <>`, with descriptors 0 to 19: `exec 5>file`, `echo x >&5`, `read a <&6`, `exec 6<&-`, `2>&1`, `>&2`, `2>/dev/null`, `exec >file` (stays for the rest of the script). Descriptors above 2 are for the shell's own commands and builtins; a program started by the shell gets descriptors 0, 1 and 2 only.

**Builtins:** `: . source break continue cd command echo eval exec exit export false getopts hash local printf pwd read readonly return set shift test [ trap true type umask unset wait alias unalias ulimit` (`alias`, `hash`, `ulimit`, `umask`, `wait` and `chmod` do nothing useful). `trap '...' EXIT` runs a command at the end; other signals are ignored. `set` takes `-e -x -u -f -n`. `echo` takes `-n` and always reads backslash escapes, as `dash` does; `printf` takes `%s %b %c %d %i %o %x %X %e %f %g %%` with width and precision.

**Utilities that are builtins when the real program is not in `PATH`:** `rm cp mv mkdir rmdir cat touch basename dirname expr sleep`, because RISC OS has none. With `!CoreUtils` in `PATH`, the real programs are used instead.

**Finding a command:** a function, a builtin, a file in `PATH` (a program is run directly; a text file or a file that starts `#!` is run by the shell itself, as `sh file args`), and last a command of the operating system if its name is a RISC OS command (`Cat`, `Copy`, `Set`, `Obey` ... or a name that has an `Alias$` variable, or one that starts with `*`). Anything else is `sh: name: not found` (an unknown command given to RISC OS can hang it waiting for the keyboard, so the shell never does that).

**Differences from `bash`/`dash` that scripts can trip over:** no job control and no `&` in the background (the command runs to its end at once), no aliases, no `[[ ]]`, no arrays, no `$'...'`, no `${var/a/b}`, no process substitution, `trap` only for `EXIT`, no `time`, `wait` and `kill` do nothing. `read` reads lines; `set -o` is not there.

**Environment of the shell:** `PATH`, `HOME`, `IFS`, `PWD`, `OLDPWD`, `OPTIND`, `OPTARG`, `SH_TMPDIR` (where the scrap files go; default `/tmp`), `SH_PROFILE=1` (prints the commands that take more than 0.2 s). The variables it exports are given to programs as system variables for the time of the command, and `Unix$Environ` names them, so a program sees exactly those in its `environ`.

## 5. The tools, one by one

| Tool | Notes |
|---|---|
| **coreutils** | all the 102 programs build; those that work as on Unix: the text tools (`sort cut tr uniq wc head tail nl fold paste join comm expand tac od`), file tools (`cp mv rm mkdir touch ls stat dd truncate`), sums (`md5sum sha*sum cksum b2sum`), `date uname id whoami env printenv expr seq sleep test basename dirname mktemp`; `ls -l` shows owner `root` and mode from the file attributes. `sort` runs in one thread; `df`, `who`, `users`, `uptime`, `stty`, `tty`, `chown`, `chgrp` have nothing to read or change here |
| **sed, grep, gawk** | GNU, complete (gawk without shared-library extensions; pipes and `system ()` of gawk run through `sh -c`). A big script run over a big file is slow on the emulator (a 480 KB file with a loop took two minutes) |
| **make** | GNU make 4.4.1. Run recipes with the shell: `SHELL = ./sh` (or the full name; the default `/bin/sh` does not exist). The jobserver is off: `-j N` is accepted and the jobs run one at a time. `$(shell ...)` works (through `popen`) |
| **diff, cmp, diff3, sdiff, patch** | as GNU; `diff -r` walks directories; `patch` reads from a file or from standard input |
| **tar** | creates, lists and extracts (the formats of GNU tar); file types are kept as `,xyz` only if you ask for them. A compressed archive (`-z`, `-j`) needs a pipe to another program running at once: use `gzip -c a.tar` and `gzip -d`, as two steps |
| **gzip** | `gzip file`, `-c`, `-d`, `-1` ... `-9`; reads archives made on other systems; `gunzip` and `zcat` are aliases |
| **find, xargs** | `find` with the usual tests; `-exec` and `-execdir` run the command to its end for each file; `xargs` runs the command once per batch, in sequence (`-P` makes no difference) |
| **Lua** | 5.4.7 with its libraries; 16 of the 17 files of its test suite that were run pass (the 17th is too slow for the build service). Scripts need `.lua` names as the program `lua` sees them (`UnixEnv$lua$sfix`) |
| **bzip2** | the library gives the bytes that the host's libbz2 gives; the program compresses, tests and decompresses |
| **sqlite3** | 3.47.2 shell and library without WAL and without extensions; transactions, indexes and rollback work; the database file stays valid between runs |

## 6. What does not work

| What | Why | What to do |
|---|---|---|
| `ln -s`, `readlink`, symbolic links | RISC OS has none | copy the file |
| `yes \| head`, any pipeline with a stage that does not end | the stages run one after the other | give the producer a limit (`yes \| head` becomes `seq 100`) |
| `cmd &` and waiting for it, `wait`, `kill`, job control, `wait` for several | one program at a time | run it in the foreground |
| two programs talking at once (sockets, pipes, FIFOs between processes) | no processes | write the data to a file in between |
| `make -j`, `xargs -P` | same | they run sequentially |
| `chown`, `chgrp`, `chmod` of owner bits, `umask` | RISC OS files have owner and public access only | |
| `df`, `who`, `users`, `uptime`, `stty`, `mount` | no list of file systems, logins or terminals | |
| file names such as `zconf.h.in` | the middle `.h` is read as a suffix folder | see [3.1](#31-file-names) |
| input redirection for a RISC OS command (`Copy a b < x`) | the operating system cannot | use a program of this tool chain |
| the error output of a RISC OS command separate from its output | same | |

## 7. Variables

| Variable | Used by | Meaning |
|---|---|---|
| `GNU$Dir`, `CoreUtils$Dir` | `!Boot`, the shell, `execvp` | folders of the applications; in the default `PATH` as `/<GNU$Dir>` |
| `UnixEnv$swap` | the C library | `1`: programs can start programs. Set by both `!Boot`s |
| `UnixEnv$sfix`, `UnixEnv$NAME$sfix` | UnixLib 64 | the suffixes that are swapped into folders (default `c:cc:f:h:i:ii:l:o:p:s:y`) |
| `UnixEnv$nonametrans`, `UnixEnv$NAME$nonametrans` | UnixLib 64 | `1`: no translation of file names |
| `Wimp$ScrapDir` | UnixLib 64 | what `/tmp` is; made as `$.tmp` when the desktop has not set it |
| `Sh$Stdin`, `Sh$Stdout`, `Sh$Stderr` | the C library (set by the shell) | redirect a program's standard streams: a file name, `null`, and for stderr `stdout` |
| `Unix$Environ` | the C library (set by the starter) | the names of the variables that the program should see in `environ` |
| `PATH`, `HOME`, `IFS`, `SHELL` | the shell, `make`, `system ()` | `system ()` and `popen ()` run `sh -c` with `$SHELL`, else the `sh` found in `PATH`, unless the command starts with `*`, is a RISC OS-only command (`Set`, `Spool`, `RMLoad` ...) or has a `<Var$X>` in it |
| `SH_TMPDIR`, `SH_PROFILE` | the shell | scrap file directory; timing of slow commands |

## 8. Porting a Unix program

What worked for the programs in the packages (all with `aarch64-riscos-gcc -munixlib`):

1. **Configure for the cross compiler:** `../pkg/configure --host=aarch64-riscos --build=x86_64-pc-linux-gnu CC="aarch64-riscos-gcc -munixlib" CFLAGS=-O2 --disable-nls` and `--without-selinux --disable-acl --disable-xattr` where the package has them. `ports/build-gnu.sh`, `build-ports.sh`, `build-lua.sh` are working examples.
2. **GCC 16 uses C23 by default.** Old code with `char *getenv ();`-style declarations or its own `getopt.c` fails; compile such files with `-std=gnu17 -include stdbool.h` (`make CFLAGS="-O2 -std=gnu17 -include stdbool.h"`, as for make and gawk).
3. **gnulib** is in most GNU packages. Its `freadahead.c`, `freadptr.c` and `freadseek.c` do not know picolibc's `FILE`: run `ports/patches/gnulib-riscos64.py DIR` on the source tree. The tests of gnulib (`gnulib-tests/`) do not build; `make -k` builds the programs.
4. **Reconfigure after the SDK changes:** configure decides which functions to replace from what the SDK has *at that time* (`fchdir`, `fdopendir`, `pipe` ...); a build made before a function existed links the package's own copy and then fails with `multiple definition` when the SDK has it. Delete the build folder and configure again.
5. **`fork`:** programs that `fork` and then `exec` in the child can use `vfork` (`-Dfork=vfork`; the child must not change data the parent uses: see `patches/findutils-4.10.0-riscos64.patch`). Programs that need the child to run at the same time cannot be ported.
6. **Pipes:** `pipe ()` is a scrap file and fits "write in the child, read in the parent". gawk is built with `-DPIPES_SIMULATED` and a small patch so that it uses `popen`; make's `$(shell)` has a patch for the same reason.
7. **File names** in the package's own files (dots, suffixes) are the usual trouble: see [3.1](#31-file-names).
8. **Test on Pyromaniac** with `tools/run_on_service.py` (a job is a list of commands and the files it needs; see the jobs `port_*` in it). Lines with `{` start RISC OS redirection and single quotes are not removed: put programs with such arguments in a script and run the script with `sh`.

## 9. Testing and building

* The sources are in [`riscos64/ports`](../riscos64/ports): `sh/` (the shell, its tests in `sh/tests`, including the comparison with `dash`), `patches/`, `build-gnu.sh` (sed, grep, gawk, diffutils, findutils, patch, tar, gzip, coreutils and their packages), `build-ports.sh` (make, bzip2, SQLite), `build-lua.sh`, `package-gnu.sh`, `package-coreutils.sh`, `userland-tests/` (scripts for the tools).
* Jobs of `riscos64/tools/run_on_service.py` for the Pyromaniac build service: `port_sh`, `port_shconf`, `port_shcfg2` (the shell against `dash`), `port_cu2` (coreutils), `port_ux` (diff, patch, gzip, tar, find, xargs), `port_final` (everything from the package layout), `port_sed`, `port_grep`, `port_gawk`, `port_make_sh`, `port_bz2`, `port_sqlite`, `lua_*`.
* To build everything: `ports/build-gnu.sh PREFIX WORKDIR` with the tool chain in `PREFIX` (see [CROSS-COMPILER.md](CROSS-COMPILER.md) and `riscos64/target/README.md`).

## 10. Troubleshooting

| You see | Because |
|---|---|
| `sh: name: not found` | the program is not in `PATH`: run `!CoreUtils`/`!GNU`, or give the path (`./name`). Names are looked up as files, not through `Alias$` |
| a command does nothing or the machine waits | the command is a RISC OS command or an unknown name given to the operating system; use a program of the tools |
| `Device or resource busy` | the file is open for writing elsewhere; close the other program or use another name |
| `Function not implemented` | a call RISC OS cannot do (`fork`, `link`, `symlink`, `mkfifo`, `chown`) |
| `File '$.name' not found` for a program that is in the current directory | `/name` is the root of the disc: write `./name` |
| a program starts but the caller never comes back | `UnixEnv$swap` is not 1; run the `!Boot` |
| `time stamp ... is ... s in the future` from tar and make | the clock and the stamps of files differ (the build service); harmless |
| a huge script is slow | the emulator is; a real machine is much faster |
| `cannot create sh_fd5: Device or resource busy` and similar | two programs open one file for writing; the shell shares one open file for `>f 2>&1`, but a program of its own and the shell cannot |

Bugs and questions: the [issue tracker](https://github.com/robheaton/riscos-gcc16/issues) of the project.
