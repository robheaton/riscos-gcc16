# OSLib

OSLib is the C library of the RISC OS operating system: a header for every SWI, with its register layout, and a veneer for each one (`xos_cli`, `os_cli`, `wimp_poll` ...). With this tool chain you build **the real OSLib, from its own sources**, into `libOSLib32.a` (and `libOSLibSupport32.a`) and use it from programs (UnixLib) and from modules (`gcc -mmodule`).

## The licence, and why this project does not ship it

OSLib is under the GNU General Public License (version 1 or later) with an exception that allows linking it into programs. It is a separate project (ROOL, [sourceforge.net/p/ro-oslib](https://sourceforge.net/p/ro-oslib/code/)). So **neither OSLib, nor its headers, nor the built library are in the releases of this project.** What is here is the recipe: `recipe/gcc-16.2.0-riscos/scripts/install-oslib.py` and five patches (`recipe/gcc-16.2.0-riscos/patches-oslib`, [README](../recipe/gcc-16.2.0-riscos/patches-oslib/README.md)); four of them are GCCSDK's own, which make OSLib's build produce the EABI hard-float library, and one is ours (below).

## Building it

```bash
svn checkout https://svn.code.sf.net/p/ro-oslib/code/trunk/'!OSLib' ~/oslib      # revision 477 was used
recipe/gcc-16.2.0-riscos/scripts/install-oslib.py --oslib-src ~/oslib --toolchain <the tool chain> [--jobs N] [--keep DIR]
```

It needs `patch`, `make`, `bison`, `flex` and a host C compiler. It copies your checkout, applies the patches, builds OSLib's own tools (`DefMod`, which makes the headers and the assembler veneers from the definition files), and makes `oslib` and `oslibsupport` with the cross compiler. It then links every member of the library into a program and into a module (through `modreloc`) and stops if that fails, and installs:

| Where (in `<tool chain>/arm-riscos-gnueabihf`) | What |
|---|---|
| `include/oslib/` | the 333 headers (`*.h` for C, `*.Hdr` for assembler) |
| `include/` | the headers of OSLibSupport (`Choices.h`, `Err.h`, `Event.h`, `Message.h`, `Task.h`, `trace.h` ...) |
| `lib/libOSLib32.a`, `lib/libOSLibH32.a` | the library (4,674 members, the same 4,668 symbols as GCCSDK's `libOSLib32.a`; `H32` is the same file under the other name some Makefiles use) |
| `lib/libOSLibSupport32.a` | OSLibSupport |

The library has no C library of its own (the veneers only call SWIs), so the same file links into programs that use UnixLib and into modules. Six symbols are defined by two members (as in GCCSDK's library, for example `xos_read_var_val_size`); a normal link takes one, and the whole-library link test allows both.

## Using it

```c
#include "oslib/os.h"
#include "oslib/wimp.h"
```
```bash
arm-riscos-gnueabihf-gcc -O2 -o prog,ff8 main.c -lOSLib32                 # a program
arm-riscos-gnueabihf-gcc -mmodule -O2 -o mod.elf mod.o header.o -lOSLib32  # a module
```

A 4.7.4 Makefile (`-LOSLib: -lOSLib32`, `-lOSLibH32`) links unchanged. The X functions return the error block; the non-X ones return the result register OSLib's header names and raise an error with `OS_GenerateError` (which ends the command line: use X functions where an error is expected).

## The finding: non-X veneers in SVC mode

OSLib assumes that a non-X function is only called in user mode, and its veneers keep the return address in LR. A module runs in SVC mode, where a SWI overwrites R14_svc. The first function of the library that was tried in the interpreter (`os_cli`) never came back. DefMod now has an option `-svc-safe` (`patches-oslib/zz-defmod-svc-safe.patch`, enabled for ARMEABIHF in `Makefile.setup`) that keeps LR in IP as the X veneers do; this costs a `mov` and makes the same library safe in both modes. GCCSDK's own library has the unsafe veneers; if you take it into a module, use the X functions.

## What was tested

* `modkit/tests/sim-oslib.py` (on the ARM interpreter and the kernel model): the veneers of X and non-X functions with 0 to 8 arguments, outputs, errors, and the whole-library link: 20 checks.
* On the Raspberry Pi (RISC OS 5.30, 2026-10-11, `pack/module54`): **OlHw**, a module linked with the real `libOSLib32.a` in SVC mode: 21 checks, 0 failed (`os_read_monotonic_time`, `os_convert_cardinal4`, the X forms, a SWI with seven and with eight arguments, `xos_gs_trans`, `xos_cli`, errors, mode variables, files); **SockHw**, the 53-check socket test of `pack/module45`, built again with it: 53 checks, 0 failed; a failing non-X call ended the command line as described.
* GCCSDK's own examples and modules build with the real library (`test-gccsdk-modules.py`, `test-gccsdk-examples.py`).

## The older route

`modkit/bin/mkoslib-lib.sh` and `mkoslib` (with `OSLIB=<headers>`) make veneers from OSLib's headers without building OSLib. They are kept for a tool chain without the OSLib source: `mkoslib` writes only the functions your objects use, and its library lacks 494 functions (the Toolbox calls that are messages, and those that do not compile with OSLib's types). When you have a checkout, `install-oslib.py` is better: the library is OSLib's own.

## Not done

A native build of OSLib on RISC OS; a `libOSLib32.a` for the Shared C Library.
