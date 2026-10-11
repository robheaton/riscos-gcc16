# RUfl

RUfl is NetSurf's **RISC OS Unicode font library**: it draws, measures and splits text in any Unicode character (UTF-8 in, Font Manager out), choosing among the fonts the machine has and **substituting** a font that has the character when the one you asked for has not. It is what NetSurf uses for all its text. Source: `git://git.netsurf-browser.org/librufl.git` (MIT licence).

**It is not in the releases of this project** (as with [OSLib](OSLIB.md), [DeskLib](DESKLIB.md) and [FlexLib and SFLib](FLEXLIB.md)): the recipe is here, and it builds the library from your own checkout. The MIT licence would allow more, but the rule is the same for every library of someone else's.

## Building it

```bash
git clone git://git.netsurf-browser.org/librufl.git ~/librufl
recipe/gcc-16.2.0-riscos/scripts/install-oslib.py --oslib-src <OSLib checkout> --toolchain <the tool chain>     # RUfl needs OSLib
recipe/gcc-16.2.0-riscos/scripts/install-rufl.py  --rufl-src ~/librufl --toolchain <the tool chain>
```

It needs Perl: the table of glyph names (`rufl_glyph_map.c`) is made from `src/Glyphs` by the library's `tools/makeglyphs`. NetSurf's own build system is not needed: the script compiles the files that `src/Makefile` lists (10 files and the generated table) with the flags of the library's Makefile (`-std=c99 -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=500`), links every member into a program, and installs `librufl.a` and `rufl.h`.

**One patch** ([`patches-rufl/rufl-log-off.patch`](../recipe/gcc-16.2.0-riscos/patches-rufl/README.md)): the library's debug log is hard-wired on in the source (`#if 1 /*ndef NDEBUG*/`), so each font it cannot find prints a line with the file name to stderr, and in a Wimp program stderr is the screen. The patch compiles the log only when `RUFL_LOG` is defined.

## Using it

```c
#include "rufl.h"
```
```bash
arm-riscos-gnueabihf-gcc -O2 -std=gnu99 -o !RunImage,e1f main.c -lrufl -lOSLib32
```

`rufl_init ()` scans every font on the machine (it takes a moment); then `rufl_paint`, `rufl_width`, `rufl_split`, `rufl_x_to_offset`, `rufl_font_metrics`, `rufl_glyph_metrics`, `rufl_font_bbox`, `rufl_decompose_glyph` and `rufl_paint_callback` take a font family name, a style (`rufl_WEIGHT_400` ... `rufl_WEIGHT_900`, `| rufl_SLANTED`), a size in sixteenths of a point and a UTF-8 string. Call `rufl_invalidate_cache ()` on a mode change.

## What was tested

[`modkit/tests/rufl/rtest.c`](../modkit/tests/rufl/rtest.c) is a Wimp task. On the Raspberry Pi (RISC OS 5.30, 2026-10-11): **20 checks, 0 failed**: `rufl_init`; the font list; widths (positive, longer text wider, twice the size about twice the width, an empty text 0); the euro sign, Greek and Cyrillic (the width is found through substitution); bold and slanted; `rufl_split` and `rufl_x_to_offset`; font and glyph metrics and the bounding box; a glyph outline (`rufl_decompose_glyph` of `A` gives moves and lines or curves); `rufl_paint_callback`; and the same width after `rufl_invalidate_cache`. The task then opens a window and paints six lines with `rufl_paint`: Homerton, Trinity and Corpus showed the accented letters, euro, pound, dash, ellipsis, Greek and Cyrillic, the last two through font substitution.

## Not done

The library's own tests (`test/`, a harness with mocks for a host build); the Python module; `pkg-config` files.
