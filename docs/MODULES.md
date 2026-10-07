# modkit: building RISC OS modules with this tool chain (experimental)

> **Experimental.** Relocatable modules are the one thing the EABI tool chain could not build: GCCSDK's 4.7.4 compiler (APCS, SharedCLibrary) is still the only supported way for modules that matter. `modkit` shows that GCC 16 can build working modules **without any C library**,
> and since 16.2.0-14 it is used the way GCCSDK 4.7.4 was: **`gcc -mmodule`** and **`cmunge`**. It has been run on real hardware for small modules (below). **This page describes the repository, which is ahead of the last release:** the C library of the module kit (about 165 functions instead of 30, with `stdio` files),
> the CMHG support and the `module-is-runnable` start code described below are in the source and pass the host tests, and ship with the next release (16.2.0-15); 16.2.0-14 has the smaller kit. Read the limits first.

## The commands (the same as with GCCSDK 4.7.4)

| GCCSDK 4.7.4 | This tool chain (16.2.0-14 and later) |
|---|---|
| `cmunge -tgcc -32bit -p -d header.h -o header.o file.cmhg` | the same command: `cmunge` is in the tool chain's `bin/` (it is modkit's module header generator behind CMunge's command line; the options that make no sense here are refused, not ignored) |
| `gcc -mmodule -O2 -c file.c` | the same: `-mmodule` means ARMv6, soft float, ARM state, freestanding, no PIC, no stack protector, no unwind tables, and the headers of modkit instead of UnixLib's; `__TARGET_MODULE__` and `__TARGET_SCL__` are defined, `__TARGET_UNIXLIB__` is not |
| `gcc -mmodule -o Module,ffa file.o header.o` (the linker writes the module) | **the same command**: the driver links with the module linker script and `libmodkit.a` (the tool chain has both) and then runs `modreloc`, so the output is the flat module image. An output named `*.elf` stays an ELF file (for a debugger or a simulation) |
| the Shared C Library through its stubs | **no C library of the usual kind**: `libmodkit.a` is a small library of its own (about 165 functions, [listed below](#the-c-library-of-modkit)): the string, `ctype`, `stdlib`, `time`, `setjmp`, `stdio` (`printf`, `sscanf` and files), `_swi` / `_swix` and `_kernel_*` functions that modules use, not floating point |
| `-lOSLib32` / `-lOSLibH32` (OSLib's SWI veneers) | **`arm-riscos-gnueabihf-mkoslib -I <OSLib>/oslib -o oslibv.c --from-objects main.o`** writes the veneers of exactly the OSLib functions that your objects use (from OSLib's own headers: the comment above each function says which SWI and which registers); compile `oslibv.c` and link it. GCCSDK's prebuilt OSLib archives are for the old ABI and cannot be linked |

```bash
export PATH=<tool chain>/bin:$PATH
cmunge -tgcc -32bit -p -d header.h -o header.o module.cmhg
arm-riscos-gnueabihf-gcc -mmodule -O2 -Wall -c main.c -o main.o
arm-riscos-gnueabihf-gcc -mmodule -o MyModule,ffa main.o header.o     # the module: the driver runs modreloc after the linker
```

A makefile that has `.cmhg.o` and `.cmhg.h` rules with `cmunge -tgcc -32bit -p` and `LDFLAGS = -mmodule` works as it is, apart from the library (below). `module.mk` (below) has it all as make rules.
How the module is made: the linker writes an ELF file, and `arm-riscos-gnueabihf-modreloc` (a small C program; the driver runs it, `gcc -mmodule` only) replaces it by the flat image with the table of addresses that the module relocates itself with (the reason is under Limits). A partial link (`-r`) is not turned into a module.

## What can be built today, and what cannot

**It works for** small and medium C modules that need little from a C library: SWI handlers and decoding tables, `*commands` with international help, service calls, vector, IRQ, callback and generic handlers, OSLib and `_swix` calls, `malloc` from the RMA, `printf` and `sscanf`, files (`fopen` and the rest of `stdio`, [with its rules](#stdio-files-and-streams)), modules that can also be run as programs. Modules of that kind ran on the Pi (a service-call and SWI module, a `TickerV` and callback module, a network module of 800 lines, and the module that the self-test builds, loads and runs).

**What a module may need, and where it stands:**

| A module needs | State today |
|---|---|
| **C library** | [About 165 functions](#the-c-library-of-modkit): `string.h`, `ctype.h`, `stdlib.h` (`malloc` from the RMA, `strtol`, `qsort`, `rand`, `getenv`, `atexit` ...), `time.h`, `setjmp.h`, `locale.h`, `limits.h`, `stdint.h`, `assert.h`, `errno.h`, `stdio.h` (`printf`, `sscanf`, files: `fopen`, `fgets`, `fprintf`, `fscanf` ... and the streams `stdin`, `stdout`, `stderr`; [the rules](#stdio-files-and-streams)), `swis.h` (`_swi`, `_swix`) and `kernel.h` (`_kernel_*`). **Not there:** floating point in `printf` and `scanf`, `tmpfile`, `tmpnam`, `gets`, `math.h`, `signal`, `system`. A call that is not there is an undefined symbol at the link. |
| **CMHG** | The directives and options listed in the header of `bin/mkmodhdr.py`: title, help and date strings, initialisation and finalisation, service calls, SWI chunk, decoding table and handler, `*commands` (minimum and maximum arguments, GSTrans map, help and syntax text, `international:`, `add-syntax:`, `configure:`, `status:`, `fs-command:`), `international-help-file`, `module-is-runnable`, vectors, IRQ handlers and generic veneers. **Not there** (`cmunge` stops with an error): `event-handler`, `library-enter-code`, `library-initialisation-code`, `help:`, and the handler options of the `*-handlers:` directives. |
| **Floating point** | `float` and `double` compile, and link (the soft-float helpers of `libgcc` are linked after the kit's library: no instruction of the VFP is used). **Not tested on hardware.** |
| **C++** | Compiles with `-mmodule -fno-exceptions -fno-rtti`, but nothing runs static constructors, and there is no `operator new` or `delete`: do not use it yet. |

**How much of the OS that is.** The RISC OS Open sources (the BCM2835 subset) have 86 components that build assembler modules, which no C compiler helps with, and 66 that build C modules. [`tools/scan-os-modules.py`](../tools/scan-os-modules.py) reads the C sources of those 66 (the calls of C library functions, comments and strings left out) and their CMHG files and holds them against what the kit has: **all 66 use only C library functions that the kit has** (51 before the `stdio` file functions: the 15 others used `fopen`, `fclose` and the like; with the 28 functions of 16.2.0-14 the figure was 4), **58 of the 66 have a CMHG file that `cmunge` accepts** (27 with 16.2.0-14; the others use `event-handler`, `library-enter-code`, `library-initialisation-code` and `help:`), and **the same 58 pass both**. One of the 66 uses floating point and none uses C++. `modkit/tests/os-cmhg-corpus.py` runs the CMHG files of the 61 components that have one through `cmunge` and through the real CMunge and compares what the module headers say: where both accept a file the headers are the same. A real module also has to compile with its own headers (most of that source is written for the Norcroft C compiler), so these figures say what the library and the header generator would allow, not what builds untouched; and 51 of the 66 call `_swix`, which behaves here as the Shared C Library's does only as far as `libtest` checks it.

**The plan**, in this order and without promises: `stdio` files checked on hardware; floating point checked on hardware; C++ (static constructors, `new` and `delete`); the CMHG directives that are still refused. Bigger or older modules should keep using GCCSDK 4.7.4.

## The C library of modkit

One object per group of functions, so a module pays for what it uses (a module with `sprintf` and `memcpy` takes about 3 KB of library). It is freestanding; its only state is what C itself has (`errno`, the `rand` seed, the `strtok` position, the static buffers of `asctime`, `gmtime` and `strerror`) the list of exit functions, the open files and the three standard streams. Where C leaves a choice, the answer is glibc's, and the tests compare with glibc (below).

| Header | Functions and notes |
|---|---|
| `<string.h>` | `memcpy`, `memmove`, `memset`, `memcmp`, `memchr`, `memccpy`, `strlen`, `strnlen`, `strcpy`, `strncpy`, `strlcpy`, `strcat`, `strncat`, `strlcat`, `strcmp`, `strncmp`, `strcoll`, `strxfrm`, `strchr`, `strrchr`, `strstr`, `strspn`, `strcspn`, `strpbrk`, `strtok`, `strtok_r`, `strsep`, `strdup`, `strndup`, `strerror`, `strcasecmp`, `strncasecmp`, `stricmp`, `strnicmp`, `bcopy`, `bzero` |
| `<ctype.h>` | all the classification and case functions, ASCII only (`static inline`: no call, no table) |
| `<stdlib.h>` | `malloc`, `calloc`, `realloc`, `free` (the RMA: `OS_Module` 6 and 7; 8 byte aligned; a size that cannot be given is `ENOMEM`), `atoi`, `atol`, `atoll`, `strtol`, `strtoul`, `strtoll`, `strtoull`, `abs`, `labs`, `llabs`, `div`, `ldiv`, `lldiv`, `rand`, `srand` (`RAND_MAX` is `0x7fffffff`), `qsort`, `bsearch`, `getenv` (`OS_ReadVarVal`), `atexit`, `exit`, `_Exit`, `abort` |
| `<stdio.h>` | `printf`, `vprintf`, `sprintf`, `vsprintf`, `snprintf`, `vsnprintf`, `puts`, `putchar` (to stdout: the screen with `OS_WriteC`; `\n` is `OS_NewLine`), `sscanf`, `vsscanf`, **and the files and streams**: `FILE`, `stdin`, `stdout`, `stderr`, `fopen`, `freopen`, `fclose`, `fflush`, `setvbuf`, `setbuf`, `fread`, `fwrite`, `fgetc`, `getc`, `getchar`, `fgets`, `ungetc`, `fputc`, `putc`, `fputs`, `fseek`, `ftell`, `rewind`, `fgetpos`, `fsetpos`, `feof`, `ferror`, `clearerr`, `remove`, `rename`, `perror`, `fprintf`, `vfprintf`, `fscanf`, `vfscanf`, `scanf`, `vscanf` ([the rules](#stdio-files-and-streams)). `printf` converts `d i u x X o p c s n %` with the flags `- + space # 0`, a width and a precision (digits or `*`) and the lengths `hh h l ll j z t` (`%p` is `0x...` and `(nil)`, as in glibc); **no floating point** (`%f` is copied as text). `scanf` converts `d i u o x c s [ n %` with `*`, a width and the lengths; no floating point, no wide characters |
| `<time.h>` | `time`, `clock` (centiseconds), `difftime`, `mktime`, `gmtime`, `localtime` (UTC: there is no time zone), `asctime`, `ctime`, `strftime`. **`time_t` is a 32 bit `long`** (as in the Shared C Library): it ends in 2038 |
| `<setjmp.h>`, `<locale.h>`, `<limits.h>`, `<stdint.h>`, `<assert.h>`, `<errno.h>` | `setjmp`, `longjmp`; `setlocale`, `localeconv` (the "C" locale only); the usual constants and types; `assert` (a failed one prints the expression, file and line and calls `abort`, which in a module is a RISC OS error); `errno` and the codes (the BSD / UnixLib numbers: `EDOM` 33, `ERANGE` 34, `EOVERFLOW` 91; the Shared C Library of the RISC OS Open sources has `EDOM` 1, `ERANGE` 2, `EOVERFLOW` 5, so a program that compares numbers and not names would differ) |
| `<swis.h>` | `_swi`, `_swix` with the **Acorn flag layout** (`_IN`, `_INR`, `_OUT`, `_OUTR`, `_BLOCK`, `_RETURN`, `_FLAGS`); no SWI numbers (use OSLib's headers or your own) |
| `<kernel.h>` | `_kernel_oserror`, `_kernel_swi_regs`, and the Shared C Library's `_kernel_swi`, `_kernel_swi_c`, `_kernel_oscli`, `_kernel_osbyte`, `_kernel_osword`, `_kernel_osrdch`, `_kernel_oswrch`, `_kernel_osbget`, `_kernel_osbput`, `_kernel_osgbpb`, `_kernel_osfind`, `_kernel_osfile`, `_kernel_osargs`, `_kernel_getenv`, `_kernel_setenv`, `_kernel_last_oserror`: same return conventions (-1 for a carry, -2 for an OS error) |

### stdio: files and streams

Built on `OS_Find`, `OS_GBPB`, `OS_Args`, `OS_File` and `OS_FSControl` 25 (`rename`), and on `OS_ReadLine` for the keyboard. A `FILE` and its 512 byte buffer are one `malloc` block. How it differs from a C library on Unix, and from the Shared C Library:

* **Names** go to FileSwitch as they are, and it translates them. Seen on the Pi: `<Var>` is expanded (a variable that is not set is empty: `EINVAL`), a name ends at a space, a wild card opens the first match, and the empty name is the current directory (`EISDIR`); from the sources of FileSwitch: `"` and `/` are refused (`EINVAL`) and `Prefix:` uses `Prefix$Path`. `b` and `t` in a mode mean the same; `x` with `w` fails with `EEXIST` when the file is there. A file that `fopen` makes has the type Data (&FFD), where the Shared C Library makes Text (&FFF) unless the mode has `b`; there is no way to set a type.
* **Size**: a file of 2 GB or more is not opened (`EOVERFLOW`) and a write that would pass 2 GB - 1 fails (`EFBIG`): the C types are 32 bit, although FileSwitch has up to 4 GB.
* **Files that are open**: FileCore opens a file that is open for writing for nobody, and neither deletes nor renames it (`EBUSY`): one stream for a file that is written. A file that is read only or locked is not opened for writing (`EACCES`), and a locked one is neither deleted nor renamed (`EACCES`). `rename` does not replace a file that is there (`EEXIST`) and does not cross file systems (`EXDEV`): `remove` the old name first.
* **Errors**: `errno` is set from the OS error (`ENOENT`, `EISDIR`, `EACCES`, `EBADF`, `EBUSY`, `EEXIST`, `ENOTEMPTY`, `EMFILE`, `EROFS`, `ENOSPC`, `EINVAL`, `EFBIG`, `EOVERFLOW`, `EXDEV`; anything else is `EIO`) and the OS error itself stays in `_kernel_last_oserror ()`. FileSwitch writes late: a disc error shows at `fflush` or `fclose` (`EOF`, `ferror`).
* **stdin** reads a line at a time with `OS_ReadLine`: at most 258 characters, and the OS call drops control characters (TAB too); Escape is the end of the input, and the end-of-file flag stays until `clearerr`. **stdout and stderr** are the screen, unbuffered: one `OS_WriteC` per character and `OS_NewLine` (LF CR) for a line feed. `freopen (name, mode, stdout)` sends `printf`, `puts` and `putchar` to the file. `setvbuf` does nothing for the three. Not there: `freopen (NULL, ...)`, `tmpfile`, `tmpnam`, `gets`, `fseeko`, `ftello`.
* **The end**: `exit ()` and a return from `main` of a runnable module flush and close every file (the OS does not close files at `OS_Exit`); `_Exit` and `abort` close them without writing the buffers. **A module that is killed does not**: call `__modlib_closeall ()` (declared in `<stdio.h>`) in its finalisation. A `FILE` that a runnable program does not close is an RMA block that stays after the program, like every `malloc` block that it does not free.
* **Details**: `ungetc` takes one character, a seek throws it away, and `ftell` is one less after it; `fscanf` uses up `0x` even when no hex digit follows (glibc does); `printf` has `%n`, and a width or precision above `INT_MAX` is -1 with `EOVERFLOW`; `scanf` has no `%lc`, `%ls` or `%l[` (the scan stops) and no floating point; in `%[`, `a-c` is a range when `a <= c` and `c` is not `]`, ranges do not chain, and a first `]` is itself.

**How it was tested.** Against glibc: random sequences of operations on files (about 21,000 results in each run of the host test: every mode, `fwrite`, `fputc`, `fread`, `fgets`, `ungetc`, `fseek`, `ftell`, `fgetpos`, `fflush`, `fprintf`, `fscanf`, the buffer modes) must give the same hashes on the host build (with the sanitizers), and as ARM code on the interpreter. The file system there is a model of FileSwitch and FileCore on the host's files (`modkit/tests/libtest/hosthooks.c` and `fsmodel.py`: a file that is open for writing is open for nobody, read only and locked files, unsigned lengths, `OS_Args 1` beyond the end, disc errors that can be switched on); the checks of the section `stdio2` give the answers of the rules above, and `sim-mk32.py` leaves a file open at the end of a program, run after run. **A model is not FileSwitch**, so the pack `modkit/pack/module34` runs the same sequences and the same rules on the machine. **Run on the Pi on 2026-10-07 (NVMe, ADFS/FileCore): the section `stdio` (21,068 results) gives the same hash as glibc**, and so do the eight other compared sections; the checks of `stdio2` about read only and locked files, a file that is open twice, a folder, the end of a program that leaves a file open (run after run), the keyboard and the screen streams passed. Two checks failed, and both were wrong in the test, not in the library: a rename onto a file that is there is "Bad rename" (`EEXIST`) whatever the state of the source, because FileSwitch looks at the destination before FileCore looks at the source (the model had the order the other way round). A second run with those checks corrected (`modkit/pack/module35`) gave `TOTAL fail=0`, and its section `probe` printed what FileCore and FileSwitch answer in about 90 cases (rename, remove and open of read only, locked and open files, two streams on one file, the pointer beyond the end, attributes, names): those lines are `modkit/tests/libtest/pi-probe.txt`, and `check-probe.py` (run by `run-host.sh` and `run-arm.py`) holds the model against them. They differ only in the names (the model has the host's), and in the length that the catalogue shows for a file that was just opened for output. On the NVMe the error numbers are FileCore's with the file system number in front (&1C8C3 is "Locked", &C3): `errno` is mapped from the low byte.

`libmodkit.a` that the driver links is a linker script that names `libmodkit-core.a` and then `libgcc.a`, so the helpers of the compiler (64 bit division, soft floating point) are found without `-lgcc`; the kit's own integer division comes first.

**A module that is also a program** (`module-is-runnable:` in the CMHG file): `cmunge` makes a start entry, so that `*RMRun Module a "b c" d` (or `*Run` of the file) calls `int main (int argc, char **argv)` in user mode with the words of the command line (up to 40; `"` groups); `exit ()` and a return from `main` end the program with `OS_Exit` and the return code; `atexit` functions run. In SVC mode (a command, a SWI or a service call of the same module) `exit ()` is an error ("exit (3) was called in a module"), never the end of the desktop. A module that says `module-is-runnable:` and has no `main` ends with 0 when it is run.

**International help** (`international-help-file:` and the command option `international:`): the `help-text:` and `invalid-syntax:` of such a command are message tokens, looked up by the kernel in the Messages file that the header names (any path, also through a path variable: `"MyMod:Messages"`). **The value of a help or syntax token must end with a NUL byte (`CHR$ 0`) before the line end**, as the OS's own `CmdHelp` files have it (`HDEV:*GPIODevices lists ...<NUL><LF>`): `*Help` prints the value in place with `OS_PrettyPrint`, which stops only at a NUL, and the text of a syntax error is copied up to the NUL. A value with only the line end prints the rest of the file and then whatever follows it in memory (seen on the Pi). The header that `cmunge` makes is the same as CMunge's; the NUL is in the Messages file.

## How it is tested

* **The library**: `modkit/tests/libtest` generates its inputs from a fixed generator and runs the same test program three ways: against glibc (the oracle), against the library's own sources built for the host with AddressSanitizer and UBSanitizer, and as ARM code on the A32 interpreter (`tools/a32.py`) with a model of the few SWIs. The results of every section are hashed and **the three hashes must be equal** (about 114,000 results in the sections that are compared: `ctype`, `string`, `numbers`, `sort`, `div`, `sscanf`, `printf`, `stdio`, `time`). The sections that have answers of their own (`limits`, `rand`, `swix`, `clock`, `heap`, `setjmp`, `getenv`, and for `stdio` the errors, access, the end of a program, the screen and keyboard streams: `stdio2`, `stdio3`) check them. `run-host.sh` and `run-arm.py` run it; `run-arm.py` takes a few minutes.
* **`cmunge`**: `modkit/tests/test-ctools.py` compares the C `cmunge` with the Python one on 400 generated CMHG files, on the real ones and on files that must be refused (2956 checks), and on the files it makes with the real CMunge, where the real one accepts them. `modkit/tests/os-cmhg-corpus.py` runs every CMHG file of the RISC OS Open sources through `cmunge` and the real CMunge and compares what the module headers say (`cmhgdiff.py`).
* **The machine code**: `sim-veneers.py` (generic veneers in SVC, IRQ and USER mode with every flag state, SWI decoding table with a prefix that is not the title, service numbers that are no ARM immediate), `sim-runnable.py` (a module run as a program: `argc`, `argv`, `atexit`, `exit`, `abort`, the module as a module afterwards), `sim-mk32.py` (international help, `add-syntax`, the module's own SWIs, the self test in SVC and in USER mode), `sim-tickmod.py`: the machine code on the ARM interpreter against a model of the kernel.

## What it is

A module is a flat image with a header, loaded into the RMA, called in SVC mode, with no C library. `modkit` supplies what is missing around the compiler:

| Part | What it does |
|---|---|
| `src/cmunge.c` | CMunge's command line (`-tgcc -32bit -p -D -I -d -o -s`) over the module header generator: reads a **CMHG** file (title, help and date strings, initialisation and finalisation code, `*` commands with international help, SWI chunks, vector, IRQ/callback and generic handlers, `module-is-runnable`) and writes the assembler header with the entry veneers, plus a C header (`bin/cmunge` and `bin/mkmodhdr.py` are the Python version) |
| `lib/`, `include/` | the C library of the kit (one source per group of functions, [above](#the-c-library-of-modkit)), integer division (`divmod.c`), the SWI veneer (`modswi.S`), the linker script `module.ld` |
| `src/mkoslib.c` | writes OSLib-style veneers for the OSLib functions your objects use (or the SWIs you name), from OSLib's headers |
| `src/modreloc.c` | the relocation step: the module is linked as position-dependent code and **relocates itself** when it starts, from a table of address words, so it can be loaded anywhere |
| `lib/riscos_consts.py` | reads SWI and service call numbers from the RISC OS sources instead of typing them from memory |
| `module.mk` | the make rules: `include` it from the Makefile of a module; it builds the kit's library from the kit's own sources into the module's build folder, so a change of the kit counts at once |
| `tests/` | host simulations: a model of the RISC OS kernel (`riscosmodel.py`, on top of `modpoc/kernelmodel.py`) and an ARM interpreter (`tools/a32.py`) run the module's machine code offline (`sim-tickmod.py`, `sim-veneers.py`, `sim-runnable.py`, `sim-mk32.py`); `libtest/`: the library against glibc; `test-ctools.py`, `os-cmhg-corpus.py`: the tools; `hwpack/`: the module of a hardware test |
| `modpoc/` | the two earlier proofs of concept, `HelloMod` and `HelloMod2` (hand-written header and veneers), with their kernel model and a mutation test |

`-mmodule` compiles with `-march=armv6 -mfloat-abi=soft -marm -ffreestanding -fno-stack-clash-protection` (so addresses live in literal pools and no `movw`/`movt` is used), `-fvisibility=hidden` and no unwind tables or exceptions; whatever `-march`, `-mcpu`, `-mfpu` or `-mfloat-abi` you give is replaced (a module image has no relocation for `movw`/`movt`), except `-march=armv6k` and the like.

## Building the example

`modkit/examples/tickmod` is a module that claims the **`TickerV`** vector (called every centisecond in interrupt context), asks for **transient callbacks**, and records the processor mode, IRQ mask and registers in both handlers.

```bash
export OSLIB=<a directory that contains oslib/*.h>               # OSLib's headers (from GCCSDK or the OSLib distribution), for the SWI veneers
export RISCOS_SOURCES=<a checkout of the RISC OS Open sources>   # optional: https://gitlab.riscosopen.org/RiscOS/Sources: makes the one SWI number of the header be READ from the sources
cd modkit/examples/tickmod
make BIN=<cross toolchain>/bin                                   # -> TickMod,ffa (the module) and TickWait,ff8 (a tiny helper that waits in user mode)
```

A module Makefile sets `MODULE`, `CMHG`, `SRCS` and `OSLIB_FUNCS` and includes `module.mk` (see `examples/tickmod/Makefile`): it runs the commands of the table above. It needs GNU make 4.3 or later. The tool chain installs a copy of `module.mk`, the scripts and the headers in `share/riscos-modkit/`.

## Porting the Makefile of a GCCSDK 4.7.4 module

A 4.7.4 module Makefile (compile with `-mmodule`, `cmunge`, link with `-lOSLibH32`) needs four changes. This is the Makefile of a real module (a network command server, 800 lines of C that use sockets, files and OS calls through OSLib) before and after; the module built from its **unmodified source** and passed 101 of the 102 checks of the host simulation (the 102nd is a name-length detail of the renamed test copy):

```make
# before (GCCSDK 4.7.4)
CC    = arm-unknown-riscos-gcc -c -Wall -mpoke-function-name -O2 -mlibscl -mthrowback -mmodule -mhard-float -march=armv7-a -std=gnu99 -I$(ENV)/include -Ih -I.
LINK  = arm-unknown-riscos-gcc -mpoke-function-name -O2 -mlibscl -mthrowback -mmodule -mhard-float -march=armv7-a -L$(ENV)/lib
OBJS  = mod.o header.o
mod,ffa: $(OBJS)
	$(LINK) -o $@ $(OBJS) -lOSLibH32

# after (this tool chain)
TC    = <tool chain>/bin/arm-riscos-gnueabihf
CC    = $(TC)-gcc -c -Wall -mpoke-function-name -O2 -mthrowback -mmodule -std=gnu99 -I$(ENV)/include -Ih -I.
LINK  = $(TC)-gcc -mthrowback -mmodule
OBJS  = mod.o header.o oslibv.o
mod,ffa: $(OBJS)
	$(LINK) -o $@ $(OBJS)
oslibv.c: mod.o
	$(TC)-mkoslib -I $(ENV)/include/oslib -o $@ --from-objects mod.o
oslibv.o: oslibv.c
	$(CC) -x c -o $@ oslibv.c
```

1. The compiler is `arm-riscos-gnueabihf-gcc`. `-mlibscl`, `-mhard-float` and `-march=armv7-a` can stay (they are accepted; `-mmodule` replaces the architecture and the float ABI), but there is no reason to keep them.
2. `cmunge` is the same command (`cmunge -32bit -tgcc -o header.o -d header.h file.cmhg`); the `cmunge` of the tool chain is the one in its `bin/`.
3. `-lOSLibH32` becomes the veneers that `mkoslib` writes from the objects.
4. The source must not need the C library beyond the list under Limits. A call that is not there is an undefined symbol at the link: that is the whole list of what to port.

OSLib's headers are needed (for the register layout in their comments); the headers of GCCSDK's `env/include/oslib` are the ones used here.

## Trying it on RISC OS

Copy `TickMod,ffa` (type &FFA, module) and `TickWait,ff8` to RISC OS. **Save your work first**: the module is called from interrupts, and a mistake in a vector veneer shows up as a crash within a second of `*TickMod_Start`.

```
RMLoad TickMod
Help TickMod
TickMod_Start                 claims TickerV
TickWait 200                  wait two seconds in user mode: ticks and callbacks are counted
TickMod_Status
TickMod_Delay 200             two seconds inside a command (SVC mode): one callback waits until it returns
TickMod_Stop
RMKill TickMod
```

On the author's machine the module ran with: ticks at exactly 100 Hz, the tick handler always in SVC mode with IRQs off, the callback in SVC mode with IRQs on, `r12` right, a callback that waits for the end of a command, `OS_RemoveCallBack`,
`RMKill` with the vector claimed, and thirty quick load / start / kill rounds, without a crash.

## What was proven on hardware

| Module | Shows |
|---|---|
| `modpoc` HelloMod | header, initialisation, `*Help`, `*` commands, `RMReInit`, `RMKill`, reload; a self-relocating flat image |
| `modpoc` HelloMod2 | a SWI decoding table and handler veneer, a service call handler, static data (`.data`, `.bss`), tables of pointers to strings and functions, a `switch` (jump table), `memcpy`/`memset`/`memmove`/`memcmp`, 17 checks run inside the module through the kernel |
| TickMod | the vector and callback veneers (above) |
| a port of an existing network module | sockets, a spool file, a re-entrancy guard (not published: it is derived from someone else's module) |
| the same module's current source, built by the GCCSDK 4.7.4 commands (`cmunge`, `gcc -mmodule`, `mkoslib`) on Linux **and on the Pi** (16.2.0-14) | the build flow itself: the file the Pi makes in 3 seconds is byte for byte the file Linux makes, and it passed 39 network checks on the machine (banner, commands, files of 0 to 500,000 bytes, a client that vanishes, a second client, the spool diagnostics, a soak of 300 commands) |
| `modkit/tests/hwpack`, run on the Pi on 2026-10-07 (the repository, ahead of 16.2.0-14) | **the C library of the kit on a real CPU**: the 8 sections of `libtest` that are compared with glibc (92,724 results) give the same hashes, and the heap (5,279 checks: random `malloc` / `realloc` / `calloc` / `free` with patterns), `setjmp`, `getenv` and the real SWIs (`OS_SWINumberFromString`, `OS_ConvertCardinal4`, the monotonic time and the clock) pass; **`module-is-runnable`**: `*RMRun Module a "b c" d` with the right `argc` / `argv`, exit codes 10, 40, 9, 12 and 134 (`exit`, `return`, `abort`), the `atexit` function run or not as it should be; `*Help` and the syntax errors of commands with `add-syntax:`. A second run the same day: **international help** (`international-help-file:` and `international:`: the two lines from the Messages file, and the translated syntax error, once the Messages file has its NULs), the module's **own SWIs and decoding table** with a prefix that is not the title (`OS_SWINumberFromString` / `ToString` and the kernel's dispatch), the **generic veneer** called from SVC code, from USER code (the module run as a program) and by `OS_CallEvery` in interrupt time (SVC mode, IRQs off, 8-byte aligned stack, `r12` right, registers and flags returned as they were with 8 flag states, V and `r0` set for an error), 15 checks of `MK32_SelfTest` in SVC mode and the same 15 in USER mode, `exit` in a command (an error, the desktop stays), `snprintf` of `long long`, `getenv`, `malloc` |
| `tests/selftest`, check 12 (16.2.0-14) | a small module built on RISC OS, checked (header and relocation table), loaded, its command run (`*ModHello_Sum 2 3` prints 5) and removed |

These ran on the machine before `-mmodule` and `cmunge` existed, built by `module.mk`'s older rules. Built again through `gcc -mmodule` and `cmunge`, TickMod and the port come out **byte for byte the same** as the images that ran, and the 34 deliberate breakages of the port's host test (`tests/mutate-cmdserv2.py`) are all still caught.

## Limits

* No `UnixLib` or `SharedCLibrary`, no C++ and no exceptions yet, files only with [the rules above](#stdio-files-and-streams) (nothing is closed when a module is killed), and floating point untested on hardware (see [what can be built today](#what-can-be-built-today-and-what-cannot)): modules run on a small stack, in SVC mode, so keep stack use low (`qsort` is an in-place heap sort, with no recursion and no memory) and do not call blocking functions.
* There is no automatic stack checking. `malloc`, `free`, `calloc` and `realloc` take their memory from the RMA (`OS_Module` 6 and 7).
* The header generator implements the subset of CMHG listed above; it reports anything else as an error rather than dropping it silently. **Where it differs from CMunge** (checked against the real CMunge 0.76 on the RISC OS Open sources and on generated files): a command without `max-args:` takes no arguments (CMunge's default; `cmunge` of 16.2.0-14 accepted 255); `min-args:`, `max-args:` and `gstrans-map:` above 255 are refused (they were cut to 255); a SWI chunk must not be 0, a multiple of 64 or have bit 17 set; a `swi-decoding-table:` without a prefix of its own takes the module title; `\n` in a string is a carriage return (13) as in CMHG, and the C escapes `\t \x41 \a` are accepted (CMunge keeps the letter); quoted names are accepted (CMunge keeps the quotes); `help:` is refused (CMunge refuses it too); the `help-string:` is read the way CMunge reads it (name, version, date) and refused when CMunge refuses it; `finalisation-code:` is optional; the handler of `generic-veneers:` returns a `_kernel_oserror *` (0: return to the caller with the registers the handler left in the block and the flags as they were, else V set and r0 = the error), where 16.2.0-14 treated it like a vector handler; `module-is-runnable:` was ignored with a warning in 16.2.0-14 and is a real start entry now.
* The relocation is the modkit's own (a self-relocating image made by `modreloc`), not `ld`'s module support (`--ro-module-reloc` of GCCSDK's linker is not in binutils 2.45.1): that is why the driver runs a program (`modreloc`) after the linker, and why the code is ARMv6.
* `cmunge` has the options of CMunge that a freestanding module can use (`-tgcc -32bit -p -px -D -U -I -d -o -s`) and refuses the others (`-zbase`, `-zoslib`, `-zerrors`, `-x<type>`, `-tnorcroft`, ...). **The tools are C programs** (`modkit/src`: `cmunge`, `modreloc`, `mkoslib`), the same source for Linux (in the cross tool chain) and for RISC OS (in the native package), so modules can be built **on RISC OS** too: the native `Gcc16` 16.2.0-14 has them, `libmodkit.a`, the linker script and the headers, and its `gcc -mmodule` works as the cross one. The `*.py` files in `modkit/bin` are the first versions of the three tools; `modkit/tests/test-ctools.py` checks the C ones against them (2956 comparisons: the same output byte for byte on every CMHG file and ELF file of the test, and on every one of the 2334 OSLib functions).
* One person has tested it on one machine. For modules that matter, use GCCSDK's tool chain.
