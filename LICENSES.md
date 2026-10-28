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
| `recipe/gcc-16.2.0-riscos/patches-unixlib/unixlib-thread-ticker.patch` (new in 16.2.0-24) | the thread ticker of **Andrew Youll's riscos-unixlib** (UnixLib 5.0.3.3; `pthread/ticker.c`, `incl-local/internal/ticker.s`, `module/pthticker.s` and his changes to other files): © 2026 UnixLib Developers as his files say, under UnixLib's revised BSD licence (the text of the PThreadTicker licence is the BSD one that UnixLib uses); his comments and notices are kept |
| `docs/upstream/patches`, `docs/upstream/src` | the same rule: UnixLib (BSD/LGPL), SharedUnixLibrary and ARMEABISupport (GCCSDK's licences) |
| `modkit/include/swisnums.h` (new in 16.2.0-16) | the names and numbers of the SWIs, made by `modkit/bin/mkswis.py` from the assembler headers of the RISC OS Open sources, which are under the **Apache License 2.0** ([text](licenses/Apache-2.0.txt); Castle Technology Ltd, RISC OS Open Ltd and others; the notice is in the file's first lines, and the text is in the `licenses/` folder of the Linux tarball and as `docs/Apache-2.0` in the `Gcc16` package (the file `Apache-2/0` of the folder `docs` on RISC OS, where a dot in a name is a directory separator); Apache-2.0 material may be included in a GPL-3.0-or-later work). It holds macros only |
| `modkit/lib/fd_*.c`, `fdlibm.h` (new in 16.2.0-17) | Sun Microsystems' **fdlibm** 5.3 (GNU Classpath's copy), which is free to use, copy, modify and distribute if its notice stays (the notice is at the top of each file; `lib/fdlibm.h` lists the changes) |
| `recipe/extra-libs/`, `tests/extra-libs/` | GPL-3.0-or-later (the scripts and test programs of this project; the libraries themselves are libffi and OpenSSL, under their own licences, as above) |
| everything else (scripts, `tools/`, `tests/`, `modkit/`, `modpoc/`, `docs/`) | GPL-3.0-or-later |

## The binaries on the releases page

| Binary | Licence |
|---|---|
| GCC (`gcc`, `g++`, `gfortran`, `cc1`, `cc1plus`, `f951`, `lto1` ...), binutils, GNU make | GPL-3.0-or-later |
| libstdc++, libgfortran, libgcc | GPL-3.0-or-later **with the GCC Runtime Library Exception**, so programs you build may be licensed as you like |
| UnixLib (`libunixlib`, `libm`), the loader `ld-riscos`, `libdl` | UnixLib's licence: the revised BSD licence for most files, LGPL for some |
| `swisnums.h` among the module kit's headers (in `Gcc16` and in the Linux tarball) | Apache License 2.0 (text: `docs/Apache-2.0` in `Gcc16`, shown as `Apache-2/0` on RISC OS; `licenses/Apache-2.0.txt` in the tarball); the rest of the kit: GPL-3.0-or-later |
| `libgcc_s.so.1` in the C runtime package | GPL-3.0-or-later with the GCC Runtime Library Exception (GCC 10.2.0's, from GCCSDK) |
| the icon sprites in `!GCC16` | GCCSDK's, from its own `gcc` package |
| libffi 3.8.0 in the library package `libs-armeabihf-1` | the MIT licence ([text](licenses/libffi-MIT.txt); copyright Anthony Green, Red Hat and others, as the source files say) |
| OpenSSL 1.1.1w in the library package `libs-armeabihf-1` | the OpenSSL licence and the SSLeay licence ([text](licenses/OpenSSL-1.1.1-LICENSE.txt)); the advertising clause applies to material that mentions features of the library: *This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit (http://www.openssl.org/)*. The package carries both texts in `share/licenses/riscos-extra-libs/`; the sources are attached to the release |
| the module `SharedULib` in `SharedULibFix` | UnixLib's licence (the revised BSD licence for most files, LGPL for some): it is `sul.s` of UnixLib with three patches |
| the programs and Obey files of `SharedULibFix` (`sulfile`, `Install` ...) | GPL-3.0-or-later |

The **source** for each binary is the unmodified upstream tarball plus this repository at the release tag: see [SOURCES.md](SOURCES.md). The release also attaches the upstream tarballs and a snapshot of the UnixLib sources.

## No warranty

This software comes with **no warranty** (GPL-3.0 sections 15 and 16). It is experimental, tested on one machine, and some parts (the modkit, the fixed system module patches) can crash a RISC OS machine if they are wrong.

## How it was made

The analysis, the patches and the drafting were done with the help of an AI assistant (Claude); every run on the machine was done by Rob Heaton.
