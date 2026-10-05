# modkit

Build RISC OS **relocatable modules** with the GCC 16 EABI cross compiler, with no C library. Experimental: see [docs/MODULES.md](../docs/MODULES.md) for what it is, how to build the example (`examples/tickmod`), what was proven on hardware, and the limits.

| Folder | |
|---|---|
| `bin/` | `mkmodhdr.py` (CMHG file to module header and veneers), `mkoslib.py` (OSLib-style SWI veneers), `modreloc.py` (the self-relocating image) |
| `lib/` | the mini C library, integer division, the SWI veneer, the linker script, and the SWI and service numbers read from the RISC OS sources |
| `include/` | the few headers a module needs |
| `module.mk` | make rules to include from a module's Makefile |
| `examples/tickmod` | a module that claims `TickerV` and uses callbacks |
| `tests/` | host simulation of the module's machine code on an ARM interpreter, against a model of the RISC OS kernel |
