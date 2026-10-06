# Licences

## The repository

The scripts, tools, test programs, modkit and documents written for this project are **Copyright (C) 2026 Rob Heaton** and released under the **GNU General Public License, version 3 or later** (the text is in [LICENSE](LICENSE) and [licenses/GPL-3.0.txt](licenses/GPL-3.0.txt)).

Patches and new files that become part of another project's source keep **that project's licence**. A patch contains lines of the file it changes, so it carries the file's licence.

| Path | Licence |
|---|---|
| `recipe/gcc-16.2.0-riscos/patches*`, `new-files*` | the GCC files they change: GPL-3.0-or-later; the libgcc and libstdc++ parts also carry the GCC Runtime Library Exception 3.1 ([text](licenses/GCC-Runtime-Library-Exception-3.1.txt)) |
| `recipe/binutils-2.45.1-riscos/patches` | binutils: GPL-3.0-or-later |
| `recipe/make-4.4.1-riscos/patches` | GNU make: GPL-3.0-or-later |
| `recipe/gcc-16.2.0-riscos/patches-unixlib` | UnixLib: the revised BSD licence for most files, some files under the GNU Library General Public Licence (LGPL 2) or other BSD-style notices: the licence of the file each patch changes. UnixLib's own statement is in [licenses/UnixLib-COPYING.txt](licenses/UnixLib-COPYING.txt); the LGPL text is in [licenses/LGPL-2.0.txt](licenses/LGPL-2.0.txt) |
| `docs/upstream/patches`, `docs/upstream/src` | the same rule: UnixLib (BSD/LGPL), SharedUnixLibrary and ARMEABISupport (GCCSDK's licences) |
| everything else (scripts, `tools/`, `tests/`, `modkit/`, `modpoc/`, `docs/`) | GPL-3.0-or-later |

## The binaries on the releases page

| Binary | Licence |
|---|---|
| GCC (`gcc`, `g++`, `gfortran`, `cc1`, `cc1plus`, `f951`, `lto1` ...), binutils, GNU make | GPL-3.0-or-later |
| libstdc++, libgfortran, libgcc | GPL-3.0-or-later **with the GCC Runtime Library Exception**, so programs you build may be licensed as you like |
| UnixLib (`libunixlib`, `libm`), the loader `ld-riscos`, `libdl` | UnixLib's licence: the revised BSD licence for most files, LGPL for some |
| `libgcc_s.so.1` in the C runtime package | GPL-3.0-or-later with the GCC Runtime Library Exception (GCC 10.2.0's, from GCCSDK) |
| the icon sprites in `!GCC16` | GCCSDK's, from its own `gcc` package |
| the module `SharedULib` in `SharedULibFix` | UnixLib's licence (the revised BSD licence for most files, LGPL for some): it is `sul.s` of UnixLib with three patches |
| the programs and Obey files of `SharedULibFix` (`sulfile`, `Install` ...) | GPL-3.0-or-later |

The **source** for each binary is the unmodified upstream tarball plus this repository at the release tag: see [SOURCES.md](SOURCES.md). The release also attaches the upstream tarballs and a snapshot of the UnixLib sources.

## No warranty

This software comes with **no warranty** (GPL-3.0 sections 15 and 16). It is experimental, tested on one machine, and some parts (the modkit, the fixed system module patches) can crash a RISC OS machine if they are wrong.

## How it was made

The analysis, the patches and the drafting were done with the help of an AI assistant (Claude); every run on the machine was done by Rob Heaton.
