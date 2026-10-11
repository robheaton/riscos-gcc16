# DeskLib

DeskLib is the C library for RISC OS Wimp programs. With this tool chain you can build it, from a copy of **DeskLib32** ([github.com/adyoull/desklib32](https://github.com/adyoull/desklib32): DeskLib with the fixes for RISC OS 5), into the two libraries that GCCSDK 4.7.4 had:

| Library | For | Use |
|---|---|---|
| `libDesk.a` | Wimp programs that use UnixLib | `-lDesk` |
| `libDesk-sclmodule.a` | modules (`gcc -mmodule`) that use the C library of [the module kit](MODULES.md) | `-lDesk-sclmodule` |

Not built: `libDesk-scl.a` (programs on the Shared C Library: programs here use UnixLib), and a shared `libDesk.so`.

## The licence, and why this project does not ship it

DeskLib is not under the GPL. It has its own licence: the copyright messages and conditions must stay with every copy, **only unaltered copies may be passed on** (changes go to the moderator), no profit may be made from distributing it, and software built with it must say so. DeskLib32 documents its changes file by file ([MODIFICATIONS.md](https://github.com/adyoull/desklib32/blob/main/MODIFICATIONS.md)) and keeps the licence text in its `LICENCE`.

So, as with OSLib, **neither DeskLib, nor its headers, nor the built library are in the releases of this project.** What is shipped is the recipe: you get DeskLib32 from its authors, and `install-desklib.py` builds the libraries from your copy and puts them in your tool chain. A program built with DeskLib should say so in its documentation, for example: *"This program uses DeskLib, the FreeWare C library for RISC OS machines."*

## Building it

```bash
git clone https://github.com/adyoull/desklib32 ~/desklib32
recipe/gcc-16.2.0-riscos/scripts/install-desklib.py --desklib32 ~/desklib32 --toolchain <the tool chain> [--flavour unixlib|module|both]
```

It compiles the 271 C files with the cross compiler (`-O2 -std=gnu99 -fcommon`) and assembles the 259 assembler files with `asasm`, GCCSDK's assembler (the files are in ObjAsm syntax; `asasm` is in `~/gccsdk/cross/bin`, or give `ASASM=`). Two things are done to the assembler objects: `asasm` writes ELF files with no EABI version, which the linker will not merge into an EABI 5 program, so the version is set; and for the module library the code areas (`D$$code`, `C$$code`) are renamed `.text`, the only code section the linker script of a module takes. The script then links every member of each library into a test program (for the module library: into a module, run through `modreloc`) and stops if a symbol is missing. It installs the headers in `arm-riscos-gnueabihf/include/DeskLib` and the libraries in `arm-riscos-gnueabihf/lib`.

## Using it

```c
#include "DeskLib:Core.h"
#include "DeskLib:Wimp.h"
```
```bash
arm-riscos-gnueabihf-gcc -O2 -o !RunImage,e1f main.c -lDesk
```

A name with a RISC OS prefix (`DeskLib:Core.h`) is read by the cross compiler of 16.2.0-21 and later like this, the case of the prefix and of the file name not mattering (RISC OS names are not case sensitive: `Desklib:Colourtran.h` is `ColourTran.h`): (1) in the directories of the environment variable `DESKLIB_PATH` (separated by commas), (2) in a directory called `DeskLib` (any case) in the include path, so `-I <tool chain>/arm-riscos-gnueabihf/include` is enough, (3) otherwise the prefix is dropped and the name is searched like any other, so `-I <folder of the headers>` works too. Other prefixes work the same way with their own `PREFIX_PATH`. The native compiler on RISC OS asks the OS, which reads `DeskLib$Path` as `!DeskLib` sets it. With an older cross compiler the script makes a folder of links named `DeskLib:Core.h` for its own build; a program needs the same, or a `sed` over its includes.

A module links `-lDesk-sclmodule`. DeskLib is a library for Wimp programs: in a module the parts that make sense are the ones that only use SWIs and strings (`Environment_`, `File_`, `Filing_`, `Str`, `Time_`, `Screen_`, `SWI`, `Module_` ...); `Mem`, DeskLib's heap that grows the Wimp slot, is there for linking but a module has no slot.

## What was tested

* Every one of the 530 objects (271 C, 259 assembler) builds, and each library links as a whole. The symbols of `libDesk.a` are those of GCCSDK's own `libDesk.a` but for the 6 `BackTrace_` functions (the sources compile them only for the Shared C Library, which has `_kernel_unwind`: GCCSDK's build has them from another source) and a few names that DeskLib32 renamed or added.
* On the Raspberry Pi (RISC OS 5.30, 2026-10-11): `dltest`, a program for a Task window with 33 checks of the assembler veneers and the C functions that need no Wimp task (strings that end in CR, system variables, `OS_GSTrans`, `OS_CLI`, files, the clock, the screen mode, the keyboard, the RMA, the generic `SWI` veneer, the hourglass): all passed; `DLMod`, a module with the same checks in SVC mode, linked with `libDesk-sclmodule.a` and the C library of the kit: all passed; `!DLWimp`, a Wimp task with an icon on the icon bar (`Event_Initialise`, `Icon_BarIcon`, `Menu_New`, `Error_Report`), started, showed its box with the date and the screen size, and its Menu and Quit worked. `Screen_CacheModeInfo` gives the size in OS units (3840 x 2160 for a 1920 x 1080 screen with eigen values 1): `screen_size >> screen_eig` is the size in pixels.

* The 11 example programs of upstream DeskLib (`Examples`: `!Timer`, `!Tiles`, `!Tiles2`, `!Redraw`, `!IncDec`, `!Slider`, `!SpriteTst`, `!FontDemo`, `!Dialog2EG`, `!Pane2test`, `!TestApp`) build with the cross compiler and `libDesk.a`, and were run from the desktop on the Pi (2026-10-11); `modkit/tests/desklib/build-examples.py` builds them. They are written for old compilers: GCC 14 and later make an `int` from a pointer, a call of a function that has no declaration and the like errors, so they need `-fpermissive` (your own old DeskLib programs will too, or need the fixes). One needed more than that, and it is a change of DeskLib, not of the port: **since DeskLib 2.41, `Event_Initialise` asks the Wimp for no messages** (before, for all of them). A program that needs a message must ask for it: the Info entry of `!Dialog2EG` is a submenu that is opened by `Message_MenuWarning` and did nothing until the task started with `Event_Initialise3 (name, 310, wanted)` and `wanted` was `{ 0x400C0, 0 }`. (`Event_InitNested` asks for no messages either.)

## Not done

`BackTrace` in programs and modules (the 6 functions); `libDesk-scl.a`; a shared library; the examples of DeskLib have not been built here.
