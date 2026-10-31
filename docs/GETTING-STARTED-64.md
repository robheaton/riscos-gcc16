# Getting started: GCC for RISC OS 64 (AArch64)

This page takes you from the downloads to a program, a C++ program and a module running on a RISC OS 64 machine, and lists what does not work yet. Every command below was run with the published releases.

**Status: pre-release.** The releases below are tested on the RISC OS Pyromaniac emulator (and the compilers that run on RISC OS 64 also on a RISC OS 64 machine): the module kit, the library and process test packs, the shell against `dash`'s expected output, the tools (`tar`, `gzip`, `xz`, `zstd`, `jq`, the coreutils scripts), and GNU `make`, `sed`, `Lua` and `zlib` built by the compiler running on Pyromaniac (the native compiler is the preview release `riscos64-native-0.13`; the newer `riscos64-native-0.15`, with C++, and `riscos64-native32-0.1` are described in [NATIVE-ON-RISCOS64.md](NATIVE-ON-RISCOS64.md)). It is one person's work: expect rough edges, and please report them.

## What you need

* A Linux x86-64 PC with glibc 2.38 or newer, to build programs (the cross compiler).
* Something to run them on (RISC OS Pyromaniac, or a RISC OS 64 system), and a way to copy files to it (a file share, a USB stick).

## Install

Download from the [releases page](https://github.com/robheaton/riscos-gcc16/releases?q=%22RISC+OS+64%22&expanded=true) (names start `riscos64-`; they are marked pre-release, so GitHub's "Latest" stays the 32-bit compiler):

| Download | What it is | You need it to |
|---|---|---|
| [`riscos64-toolchain-0.14`](https://github.com/robheaton/riscos-gcc16/releases/tag/riscos64-toolchain-0.14) (60 MB) | the cross compiler for Linux | build programs and modules |
| [`riscos64-gnu-tools-15`](https://github.com/robheaton/riscos-gcc16/releases/tag/riscos64-gnu-tools-15) (13 MB) | `!GNU`: the shell, `sed`, `grep`, `gawk`, `make`, `tar`, `gzip`, `xz`, `zstd`, `zip`, `jq`, `less`, `m4`, `bc` ... | scripts and tools on RISC OS 64 |
| [`riscos64-coreutils-9.5-11`](https://github.com/robheaton/riscos-gcc16/releases/tag/riscos64-coreutils-9.5-11) (26 MB) | `!CoreUtils`: the 102 GNU coreutils programs (`ls`, `cp`, `sort` ...) | the same |
| [`riscos64-libs-1`](https://github.com/robheaton/riscos-gcc16/releases/tag/riscos64-libs-1) (0.7 MB) | zlib, libpng, libjpeg-turbo, expat, PCRE2 | linking against them |
| [`riscos64-native-0.15`](https://github.com/robheaton/riscos-gcc16/releases/tag/riscos64-native-0.15) (57 MB) | **preview:** the C and C++ compilers running on the target itself, with a C++ and a zlib example ([NATIVE-ON-RISCOS64.md](NATIVE-ON-RISCOS64.md)); [`riscos64-native32-0.1`](https://github.com/robheaton/riscos-gcc16/releases/tag/riscos64-native32-0.1) (41 MB) is the same for programs for 32-bit RISC OS | compiling on the target |
| [`riscos64-samples-1`](https://github.com/robheaton/riscos-gcc16/releases/tag/riscos64-samples-1) (21 KB) | four sample projects with Makefiles: C, C++, a command-line tool, a module | copying a working start |

On Linux:

```bash
tar -xf aarch64-riscos-gcc-16.2.0-0.14-linux-x86_64.tar.xz
export PATH=$PWD/aarch64-riscos-gcc-16.2.0-0.14-linux-x86_64/bin:$PATH
aarch64-riscos-gcc-16.2.0-0.14-linux-x86_64/selftest.sh      # builds three small programs and checks them: "Self test passed."
```

## Your first program

```c
/* hello.c */
#include <stdio.h>
int main (int argc, char **argv) { printf ("hello, argc=%d\n", argc); return 0; }
```

```bash
aarch64-riscos-gcc -munixlib -O2 hello.c -o hello,ff8
```

* `-munixlib` links UnixLib 64: Unix file names, processes (`system`, `popen`, `posix_spawn`), signals, threads, sockets. Without it you get the plain C library and RISC OS file names as they are.
* The `,ff8` on the end is the RISC OS file type of a 64-bit program (&FF8).

Copy `hello,ff8` to the RISC OS 64 system and run it: `*Run hello a b` prints `hello, argc=3`.

**If `Run` opens the file in an editor instead of running it, it has no file type:** `*SetType hello &FF8`. File systems that keep the type in the name (`,ff8`) keep it when you copy; a program written on the target itself (for example by the native linker) needs a `*SetType`.

## C++

```bash
aarch64-riscos-g++ -munixlib -O2 prog.cc -o prog,ff8        # add -pthread for std::thread
```

C++20 with the standard library, exceptions and threads. (A C++ module cannot use exceptions yet: see below.)

## A module

A module is written in C with a CMHG file (the same language as the 32-bit `cmunge`), turned into a header by `cmunge64`:

```bash
cmunge64 -d mymodh.h -o mymod_hdr.o mymod.cmhg
aarch64-riscos-gcc -mmodule -O2 mymod.c mymod_hdr.o -o MyMod,ffa       # ,ffa is the file type of a module
```

On the target: `*RMLoad MyMod`, then use its commands and SWIs. Full examples with CMHG files are in the repository: [`riscos64/kit/examples`](../riscos64/kit/examples) (`hello64`, `swi64`, `kv64`, a key/value store with six SWIs and four commands, and others). The `kv64` example builds like this:

```bash
E=riscos-gcc16/riscos64/kit/examples/kv64
cmunge64 -d kv64h.h -o kv64_hdr.o $E/kv64.cmhg
aarch64-riscos-gcc -mmodule -O2 -I. $E/kv64.c kv64_hdr.o -o Kv64,ffa
aarch64-riscos-gcc -munixlib -O2 $E/kv64test.c -o kv64test,ff8
```

(The [`riscos64-samples-1`](https://github.com/robheaton/riscos-gcc16/releases/tag/riscos64-samples-1) release has a smaller module, with a Makefile, to copy.) Copy both to the target, `*RMLoad Kv64`, run `kv64test`: it prints `SUMMARY 0 failed`. [docs/MODULES64.md](MODULES64.md) describes the module kit.

## The shell and the tools

Unzip `riscos64-gnu-tools-15` and `riscos64-coreutils-9.5-11` so that `Apps/Utilities/!GNU` and `Apps/Utilities/!CoreUtils` are in your Apps directory, and run each application's `!Boot` (or double-click the application): that sets the variables and one alias for each program, so `sh`, `sed`, `grep`, `ls`, `sort` ... are commands. For example:

```
*sh -c "echo one two three | sed s/two/2/"
*gawk "BEGIN { print 21*2 }"
*sh -c "mkdir dd; echo hi > dd/f1; tar cf tt dd; rm -r dd; tar xf tt; cat dd/f1"
```

[docs/USERLAND64.md](USERLAND64.md) has the details.

## File names

UnixLib translates Unix names to RISC OS names: `a/b` is `a.b`, and a name with a suffix such as `foo.c`, `foo.h`, `foo.o`, `foo.s` is the file `foo` in the folder `c`, `h`, `o` or `s`. A dot in a name that is not one of those suffixes is a `/` in the RISC OS leaf name. Names without dots are the safest on a file share, and `-munixlib` is needed for any of this.

## Programs that start programs

A program that calls `system ()`, `popen ()` or `posix_spawn ()` needs `*Set UnixEnv$swap 1` before it runs (the `!Boot` of `!GNU` does it): without it, the kernel loads the new program over the old one and the old one never comes back.

## Known issues and limits

* **No Fortran, no shared libraries, no Toolbox or OSLib yet.** Desktop (Wimp) applications can use [DeskLib64](https://github.com/robheaton/riscos-desklib64). The compiler is for command-line programs, tools and modules first. There is no `fork`; use `posix_spawn`, `system` or `vfork` + `exec`. `bash` does not work (the included `sh` covers most scripts).
* **Command lines can be limited to 255 characters** (by the kernel of some systems). The library passes a longer command from one program to another through a file, so `system ()`, `make`, the compiler driver and the shell work with long argument lists; keep commands that you type or put in an Obey file short, and use `@file` response files with the compiler and linker.
* **POSIX timers:** a program that uses `timer_create` and then ends can take the program that started it with it (the command prompt appears instead); threads, signals, `alarm` and the process calls return to their parent normally. We are looking at it.
* **File names and file systems:** keep to names without dots on a file share, and use a local disc or a RAM disc for build trees; if `tar` says a file is "in the future", check the clocks of the machines involved.
* **Modules:** they cannot be Wimp tasks, and C++ exceptions are not supported in a module; see [docs/MODULES64.md](MODULES64.md) for the rest.
* **Memory:** a program's heap is a dynamic area that reserves its whole maximum size. A program that only starts other programs can be linked with `-Wl,--defsym=__ul64_heap_max=16M` to leave room for the program it starts.

## Where to find more, and how to report problems

* [docs/AARCH64.md](AARCH64.md) the compiler, [docs/MODULES64.md](MODULES64.md) modules, [docs/USERLAND64.md](USERLAND64.md) the shell and tools, [docs/UNIXLIB64.md](UNIXLIB64.md) the C library, [docs/RISCOS64.md](RISCOS64.md) what was tested.
* Problems and questions: [github.com/robheaton/riscos-gcc16/issues](https://github.com/robheaton/riscos-gcc16/issues). Please say which release you used, what you ran, and what happened (a photograph of an error box is fine).
