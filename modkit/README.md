# modkit

Build RISC OS **relocatable modules** with the GCC 16 EABI cross compiler, with no C library, with the commands of GCCSDK 4.7.4 (`gcc -mmodule`, `cmunge`; since 16.2.0-14). Experimental, for small and medium C modules for now (a C library of about 165 functions of its own, with `stdio` files, no C++: [what can and cannot be built](../docs/MODULES.md#what-can-be-built-today-and-what-cannot)). See [docs/MODULES.md](../docs/MODULES.md) for what it is, how to build the example (`examples/tickmod`), what was proven on hardware, and the limits.

| Folder | |
|---|---|
| `bin/` | `cmunge` (CMunge's command line), `mkmodhdr.py` (CMHG file to module header and veneers), `mkoslib.py` (OSLib-style SWI veneers, made from the objects that use them), `modreloc.py` (the self-relocating image) |
| `lib/` | the C library of the kit (one source per group of functions), integer division, the SWI veneer, the linker script, and the SWI and service numbers read from the RISC OS sources |
| `include/` | the few headers a module needs |
| `module.mk` | make rules to include from a module's Makefile (needs GNU make 4.3 or later) |

The tool chain's `recipe/gcc-16.2.0-riscos/scripts/install-modkit.sh` puts all of it into the cross compiler (`libmodkit.a`, the linker script, the headers, and `cmunge`, `arm-riscos-gnueabihf-modreloc` and `-mkoslib` in `bin/`).
| `examples/tickmod` | a module that claims `TickerV` and uses callbacks |
| `tests/` | host simulations of the module's machine code on an ARM interpreter, against a model of the RISC OS kernel; the library test against glibc (`libtest/`); the tool tests |
