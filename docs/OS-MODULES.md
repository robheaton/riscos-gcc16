# The C modules of the RISC OS Open sources, built with modkit

> **New in 16.2.0-16.** This describes the work after 16.2.0-15: the changes to the kit that it led to are in the `modkit/` sources and in the packages of 16.2.0-16 (the programs in `tools/` are in this repository only). Three of the modules were also run on a Raspberry Pi, in place of the ones in its ROM, and gave the same results in everything the program compares ([below](#compared-with-the-rom)); the other 26 were not run on a machine: see [what was checked](#what-was-checked).

[modkit](MODULES.md) builds relocatable modules with `gcc -mmodule` and `cmunge`, and so far it had been tested with small modules written for it. The RISC OS Open sources (the BCM2835 subset) have **66 components that build C modules**, written for the Norcroft C compiler and the Shared C Library, 184,000 lines of C in all. They are a test that the kit's authors did not write, so the kit was held against them: **each of the 66 was built the way the OS build builds it, with the kit in place of the Norcroft tools and the Shared C Library.**

**The result: 29 of the 66 build and link into a module image, with no change to their sources.** For 40 of them every C and assembler file compiles; for the 37 that do not link the table [below](#what-stops-the-others) says what stops each one, and it is not, with a few exceptions, the kit.

## How to run it

```bash
# SOURCES: a checkout of the RISC OS Open build environment (the folder that has Sources, BuildSys and Library)
# TC: the Linux cross tool chain, with the module kit installed;  ASASM: GCCSDK's asasm (any recent GCCSDK has it), for the OS's assembler sources
tools/build-os-modules.py --sources $SOURCES --tc $TC --asasm $ASASM --out /var/tmp/osmods    # about ten seconds
cat /var/tmp/osmods/report.txt                                  # one line per module, and what stops it
tools/modinfo.py /var/tmp/osmods/m/*/tree/*/*/objs/*            # (or the file of one module) the header and the relocation table of a module image
RISCOS_SOURCES=$SOURCES/Sources modkit/tests/sim-osmodules.py /var/tmp/osmods/results.json    # each module in the interpreter's model of RISC OS
```

What `tools/build-os-modules.py` does is what the OS build does, done by a program instead of GNU make and the Norcroft tools (it reads the Makefiles of the components for the object lists, the defines and the include paths):

1. **The headers**: the OS's own `Hdr2H` makes C headers of the assembler headers (`Global/Services.h`, `Interface/GPIODevice.h` ...; a hand-written `h/X` goes in front of the converted `hdr/X`, as the OS's `FAppend` rules do; the one header that the OS builds by assembling (`SDFSErr`) is assembled with `asasm`). The headers of the libraries are collected the way the OS exports them (OSLib, RISC_OSLib, the Toolbox libraries, TCPIPLibs, DebugLib ...). The SWI numbers are the kit's own `swis.h`.
2. **The libraries** that the modules link are built from the OS's own sources: AsmUtils, callx, ConfigLib, DebugLib, the Toolbox library, the TCP/IP libraries (inetlib, unixlib and socklib, whose SWI veneers the OS makes with a Perl script) ... Their C goes through `gcc -mmodule -std=c99` as the OS build does; their assembler through `asasm`, with the `AREA` names turned into `.text.NAME` and the like so that the module's linker script takes them.
3. **Each module**: `cmunge -p` on its CMHG file, `gcc -mmodule` on its C files, `asasm` on its assembler files, the resource object that `resgen` makes (the module's Messages file in ResourceFS format), the OSLib veneers (`mkoslib --from-objects`), and the link with the driver, which runs `modreloc`. The environment is that of the Pi build (`MACHINE=RPi`, `USERIF=Raspberry`).

The only help the sources get is a header (`norcroft.h`) that makes the Norcroft keywords `__packed`, `__value_in_regs` and `__va_list` harmless, and the compiler is told not to stop at what the OS code has always got away with (`-fpermissive`).

## The 29 that build and link

| Module | Bytes | What it is |
|---|---:|---|
| `Audio/SoundCtrl` | 26,476 | the sound mixer's control module |
| `HWSupport/CD/CDFSSoftSCSI` | 17,676 | the software SCSI driver of the CD file system |
| `HWSupport/GPIO` | 14,644 | GPIO devices on the HAL |
| `HWSupport/PortMan` | 9,832 | the port manager |
| `HWSupport/RTC` | 14,744 | the real time clock device |
| `HWSupport/SD/SDIODriver/Test/FakeCardInt` | 34,408 | a test module of the SD driver |
| `Networking/AUN/Access/Freeway` | 31,236 | Freeway, the service directory |
| `Networking/AUN/Net` | 49,240 | the Net module (Econet over IP) |
| `Networking/DHCP` | 36,628 | the DHCP client |
| `Networking/MimeMap` | 27,592 | the MIME map |
| `Networking/Omni/Protocols/OmniLanManFS` | 152,520 | the LanManager file system |
| `Programmer/Squash` | 17,560 | compression (with its assembler) |
| `Toolbox/Toolbox`, and `ColourDbox`, `ColourMenu`, `DCS`, `FileInfo`, `FontDbox`, `FontMenu`, `IconBar`, `Menu`, `PrintDbox`, `ProgInfo`, `SaveAs`, `Scale` | 16,092 - 27,392 each | the Toolbox and the 12 object modules that use the Toolbox library |
| `Video/Render/DrawFile` | 48,872 | Draw file rendering |
| `Video/UserI/BootFX` | 31,152 | the boot bar |
| `Video/UserI/ScrModes` | 53,824 | screen modes and the monitor files |
| `Video/UserI/ScrSaver` | 10,892 | the screen saver service |

Together 745,000 bytes. Each is a module image with the header words, the command table, the SWI chunk and the names that its CMHG file says, and a relocation table that `tools/modinfo.py` checks (the table ends the file; every word it lists holds an address inside the image).

## What stops the others

| How many | What stops them | Which |
|---:|---|---|
| 4 | the CMHG directives `library-enter-code` and `library-initialisation-code`, which redirect the start-up of the Shared C Library (`cmunge` refuses them, and says why) | ShellCLI, URI, MakePSFont, Internet |
| 1 | `swi-handler-code: f (flags-capable:)`, an extension of the newer Norcroft CMHG, not in GCCSDK's CMunge either | TerritoryManager |
| 4 | Norcroft inline assembler (`__asm { ... }`), which GCC cannot read | BCMSupport, SDIODriver (two files), TimeFGBG, EtherUSB |
| 4 | the USB stack's headers, which the OS build makes and this program does not | SCSISoftUSB, DWCDriver, XHCIDriver, USBDriver |
| 4 | the run-time symbols of the Shared C Library and the Norcroft compiler that assembler sources import (`_Lib$Reloc$Off$DP`, `__current_sp`, `_clib_at_destruction`) | DOSFS, PCCardFS, SCSISwitch, the `atexit` test |
| 2 | functions of OSLib that `mkoslib` cannot make: the SWIs whose parameters are "components" of a block in memory (`wimp_delete_icon`) or whose comment does not say a register | Picker, BootCmds |
| 2 | the Wimp C library of the OS (`RISC_OSLib/rlib`: `dbox_`, `event_`, `wimp_`), which is not built here | FilerAct, WindowScroll |
| 2 | C that GCC 16 rejects (an assignment to a `const`, a non-constant initialiser) | RTSupport, Gadgets |
| 2 | a constant that the exported headers do not have | SDFS, ToolAction |
| 1 | SyncLib's assembler, where `asasm` rejects an objasm macro rule | EtherGENET |
| 1 | the VFP assembler of VFPSupport (`asasm` has no VFP syntax) | VFPSupport |
| (1) | floating point (`math.h`), already counted in the first row (the kit has `math.h` since 16.2.0-17, and the scan counts 66 of 66 now; MakePSFont's other obstacles are unchanged) | MakePSFont |
| 1 each | headers that are not in the sources (libpng, VideoCore, the Linux SDIO headers), `uchar.h` (included by a test of the Shared C Library), an include path of the RISC OS folder layout (AbortTrap), a RISC OS dotted include name (Window), an assembler header that is not found (BCMVideo: BCM2835Reg; Debugger: ExcDump, with `excdump.h`), code that the Makefile builds another way (`longcmd`), and SDCMOS, which has no object to build | CompressPNG, VCHIQ, FakeLibInt, `atomic`, AbortTrap, Window, BCMVideo, Debugger, `longcmd`, SDCMOS |

So of the 37: **5** are stopped by CMHG features that the kit leaves out on purpose; **13** by source that only the Norcroft tools read (inline assembler, run-time symbols of the Shared C Library, C that GCC 16 rejects, the VFP assembler, include paths of the RISC OS folder layout); **15** by libraries, headers and constants of the OS that this program does not provide (the USB stack's headers, the Wimp C library, libpng, the VideoCore and SDIO headers, SyncLib's assembler, two constants that the exported headers lack ...); **2** are limits of the kit's own `mkoslib` (Picker and BootCmds: see [below](#what-the-survey-changed-in-the-kit)); one needs `uchar.h`, which only a test of the Shared C Library includes; and `SDCMOS` has nothing in its Makefile to build.

## What was checked

* **Built**: every module above compiles, assembles and links; the module is made by the driver's `modreloc` step. 18 of the 29 also run their initialisation and finalisation code in the interpreter's model of RISC OS (`modkit/tests/sim-osmodules.py`), which finds a crash in a module's start-up code (a bad relocation, a stack that is not 8 byte aligned); the other 11 stop where the model has no answer (the HAL, dynamic areas, `MessageTrans`, a network station that is not configured: Net says "NotConf", as it should), and that says nothing about the modules.
* **Compared with the real CMunge**: the CMHG files of the 61 components that have one were made into module headers by `cmunge` and by GCCSDK's CMunge and the decoded headers compared (`modkit/tests/os-cmhg-corpus.py --real`): `cmunge` accepts 56 of the 61 components that have a CMHG file; where the real CMunge accepts the file too (40 of the 56) the headers are the same (one differs only in the date that CMunge adds when a file has no `date-string`); the other 5 are the ones in the first two rows of the table above.
* **Run on the interpreter against the ROM-comparison program**: `modkit/tests/sim-romcmp.py` runs the test program of [the ROM comparison](#compared-with-the-rom) with the GCC builds of Squash, MimeMap and DrawFile loaded as modules (the real ARM code, with a model of the OS around it): the program's own checks pass (501 with `--quick`, 545 at full size), `gzip -dc` agrees with all of Squash's compressions and decompressions, two runs give the same result file. That is where the bugs under [What it found](#what-it-found) came from.
* **Run on the machine: three of them.** Squash, MimeMap and DrawFile were loaded over the ROM's modules on a Raspberry Pi Compute Module 4 (RISC OS 5.30) and gave the same results as the ROM's in everything the same program compares: 9,820 lines of results, none different, and the same pixel count, bounding box and checksum from DrawFile ([Compared with the ROM](#compared-with-the-rom)). The other 26 have not been run on a machine, and a module that builds is not a module that works.

## What the survey changed in the kit

Building real modules found these, all of them fixed in `modkit/` (the tests that cover each are named in [TESTING.md](TESTING.md)):

* **`cmunge`**: the numbers of a CMHG file after the C preprocessor are constant expressions (the OS's headers turn `Service_ResourceFSStarting` into `(0x60)`); the generated header now defines what CMunge's does and OS sources use (`<prefix>_00`, the `X` names of the SWIs, `error_BAD_SWI`, `arg_CONFIGURE_SYNTAX`, `arg_STATUS`, `configure_BAD_OPTION` ...); **`event-handler:`** is supported (a vector veneer that passes every event that is not in its list on at once); a handler name with options after it, such as `(flags-capable:)`, is an error instead of a bad symbol in the assembler source.
* **`cmunge`'s SWI and command veneers** (found by running the comparison program against the GCC builds, see [below](#what-it-found)): a SWI handler that returns `error_BAD_SWI` (the OS's modules do, from the `default:` of their handler) now gives the error of the system, `SWI value out of range for module <title>` (&1E6), the way CMunge's veneer does; the veneer used to return V set with R0 = -1 and a program that read the error block crashed. A command that returns `configure_BAD_OPTION` (-1) now gives V set with R0 = 0 (what `*Configure` takes for a bad option), as CMunge's does; the veneer used to take it for success. `sim-veneers.py` checks both.
* **`<kernel.h>`**: `size_t`, `_kernel_irqs_on`, `_kernel_irqs_off`, `_kernel_irqs_disabled`, `_kernel_processor_mode`, `_kernel_RMAalloc`, `_kernel_RMAextend`, `_kernel_RMAfree` and the type `_kernel_stack_chunk`: 3 of the 29 modules call at least one of the first three (PortMan, Net, LanManFS; 11 of the 66 do, 7 of them all three).
* **`<swis.h>` has the SWI numbers** (951 SWIs and their `X` names, from the OS's own assembler headers: `include/swisnums.h`, made by `bin/mkswis.py`), `XOS_Bit`, and `_vswi` / `_vswix`. The Shared C Library's header has them; the kit's did not.
* **New headers**: `<inttypes.h>` (the `PRI` and `SCN` macros) and `<signal.h>` (`signal` and `raise`, so far as a module can use them).
* **`<string.h>` and `<ctype.h>` declare the functions that are not ISO C** (`strdup`, `stricmp`, `strlcpy`, `isascii` ...) **only when the program is not strict ISO C** (`-std=c99`, as the OS build uses; `-D_GNU_SOURCE` asks for them): a module that defines its own `stricmp` with another type no longer fails to compile.
* **`module.ld`** defines the symbols the Norcroft linker does (`Image$$RO$$Base` ...), which assembler sources import; **`errno.c`** also defines `__errno`, the name that the TCP/IP libraries' veneers write to.
* **`mkoslib`**: outputs that are "the processor status register" (the flags; 34 more OSLib functions can be made), static libraries (`.a`) given to `--from-objects` (the functions that the library uses are made as far as they can be: a function that cannot be made is a warning), and a function that one of the module's own objects defines is not made again.
* **`modreloc` refuses a module that has sections outside `.image`.** The module linker script places `.text*`, `.rodata*`, `.data*` and `.bss*`; any other section name (an assembler `AREA`, a `__attribute__ ((section ("x")))`) used to be left out of the module without a word, and the module jumped into its own relocation table. It is an error now, and the message says how to rename the section. (The survey found this: DHCP, which called `socket`, ran into the table.)

<a id="compared-with-the-rom"></a>
## Compared with the ROM

A pack for the Raspberry Pi (`romcmp37`; the author's work area has the generator, which is not part of this repository; the test program is `modkit/tests/hwpack/romcmp.c`, and the two mapping files it needs are `modkit/tests/hwpack/TestMap` and the OS's own `MimeMap`) runs three of the 29 against the originals in the ROM of the machine, with the same program and the same inputs, and compares what comes out:

| Module | Why this one | What the program asks |
|---|---|---|
| **Squash** (12 KB; C, and the fast compressor and decompressor in assembler that GCCSDK's `asasm` assembled) | the compression is a function of its input alone: the answer is the same or it is not | 11 inputs (empty, 1 and 2 bytes, `TOBEORNOTTOBEORTOBEORNOT`, zeros, counting, text, random, runs, 30 KB of text, 150 KB of mixed data that fills and clears the code table) compressed by the fast code, by the restartable code in one call and in pieces; every stream decompressed in the same ways (the unused input is given again with the next piece, as the interface says); a stream cut short, damaged, an output buffer that is too small, bad flags and lengths; a guard behind every buffer and the work space |
| **MimeMap** (22 KB; C: `fopen`, `strtok`, `malloc`, `sprintf`, `OS_FSControl`, `MessageTrans`) | a lookup table read from a file | every file type &000 - &FFF to MIME type, file type, name, extension and all extensions; every MIME type, extension, file type name and number of a mapping file back; for a test file whose content is known the exact answers; the bad reasons; `*MimeMap` with each kind of argument, `*ReadMimeMap`. Two mapping files: the test file and the OS's own |
| **DrawFile** (37 KB; C, a table of callbacks, about 20 SWIs) | the most code of the three | `DrawFile_BBox` for 8 Draw files and 7 matrices, `DeclareFonts`, `Render` with the suppress flag, 12 damaged files (the verification), and, in a separate run, `Render` into a sprite of 32 bits per pixel: a checksum of the pixels |

A careful first step is `RunRomOnly37`, which does the first part on its own: the ROM modules run the program and nothing is replaced. The run (`RunRom37`): the program checks that the three images are modules **before** anything is replaced; the ROM modules run the program; the three GCC builds are loaded over them (`RMLoad` replaces a module of the same name); the same program runs; the two result files are compared (`RomCmp diff`: the lines that start with `#` are not compared); the GCC builds are removed (`RMKill`, which runs their finalisation) and the ROM modules started again (`RMReInit`); the program runs a third time and must give the first file again. `Restore37` puts the ROM modules back if a run stops half-way. The program also checks what it can by itself, so a failure in the ROM run is a difference between that ROM and the 2026 sources, not a fault of the build.

### Results

Run on a Raspberry Pi Compute Module 4 with RISC OS 5.30 (the resource files in its ROM are dated 24 September 2026) on 8 October 2026, in a Task window; the ROM's modules are Squash 0.31, MimeMap 0.19 and DrawFile 1.62, the same versions as the sources:

| Step | Result |
|---|---|
| the ROM modules run the program | **545 checks, 0 failed.** The ROM passes everything the program expects by itself (the sizes of Squash's work spaces, the guards behind every buffer, the answers that a known mapping file must give, the verification errors of DrawFile ...) |
| the three GCC builds replace them, the same program runs | **545 checks, 0 failed** |
| the two result files compared | **9,820 lines compared, 0 differ.** Squash: 167 lines: 41 compressions (each with the length of its output, its CRC-32 and the number of calls it took), 80 decompressions (each must give the input back), the 11 inputs, 10 comparisons of the fast code with the restartable code, 4 work space sizes and 21 lines for the edge cases (a stream cut short or damaged, an output buffer that is too small, bad flags and lengths, a SWI that Squash does not have); every stream is the same, from the fast code in assembler and from the restartable code. MimeMap: 9,091 lines (4,171 for the test mapping file, 4,920 for the OS's own) and the output of `*MimeMap`, `*ReadMimeMap` and `*Help`. DrawFile: 93 lines: for each of 8 Draw files its size, the box for 7 matrices, `DeclareFonts` and a `Render` with the suppress flag; the errors for 12 damaged files; and the answer to a SWI that it does not have. The error texts are the same too, among them the answer of Squash and MimeMap to a SWI that they do not have, `SWI value out of range for module Squash` (&1E6), which is what the fixed veneer of `cmunge` gives |
| the GCC builds removed (`RMKill`, which runs their finalisation), the ROM modules started again, the program run once more | **545 checks, 0 failed**, and the result file is the first one again |
| `RunDraw37`: DrawFile paints 13 cases made from 8 Draw files (rectangles, a stroked path, a dashed line, a Bezier, a group, a tagged object, text in the system font, a sprite; some of them scaled or turned, with bounding boxes, or with the suppress flag) into a sprite of 32 bits per pixel, ROM and GCC build | **28 checks, 0 failed** for each; the number of pixels painted, their bounding box and the checksum of all the pixels are the same in all 13 cases |

That is **the same behaviour on everything the program tried, for these three**: 72 KB of module code, compiled and linked by GCC 16 and the kit, in the places of the Norcroft-built modules of the ROM. The three files are the ones that the Linux tarball of 16.2.0-16 made: `tools/build-os-modules.py` with the unpacked tarball as `--tc` gave all three byte for byte (12,416, 22,412 and 37,624 bytes), so what ran on the machine was what the tools of that release produced. The kit of 16.2.0-17 makes larger files (17,560, 27,592 and 48,872 bytes: `printf` has the floating point conversions now; a module that wants none defines `__modlib_fmtdouble` and `__modlib_strtofp` and gets the old sizes); those were not run on the machine, but the tests of the library and of the modules that were run there cover the code they share. It is a sample, and the three were chosen to be checkable (a function of its input, a table read from a file, a renderer with a pixel checksum). It was not so on the first run: [what the machine found](#what-the-machine-found).

<a id="what-it-found"></a>
## What it found

Running the test program of the hardware pack ([below](#compared-with-the-rom)) against the GCC builds on the interpreter found four things before any of it went near the machine, and the machine found [two more](#what-the-machine-found):

* **Two bugs in `cmunge`'s veneers**, both fixed (the list above): `error_BAD_SWI` came back as an error block at address &FFFFFFFF, and `configure_BAD_OPTION` as success. Both are in 16.2.0-14 and 16.2.0-15, and show only for a module that returns these values (the header of those releases did not define `error_BAD_SWI`, so such a module had to write `(_kernel_oserror *) -1` itself; the OS's own modules could not be compiled with them).
* **A bug in the source of the OS's Squash module** (`s/comp_ass`, the fast compression): `Squash_Compress` with an input of length 0, no flag bits and room in the output (`3 + 3 * length / 2 <= R5`: what the module takes for the fast case) takes the first byte of the input and subtracts one from the length *before* it looks at it. The length becomes &FFFFFFFF, and the loop that follows compresses four billion bytes and writes them behind the output buffer. In the interpreter the call never returned. It is in the source of the module that is in the ROM as well; no caller is likely to ask for it (compressing nothing), and the restartable code (`R0` bit 1 set) handles an empty input and gives no output at all. The test program therefore never sends an empty input to the fast code. A guard in `Squash_swi` (`r->r[3] > 0` in the condition for the fast case) would be enough; this was reported to RISC OS Open on 8 October 2026 (the report is in [UPSTREAM.md](UPSTREAM.md#for-risc-os-open-ltd)).
* **DrawFile's answer to a SWI that it does not have is an error about a missing message**, in the ROM as well as in the GCC build: `Message token BadSwi not found` (&AC2). The source (`c/main`, `main_error_lookup (0x1E8, "BadSwi", "DrawFile")`) looks the token up, and DrawFile's `Messages` file has no `BadSwi` entry. A small fault: a caller gets that text instead of "SWI value out of range for module DrawFile". Not reported.
* **Squash's fast and restartable compression give different streams** for an input long enough to fill the code table (24000 bytes of mixed data: 19169 bytes from the fast code, 17078 from the restartable code), both valid: `gzip -dc` (the format is that of `compress -b 12`) gives the input back from both, and both decompress. Only the streams of the fast code and of the restartable code differ; the restartable code gives the same stream however the data is cut into pieces.

## What the machine found

Two differences, neither of them a fault of the compiler or of the kit, and the first of them one that the interpreter could not show:

* **The first run of the pack: 7 lines differed**, all in the output of `*Help Render` and `*Render`: with the GCC build of DrawFile loaded the OS said "Message token RenderHelp not found". The cause was in `tools/build-os-modules.py`, the program that makes the module images: it put the module's `Messages` file into the resource file of the module, where the OS build appends `CmdHelp` (the help and syntax texts of the star commands) to it, as the `resources-` rule of the OS's `CModule` makefile does unless `CMDHELP=None`. A module built as a RAM module registers its own copy of `Resources:$.Resources.<Name>.Messages` with ResourceFS, which hides the ROM's file, so the tokens `RenderHelp` and `RenderSyntax` were not found. The same program ignored `ifeq` / `else` / `endif` in the Makefiles (MimeMap was built with `-DNO_INTERNATIONAL_HELP` and the help texts written into the module; CDFSSoftSCSI with the debug libraries). Both are fixed (the survey links 29 of the 66 with the fix, 28 before). The interpreter did not see it: its model of MessageTrans reads the real Messages files of the sources. The file as ResourceFS holds it (a copy of it, the size that `MessageTrans_FileInfo` gives, `OpenFile` and `Lookup` in four ways) showed it in two small runs, and the program prints now, for each module, the messages file that its header names and whether MessageTrans can open it (the `modmsg` lines).
* **The second run: one line differed**, a size: `MessageTrans_FileInfo` of MimeMap's messages file says 217 (a file of 213 bytes) in the ROM and 390 (386 bytes) for the GCC build. The ROM's file is **tokenised**: the OS build (`Sources/Internat/Messages`: "Tokenise the Messages file using Help and Dictionary Tokens"; a messages file with a `#{DictTokens}` line takes part in it) replaces words by dictionary tokens, an escape (27) and a code byte, and the OS expands them when MessageTrans copies the text into a buffer and when `*Help` prints it (a lookup that asks for a pointer into the file gets the tokens as they are). The 213 bytes decode, with a consistent dictionary of the 21 tokens that occur in the file (`ESC &C1` is `type`, `ESC &B9` is ` ma`, `ESC &0C` is ` file`, `ESC &36` is ` re` ...), to exactly the 332 bytes of text of `Messages` and `CmdHelp`; the 386 bytes of the two plain files are those 332 plus their three `#{...}` lines (54 bytes), which the tokeniser takes out. A RAM module keeps the plain files. The same text in two sizes: `*Help MimeMap`, `*Help ReadMimeMap` and the error texts are the same in the result files. The program reports the size on a line that starts with `#` now, which is not compared, and the third run of the pack gave **9,820 lines compared, 0 differ**.

## What it does not show

That a module built with GCC works in general: three were compared with the ROM's, on one machine, and the other 26 were not run on a machine. That the OS can be built this way: the ROM is built from these same sources with the Norcroft tools, and 37 of the 66 do not build here. That the kit is complete: it is a C library of about 275 functions for freestanding modules (floating point included since 16.2.0-17), and the table above says which parts of the OS need more than that (the Wimp C library, the Shared C Library's start-up).
