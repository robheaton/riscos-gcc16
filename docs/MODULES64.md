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
* The registers of a SWI or of a service call reach your handler as 32-bit values (`mk_reg`, the same as `_kernel_swi_regs`), whatever the size of the slots of the kernel's register block (32 bits on Pyromaniac, 64 on a kernel with 64-bit slots: the kit converts). How the kit tells the two apart: a service call shows it (the number is in `x0` and the block's address in `x1` with 64-bit slots); a module without a service handler sends a SWI to its own chunk (offset 63, used only when the module's SWI table lists fewer than 64 names; the layout of the block that arrives answers); and if neither can be used, a register block at a high address (the kernel's stack, above 2 GB) means 64-bit slots. `examples/slot64` prints what the kit found. Addresses handed to SWIs through the library are cut to 32 bits: they must be below 4 GB, as they are today.
* **Service calls:** the module's service entry is a plain one that the kernel calls for every service; the kit works out which convention the kernel uses and filters the numbers of your list. Set `r[1] = 0` in the handler to claim a service, as in CMHG.
* A module's stack is the 32 KB SVC stack that the whole OS shares: a module that needs more claims its own (see `libtest/libtestmod.c`).
* **More examples (18 October 2026):** `examples/kv64` is a typical data module: a key/value store in the RMA with six SWIs (`Kv64_Set` `Get` `Delete` `Count` `Enum` `Clear`), four commands (`*Kv_Set`, `*Kv_Get`, `*Kv_List`, `*Kv_Clear`) and a CMHG file; `kv64test` has 22 checks and passes on Pyromaniac. `examples/vec64` is a **vector claim from a module**: OS_Claim (SWI &1F) of WrchV with a veneer written in assembler that counts the calls and passes them on with `ret`, OS_Release (&20) afterwards; on Pyromaniac the veneer is called with the registers of the caller of OS_WriteC and the call goes on to the next claimant. **Not found out:** how a handler *intercepts* a call (the 32-bit way is to pop the return address from the stack) and the conventions of the IRQ, event and callback veneers, so `cmunge64` still refuses `vector-handlers:`; **not tried:** Wimp tasks and filing systems as modules (Pyromaniac has no desktop here, and FileSwitch's calls into a module have 64-bit conventions that nobody has documented yet).
* Not supported yet: interrupt handlers, vectors that intercept, events and callbacks (`cmunge64` refuses them with a message), runnable modules, sockets, C++ exceptions.

## Testing with Pyromaniac

`riscos64/tools/run_on_service.py` runs a module and its test program on RISC OS Pyromaniac through the hosted [RISC OS Build service](https://build.riscos.online/) in about 20 seconds (jobs for every example are in the script); see [RISCOS64.md](RISCOS64.md) for the differences between Pyromaniac and RISC OS that have been found.
