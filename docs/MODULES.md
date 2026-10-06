# modkit: building RISC OS modules with this tool chain (experimental)

> **Experimental.** Relocatable modules are the one thing the EABI tool chain could not build: GCCSDK's 4.7.4 compiler (APCS, SharedCLibrary) is still the only supported way for modules that matter. `modkit` shows that GCC 16 can build working modules **without any C library**,
> and since 16.2.0-14 it is used the way GCCSDK 4.7.4 was: **`gcc -mmodule`** and **`cmunge`**. It has been run on real hardware for small modules (below). Read the limits first.

## The commands (the same as with GCCSDK 4.7.4)

| GCCSDK 4.7.4 | This tool chain (16.2.0-14 and later) |
|---|---|
| `cmunge -tgcc -32bit -p -d header.h -o header.o file.cmhg` | the same command: `cmunge` is in the tool chain's `bin/` (it is modkit's module header generator behind CMunge's command line; the options that make no sense here are refused, not ignored) |
| `gcc -mmodule -O2 -c file.c` | the same: `-mmodule` means ARMv6, soft float, ARM state, freestanding, no PIC, no stack protector, no unwind tables, and the headers of modkit instead of UnixLib's; `__TARGET_MODULE__` and `__TARGET_SCL__` are defined, `__TARGET_UNIXLIB__` is not |
| `gcc -mmodule -o Module,ffa file.o header.o` (the linker writes the module) | **the same command**: the driver links with the module linker script and `libmodkit.a` (the tool chain has both) and then runs `modreloc`, so the output is the flat module image. An output named `*.elf` stays an ELF file (for a debugger or a simulation) |
| the Shared C Library through its stubs | **no C library**: `libmodkit.a` has the few functions below |
| `-lOSLib32` / `-lOSLibH32` (OSLib's SWI veneers) | **`arm-riscos-gnueabihf-mkoslib -I <OSLib>/oslib -o oslibv.c --from-objects main.o`** writes the veneers of exactly the OSLib functions that your objects use (from OSLib's own headers: the comment above each function says which SWI and which registers); compile `oslibv.c` and link it. GCCSDK's prebuilt OSLib archives are for the old ABI and cannot be linked |

```bash
export PATH=<tool chain>/bin:$PATH
cmunge -tgcc -32bit -p -d header.h -o header.o module.cmhg
arm-riscos-gnueabihf-gcc -mmodule -O2 -Wall -c main.c -o main.o
arm-riscos-gnueabihf-gcc -mmodule -o MyModule,ffa main.o header.o     # the module: the driver runs modreloc after the linker
```

A makefile that has `.cmhg.o` and `.cmhg.h` rules with `cmunge -tgcc -32bit -p` and `LDFLAGS = -mmodule` works as it is, apart from the library (below). `module.mk` (below) has it all as make rules.
How the module is made: the linker writes an ELF file, and `arm-riscos-gnueabihf-modreloc` (a small C program; the driver runs it, `gcc -mmodule` only) replaces it by the flat image with the table of addresses that the module relocates itself with (the reason is under Limits). A partial link (`-r`) is not turned into a module.

## What it is

A module is a flat image with a header, loaded into the RMA, called in SVC mode, with no C library. `modkit` supplies what is missing around the compiler:

| Part | What it does |
|---|---|
| `src/cmunge.c` | CMunge's command line (`-tgcc -32bit -p -D -I -d -o -s`) over the module header generator: reads a **CMHG** file (title, help and date strings, initialisation and finalisation code, `*` commands, SWI chunks, vector and IRQ/callback handlers) and writes the assembler header with the entry veneers, plus a C header (`bin/cmunge` and `bin/mkmodhdr.py` are the Python version) |
| `lib/` | `modlib.c`: a minimal C library (`memcpy`, `strlen` ..., `printf` through `OS_WriteC`, `sprintf`, `snprintf`); `divmod.c`: integer division; `modswi.S`: `_kernel_swi`; `module.ld`: the linker script |
| `src/mkoslib.c` | writes OSLib-style veneers for the OSLib functions your objects use (or the SWIs you name), from OSLib's headers |
| `src/modreloc.c` | the relocation step: the module is linked as position-dependent code and **relocates itself** when it starts, from a table of address words, so it can be loaded anywhere |
| `lib/riscos_consts.py` | reads SWI and service call numbers from the RISC OS sources instead of typing them from memory |
| `module.mk` | the make rules: `include` it from the Makefile of a module |
| `tests/` | host simulations: a model of the RISC OS kernel (`riscosmodel.py`, on top of `modpoc/kernelmodel.py`) and an ARM interpreter (`tools/a32.py`) run the module's machine code offline (`sim-tickmod.py`) |
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
| `tests/selftest`, check 12 (16.2.0-14) | a small module built on RISC OS, checked (header and relocation table), loaded, its command run (`*ModHello_Sum 2 3` prints 5) and removed |

These ran on the machine before `-mmodule` and `cmunge` existed, built by `module.mk`'s older rules. Built again through `gcc -mmodule` and `cmunge`, TickMod and the port come out **byte for byte the same** as the images that ran, and the 34 deliberate breakages of the port's host test (`tests/mutate-cmdserv2.py`) are all still caught.

## Limits

* No C++, no exceptions, no floating point (soft float with no FP library), no `UnixLib` or `SharedCLibrary`: modules run on a small stack, in SVC mode, so keep stack use low and do not call blocking C library functions (there are none).
* There is no automatic stack checking. `malloc`, `free`, `calloc` and `realloc` in `modlib.c` take their memory from the RMA (`OS_Module` 6 and 7).
* The header generator implements the subset of CMHG that the examples use; it reports anything else as an error rather than dropping it silently.
* The relocation is the modkit's own (a self-relocating image made by `modreloc`), not `ld`'s module support (`--ro-module-reloc` of GCCSDK's linker is not in binutils 2.45.1): that is why the driver runs a program (`modreloc`) after the linker, and why the code is ARMv6.
* `cmunge` has the options of CMunge that a freestanding module can use (`-tgcc -32bit -p -px -D -U -I -d -o -s`) and refuses the others (`-zbase`, `-zoslib`, `-zerrors`, `-x<type>`, `-tnorcroft`, ...). **The tools are C programs** (`modkit/src`: `cmunge`, `modreloc`, `mkoslib`), the same source for Linux (in the cross tool chain) and for RISC OS (in the native package), so modules can be built **on RISC OS** too: the native `Gcc16` 16.2.0-14 has them, `libmodkit.a`, the linker script and the headers, and its `gcc -mmodule` works as the cross one. The `*.py` files in `modkit/bin` are the first versions of the three tools; `modkit/tests/test-ctools.py` checks the C ones against them (2812 comparisons: the same output byte for byte on every CMHG file and ELF file of the test, and on every one of the 2334 OSLib functions).
* One person has tested it on one machine. For modules that matter, use GCCSDK's tool chain.
