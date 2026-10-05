# modkit: building RISC OS modules with this tool chain (experimental)

> **Experimental proof of concept.** Relocatable modules are the one thing the EABI tool chain could not build: GCCSDK's 4.7.4 compiler (APCS, SharedCLibrary) is still the only supported way. `modkit` shows that GCC 16 can build working modules **without any C library**.
> It has been run on real hardware for three small modules (below). It is not a replacement for GCCSDK's module support: read the limits first.

## What it is

A module is a flat image with a header, loaded into the RMA, called in SVC mode, with no C library. `modkit` supplies what is missing around the compiler:

| Part | What it does |
|---|---|
| `bin/mkmodhdr.py` | reads a **CMHG** file (title, help and date strings, initialisation and finalisation code, `*` commands, SWI chunks, vector and IRQ/callback handlers) and writes the assembler header with the entry veneers, plus a C header |
| `lib/` | `modlib.c`: a minimal C library (`memcpy`, `strlen` ..., `printf` through `OS_WriteC`, `sprintf`, `snprintf`); `divmod.c`: integer division; `modswi.S`: `_kernel_swi`; `module.ld`: the linker script |
| `bin/mkoslib.py` | writes OSLib-style veneers for the SWIs you name (`OSLIB_FUNCS`), from OSLib's headers |
| `bin/modreloc.py` | the relocation step: the module is linked as position-dependent code and **relocates itself** when it starts, from a table of address words, so it can be loaded anywhere |
| `lib/riscos_consts.py` | reads SWI and service call numbers from the RISC OS sources instead of typing them from memory |
| `module.mk` | the make rules: `include` it from the Makefile of a module |
| `tests/` | host simulations: a model of the RISC OS kernel (`riscosmodel.py`, on top of `modpoc/kernelmodel.py`) and an ARM interpreter (`tools/a32.py`) run the module's machine code offline (`sim-tickmod.py`) |
| `modpoc/` | the two earlier proofs of concept, `HelloMod` and `HelloMod2` (hand-written header and veneers), with their kernel model and a mutation test |

The code is compiled with `-march=armv6 -mfloat-abi=soft -marm -ffreestanding -fno-stack-clash-protection` (so addresses live in literal pools and no `movw`/`movt` is used), `-fvisibility=hidden` and no unwind tables or exceptions.

## Building the example

`modkit/examples/tickmod` is a module that claims the **`TickerV`** vector (called every centisecond in interrupt context), asks for **transient callbacks**, and records the processor mode, IRQ mask and registers in both handlers.

```bash
export RISCOS_SOURCES=<a checkout of the RISC OS Open sources>   # https://gitlab.riscosopen.org/RiscOS/Sources: the SWI and service numbers
export OSLIB=<a directory that contains oslib/*.h>               # OSLib's headers (from GCCSDK or the OSLib distribution)
cd modkit/examples/tickmod
make BIN=<cross toolchain>/bin                                   # -> TickMod,ffa (the module) and TickWait,ff8 (a tiny helper that waits in user mode)
```

A module Makefile sets `MODULE`, `CMHG`, `SRCS` and `OSLIB_FUNCS` and includes `module.mk` (see `examples/tickmod/Makefile`).

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

## Limits

* No C++, no exceptions, no floating point (soft float with no FP library), no `UnixLib` or `SharedCLibrary`: modules run on a small stack, in SVC mode, so keep stack use low and do not call blocking C library functions (there are none).
* There is no automatic stack checking. `malloc`, `free`, `calloc` and `realloc` in `modlib.c` take their memory from the RMA (`OS_Module` 6 and 7).
* The header generator implements the subset of CMHG that the examples use; it reports anything else as an error rather than dropping it silently.
* The relocation is the modkit's own (a self-relocating image), not `ld`'s module support: the "proper tool chain route" (a CMHG header and linker support for module relocation) was not attempted.
* One person has tested it on one machine. For modules that matter, use GCCSDK's tool chain.
