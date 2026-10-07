# The C modules of the RISC OS Open sources, built with modkit

> **Not in a release yet.** This describes work after 16.2.0-15: the changes to the kit that it led to are in the `modkit/` sources of this repository and in the next release. Nothing here was run on the machine: see [what was checked](#what-was-checked).

[modkit](MODULES.md) builds relocatable modules with `gcc -mmodule` and `cmunge`, and so far it had been tested with small modules written for it. The RISC OS Open sources (the BCM2835 subset) have **66 components that build C modules**, written for the Norcroft C compiler and the Shared C Library, 184,000 lines of C in all. They are a test that the kit's authors did not write, so the kit was held against them: **each of the 66 was built the way the OS build builds it, with the kit in place of the Norcroft tools and the Shared C Library.**

**The result: 28 of the 66 build and link into a module image, with no change to their sources.** For 41 of them every C and assembler file compiles; for the 38 that do not link the table [below](#what-stops-the-others) says what stops each one, and it is not, with a few exceptions, the kit.

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

## The 28 that build and link

| Module | Bytes | What it is |
|---|---:|---|
| `Audio/SoundCtrl` | 14,944 | the sound mixer's control module |
| `HWSupport/GPIO` | 9,324 | GPIO devices on the HAL |
| `HWSupport/PortMan` | 9,784 | the port manager |
| `HWSupport/RTC` | 9,552 | the real time clock device |
| `HWSupport/SD/SDIODriver/Test/FakeCardInt` | 23,120 | a test module of the SD driver |
| `Networking/AUN/Access/Freeway` | 25,888 | Freeway, the service directory |
| `Networking/AUN/Net` | 42,292 | the Net module (Econet over IP) |
| `Networking/DHCP` | 31,332 | the DHCP client |
| `Networking/MimeMap` | 22,288 | the MIME map |
| `Networking/Omni/Protocols/OmniLanManFS` | 139,368 | the LanManager file system |
| `Programmer/Squash` | 12,368 | compression (with its assembler) |
| `Toolbox/Toolbox`, and `ColourDbox`, `ColourMenu`, `DCS`, `FileInfo`, `FontDbox`, `FontMenu`, `IconBar`, `Menu`, `PrintDbox`, `ProgInfo`, `SaveAs`, `Scale` | 16,000 - 27,400 each | the Toolbox and the 12 object modules that use the Toolbox library |
| `Video/Render/DrawFile` | 37,264 | Draw file rendering |
| `Video/UserI/BootFX` | 19,020 | the boot bar |
| `Video/UserI/ScrModes` | 48,596 | screen modes and the monitor files |
| `Video/UserI/ScrSaver` | 5,748 | the screen saver service |

Together 724,000 bytes. Each is a module image with the header words, the command table, the SWI chunk and the names that its CMHG file says, and a relocation table that `tools/modinfo.py` checks (the table ends the file; every word it lists holds an address inside the image).

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
| 1 | the Desk-based debug libraries (PDebug, Trace) | CDFSSoftSCSI |
| 1 | SyncLib's assembler, where `asasm` rejects an objasm macro rule | EtherGENET |
| 1 | the VFP assembler of VFPSupport (`asasm` has no VFP syntax) | VFPSupport |
| 1 | floating point (`math.h`) | MakePSFont |
| 1 each | headers that are not in the sources (libpng, VideoCore, the Linux SDIO headers), `uchar.h` (included by a test of the Shared C Library), an include path of the RISC OS folder layout (AbortTrap), a RISC OS dotted include name (Window), an assembler header that is not found (BCMVideo), code that the Makefile builds another way (Debugger, `longcmd`), and SDCMOS, which has no object to build | CompressPNG, VCHIQ, FakeLibInt, `atomic`, AbortTrap, Window, BCMVideo, Debugger, `longcmd`, SDCMOS |

So of the 38: **5** are stopped by CMHG features that the kit leaves out on purpose; **13** by source that only the Norcroft tools read (inline assembler, run-time symbols of the Shared C Library, C that GCC 16 rejects, the VFP assembler, include paths of the RISC OS folder layout); **16** by libraries, headers and constants of the OS that this program does not provide (the USB stack's headers, the Wimp C library, libpng, the VideoCore and SDIO headers, SyncLib's assembler, two constants that the exported headers lack ...); **2** are limits of the kit's own `mkoslib` (Picker and BootCmds: see [below](#what-the-survey-changed-in-the-kit)); one needs `uchar.h`, which only a test of the Shared C Library includes; and `SDCMOS` has nothing in its Makefile to build.

## What was checked

* **Built**: every module above compiles, assembles and links; the module is made by the driver's `modreloc` step. 17 of the 28 also run their initialisation and finalisation code in the interpreter's model of RISC OS (`modkit/tests/sim-osmodules.py`), which finds a crash in a module's start-up code (a bad relocation, a stack that is not 8 byte aligned); the other 11 stop where the model has no answer (the HAL, dynamic areas, `MessageTrans`, a network station that is not configured: Net says "NotConf", as it should), and that says nothing about the modules.
* **Compared with the real CMunge**: the CMHG files of the 61 components that have one were made into module headers by `cmunge` and by GCCSDK's CMunge and the decoded headers compared (`modkit/tests/os-cmhg-corpus.py --real`): `cmunge` accepts 55 of the 61 and the headers are the same wherever both accept a file (a 56th is the same but for the date that CMunge adds when a file has no `date-string`); the other 5 are the ones in the first two rows of the table above.
* **Not run on the machine.** Loading a module that replaces one of the ROM's is not something to do without a reason, and a module that builds is not a module that works. A hardware pack that runs a few of them against the ROM versions (`MimeMap`, `DrawFile`, `Squash` with the same inputs and the outputs compared) is the next step.

## What the survey changed in the kit

Building real modules found these, all of them fixed in `modkit/` (the tests that cover each are named in [TESTING.md](TESTING.md)):

* **`cmunge`**: the numbers of a CMHG file after the C preprocessor are constant expressions (the OS's headers turn `Service_ResourceFSStarting` into `(0x60)`); the generated header now defines what CMunge's does and OS sources use (`<prefix>_00`, the `X` names of the SWIs, `error_BAD_SWI`, `arg_CONFIGURE_SYNTAX`, `arg_STATUS`, `configure_BAD_OPTION` ...); **`event-handler:`** is supported (a vector veneer that passes every event that is not in its list on at once); a handler name with options after it, such as `(flags-capable:)`, is an error instead of a bad symbol in the assembler source.
* **`<kernel.h>`**: `size_t`, `_kernel_irqs_on`, `_kernel_irqs_off`, `_kernel_irqs_disabled`, `_kernel_processor_mode`, `_kernel_RMAalloc`, `_kernel_RMAextend`, `_kernel_RMAfree` and the type `_kernel_stack_chunk`: 3 of the 28 modules call the first three (PortMan, Net, LanManFS; 7 of the 66 do).
* **`<swis.h>` has the SWI numbers** (951 SWIs and their `X` names, from the OS's own assembler headers: `include/swisnums.h`, made by `bin/mkswis.py`), `XOS_Bit`, and `_vswi` / `_vswix`. The Shared C Library's header has them; the kit's did not.
* **New headers**: `<inttypes.h>` (the `PRI` and `SCN` macros) and `<signal.h>` (`signal` and `raise`, so far as a module can use them).
* **`<string.h>` and `<ctype.h>` declare the functions that are not ISO C** (`strdup`, `stricmp`, `strlcpy`, `isascii` ...) **only when the program is not strict ISO C** (`-std=c99`, as the OS build uses; `-D_GNU_SOURCE` asks for them): a module that defines its own `stricmp` with another type no longer fails to compile.
* **`module.ld`** defines the symbols the Norcroft linker does (`Image$$RO$$Base` ...), which assembler sources import; **`errno.c`** also defines `__errno`, the name that the TCP/IP libraries' veneers write to.
* **`mkoslib`**: outputs that are "the processor status register" (the flags; 34 more OSLib functions can be made), static libraries (`.a`) given to `--from-objects` (the functions that the library uses are made as far as they can be: a function that cannot be made is a warning), and a function that one of the module's own objects defines is not made again.
* **`modreloc` refuses a module that has sections outside `.image`.** The module linker script places `.text*`, `.rodata*`, `.data*` and `.bss*`; any other section name (an assembler `AREA`, a `__attribute__ ((section ("x")))`) used to be left out of the module without a word, and the module jumped into its own relocation table. It is an error now, and the message says how to rename the section. (The survey found this: DHCP, which called `socket`, ran into the table.)

## What it does not show

That a module built with GCC works: that needs the machine. That the OS can be built this way: the ROM is built from these same sources with the Norcroft tools, and 38 of the 66 do not build here. That the kit is complete: it is a C library of about 170 functions for freestanding modules, and the 38 above say which parts of the OS need more than that (the Wimp C library, floating point, the Shared C Library's start-up).
