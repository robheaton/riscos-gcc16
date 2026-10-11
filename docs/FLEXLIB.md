# FlexLib and SFLib

Two small C libraries for RISC OS Wimp programs, both from the GCCSDK world and both built here, from your own copies of their sources, for the cross compiler and UnixLib:

| Library | What | Source | Licence | You get |
|---|---|---|---|---|
| **FlexLib** | Acorn's *flex* heap: blocks that the library moves, so that a Wimp program can grow and shrink them without fragmenting its slot (or in a dynamic area) | [github.com/steve-fryatt/flexlib](https://github.com/steve-fryatt/flexlib) | Apache License 2.0 (Copyright 1997 Acorn Computers Ltd) | `libFlexLib32.a`, `flex.h` |
| **SFLib** | Stephen Fryatt's Wimp libraries: windows, menus, icons, events, saving, templates, message files, configuration, a heap on a flex block, string and URL helpers | [github.com/steve-fryatt/sflib](https://github.com/steve-fryatt/sflib) | European Union Public Licence 1.2 (compatible with the GPL v2) | `libSFLib32.a`, `sflib/*.h` |

**Neither library is in the releases of this project**, as with [OSLib](OSLIB.md) and [DeskLib](DESKLIB.md): what is here is the recipe (two scripts and one patch). SFLib may be used in software under the EUPL or a compatible licence: read its `Licence`.

## Building them

```bash
git clone https://github.com/steve-fryatt/flexlib ~/flexlib
git clone https://github.com/steve-fryatt/sflib ~/sflib
recipe/gcc-16.2.0-riscos/scripts/install-oslib.py   --oslib-src <OSLib checkout> --toolchain <the tool chain>     # SFLib needs OSLib ...
recipe/gcc-16.2.0-riscos/scripts/install-flexlib.py --flexlib-src ~/flexlib --toolchain <the tool chain>          # ... and FlexLib
recipe/gcc-16.2.0-riscos/scripts/install-sflib.py   --sflib-src ~/sflib --toolchain <the tool chain>
```

* `install-flexlib.py` applies [`patches-flexlib/flexlib-unixlib.patch`](../recipe/gcc-16.2.0-riscos/patches-flexlib/README.md), compiles `flex.c`, makes `libFlexLib32.a`, links a test program and installs `flex.h` and the library in the tool chain.
* `install-sflib.py` compiles the 20 files of the library (the list of objects is read from SFLib's own Makefile; `strdup.c` is for the Norcroft compiler only, as in that Makefile), makes `libSFLib32.a`, links every member into a program with FlexLib and OSLib, and installs the library and the 21 headers (`include/sflib/`). The sources are not changed. The author's makefiles (`-mlibscl`) are replaced by the script.
* Both libraries have **exactly the symbols of GCCSDK's own old-ABI builds** (11 for FlexLib, 216 for SFLib: `comm` finds no difference).

## Using them

```c
#include <flex.h>
#include "sflib/event.h"
#include "sflib/msgs.h"
```
```bash
arm-riscos-gnueabihf-gcc -O2 -std=gnu99 -o !RunImage,e1f main.c -lSFLib32 -lFlexLib32 -lOSLib32
```

The order matters (SFLib calls FlexLib and OSLib). Write the SFLib headers with the `sflib/` prefix and do not put SFLib's own `src` folder on the include path with `-I`: it has a `string.h` that would hide the C library's.

## The one change to FlexLib, and flex under UnixLib

FlexLib was written for the Shared C Library, which asks a registered procedure to make room when the C heap grows (`_kernel_register_slotextend`): that is what `flex_set_budge` sets. UnixLib has no such call, and its C heap does not move flex blocks. The patch makes `flex_set_budge` do nothing under UnixLib: it returns 0 ("do not budge"), or -1 when flex lives in a dynamic area, as before. Everything else is the original code.

**Both of flex's memory modes work with UnixLib** (tested on the Pi, below): `flex_init (name, 0, 0)` puts the store in the application's slot, and `flex_init (name, 0, size)` in a dynamic area. The dynamic area does not depend on the slot at all and is the safer choice for a program that also uses a lot of `malloc`; a program in slot mode needs a `WimpSlot` big enough for both.

## What was tested

[`modkit/tests/flexsf/flxsf.c`](../modkit/tests/flexsf/flxsf.c) is a Wimp task that writes one line per check to a file and shows its totals. On the Raspberry Pi (RISC OS 5.30, 2026-10-11) it ran **in both modes, 57 checks each, 0 failed**:

* FlexLib: `flex_alloc`, `flex_extend` (growing and shrinking, with the blocks above it moving and their anchors following), `flex_midextend` (inserting and removing in the middle), `flex_reanchor`, `flex_free`, 40 blocks of 10 to 30 KB with every other one freed, a block of 3 MB, 1 MB of `malloc` while flex blocks exist (the flex blocks stay intact, and so do the `malloc` blocks), `flex_compact`, `flex_set_budge`.
* SFLib: the string functions, `stack_*`, `heap_*` (a heap on a flex block, with `heap_extend`), message files (`msgs_lookup` with parameters) and the configuration options.

Writing the test showed how a few functions behave; they are the library's, not the port's: `flex_midextend (&p, at, by)` with a negative `by` moves the bytes after `at` down to `at + by` (so `at` must be at least `-by`); `flex_reanchor` clears the old anchor and cannot take the same anchor twice; `string_nocase_strstr` returns a pointer to the end of the string, not NULL, when the text is not there; `stack_pop` only looks at the top (`stack_pull` removes it); `general_mode_width` and `_height` are in OS units (3840 x 2160 for a 1920 x 1080 screen with eigen values 1); `config_restore_default` is not implemented by SFLib (its own header says so); and `heap_initialise` has a stray `;` after its `if (error != NULL)` (harmless).

## Not done

SFLib's unit tests (`test/`), its Doxygen manual and its `norcroft` folder (the AOF library for the Norcroft compiler); a flavour for modules (they are libraries for Wimp programs); a native build on RISC OS.
