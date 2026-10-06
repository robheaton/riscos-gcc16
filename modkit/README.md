# modkit

Build RISC OS **relocatable modules** with the GCC 16 EABI cross compiler, with no C library, with the commands of GCCSDK 4.7.4 (`gcc -mmodule`, `cmunge`; since 16.2.0-14). Experimental: see [docs/MODULES.md](../docs/MODULES.md) for what it is, how to build the example (`examples/tickmod`), what was proven on hardware, and the limits.

| Folder | |
|---|---|
| `bin/` | `cmunge` (CMunge's command line), `mkmodhdr.py` (CMHG file to module header and veneers), `mkoslib.py` (OSLib-style SWI veneers, made from the objects that use them), `modreloc.py` (the self-relocating image) |
| `lib/` | the mini C library, integer division, the SWI veneer, the linker script, and the SWI and service numbers read from the RISC OS sources |
| `include/` | the few headers a module needs |
| `module.mk` | make rules to include from a module's Makefile (needs GNU make 4.3 or later) |

The tool chain's `recipe/gcc-16.2.0-riscos/scripts/install-modkit.sh` puts all of it into the cross compiler (`libmodkit.a`, the linker script, the headers, and `cmunge`, `arm-riscos-gnueabihf-modreloc` and `-mkoslib` in `bin/`).
| `examples/tickmod` | a module that claims `TickerV` and uses callbacks |
| `tests/` | host simulation of the module's machine code on an ARM interpreter, against a model of the RISC OS kernel |
