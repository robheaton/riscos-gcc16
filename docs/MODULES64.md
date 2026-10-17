# Writing modules for RISC OS 64 (AArch64)

**Status: experimental (16 October 2026).** The kit builds 64-bit modules in C and C++ with one command, and they load and pass their tests on RISC OS Pyromaniac (see [RISCOS64.md](RISCOS64.md)). The kernel's conventions for interrupts, vectors and events are not supported, and the details of service calls and `x12` are those of Pyromaniac.

## Install

You need the AArch64 compiler (`riscos64/tools/build-a64-toolchain.sh`: GCC 16.2 and binutils 2.45.1 for `aarch64-none-elf`, C and C++, about 30 minutes) and, for programs, picolibc (`riscos64/tools/build-picolibc.sh`). Then `riscos64/install-kit64.sh PREFIX [--toolchain BINDIR] [--picolibc DIR]` installs the commands `aarch64-riscos-gcc`, `cmunge64`, `modreloc64`, `mkaif64`, `mkoslib` in `PREFIX/bin` and builds the libraries.

## A module in C

```c
#include "kernel.h"
#include "modkit64.h"
#include "hello64h.h"                 /* written by cmunge64 from the CMHG file */

_kernel_oserror *hello_swi (int swi_offset, _kernel_swi_regs *r, void *pw)
{
  if (swi_offset == 0) { r->r[0] = r->r[0] + r->r[1]; return 0; }
  return error_BAD_SWI;
}
```

With a CMHG file (`title-string:`, `swi-chunk-base-number:`, `command-keyword-table:` ... the same language as the 32-bit `cmunge`):

```
cmunge64 -d hello64h.h -o hello64_hdr.o hello64.cmhg
aarch64-riscos-gcc -mmodule -O2 hello64.c hello64_hdr.o -o Hello64        # makes Hello64,ffa
```

or with a Makefile that includes `riscos64/kit/module64.mk` (`MODULE`, `CMHG`, `SRCS`, `FP`). `RMLoad Hello64` loads it. The handlers have the types and names of a 32-bit CMHG module; `kit/include/modkit64.h` has the kit's own interface (`mk_error`, `mk_swi`, `mk_printf`) and the entry points for a module without CMHG (`module_init`, `module_swi` ...), and `kit/examples/` has eight complete examples with their tests.

## What a module can use

* **The C library of the kit** (`libmodkit64.a`, the 129 sources of the 32-bit kit's library compiled for AArch64): `<stdio.h>` (`printf` goes to the screen, `snprintf`, `sscanf`, files), `<stdlib.h>` (`malloc` from the RMA, 16-byte aligned, `qsort`, `strtol`, `strtod`), `<string.h>`, `<ctype.h>`, `<math.h>`, `<time.h>`, `<setjmp.h>` ...; the library's own test (39 sections) gives the same results as glibc in 35 and as the host build in the rest on Pyromaniac, as a program and inside a module.
* **`_swi` and `_swix`** of `<swis.h>` (a `_BLOCK` takes up to 16 words), and OSLib's X and non-X functions through the veneers of `mkoslib` (simple SWIs; structures that hold pointers do not have the layout the OS expects on 64 bits).
* **Floating point:** compile with `-mmodule-fp` (hardware floating point and vector registers; Pyromaniac keeps the caller's registers across a module's SWI). The default, `-mmodule`, uses no floating point or vector register (`-mgeneral-regs-only`), so `printf("%f")`, `math.h` and `double` need `-mmodule-fp`.
* **C++** (`.cc`, `.cpp`, `.cxx`; `module-is-c-plus-plus:` in the CMHG file): static constructors and destructors, `new` and `delete`, virtual functions, templates. No exceptions, no RTTI (`-fno-exceptions -fno-rtti`), no libstdc++.
* **Errors:** a handler may return an error block from anywhere (a static block in the module's code too): the kit copies it to the RMA, because a program cannot read an error block in the module's code.

## What a module must know

* A module is linked at address 4 and loaded at a page plus 4; it relocates its own pointers when it starts, and the kit clears its `.bss` at every start (Pyromaniac keeps `.data` and does not zero the area again after `*RMReInit`; another kernel may do either).
* The registers of a SWI or of a service call reach your handler as 32-bit values (`mk_reg`, the same as `_kernel_swi_regs`), whatever the size of the slots of the kernel's register block (32 bits on Pyromaniac, 64 on a kernel with 64-bit slots: the kit converts). Addresses handed to SWIs through the library are cut to 32 bits: they must be below 4 GB, as they are today.
* **Service calls:** the module's service entry is a plain one that the kernel calls for every service; the kit works out which convention the kernel uses and filters the numbers of your list. Set `r[1] = 0` in the handler to claim a service, as in CMHG.
* A module's stack is the 32 KB SVC stack that the whole OS shares: a module that needs more claims its own (see `libtest/libtestmod.c`).
* Not supported yet: interrupt handlers, vectors, events and callbacks (`cmunge64` refuses them with a message), runnable modules, sockets, C++ exceptions.

## Testing with Pyromaniac

`riscos64/tools/run_on_service.py` runs a module and its test program on RISC OS Pyromaniac through the hosted [RISC OS Build service](https://build.riscos.online/) in about 20 seconds (jobs for every example are in the script); see [RISCOS64.md](RISCOS64.md) for the differences between Pyromaniac and RISC OS that have been found.
