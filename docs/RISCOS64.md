# RISC OS 64 (AArch64): tests, the module kit and the SDK on Pyromaniac

**Status: experimental, started 15 October 2026.** The tool chain of this repository makes 32-bit code (`arm-riscos-gnueabihf`). RISC OS 64 (AArch64) is being developed by others; this page is about the tests written for it
and what they found. **Everything here has been run on** [RISC OS Pyromaniac](https://pyromaniac.riscos.online/), an emulation of RISC OS, through the hosted [RISC OS Build service](https://build.riscos.online/). Writing modules
with the kit is described in [MODULES64.md](MODULES64.md).

## What is in [`riscos64/`](../riscos64)

* a freestanding AArch64 tool chain recipe (`tools/build-a64-toolchain.sh`: GCC 16.2.0 and binutils 2.45.1 for `aarch64-none-elf`, C and C++);
* a start-up file, a SWI veneer and a tiny runtime written for the purpose, a header for an **AIF64** absolute and a **64-bit module**, written from the formats in the README of
  [gerph/riscos64-clib](https://github.com/gerph/riscos64-clib) (the repository declares no licence; **no code of it is used**, only the formats and the SWI interface: `SVC #0`, the SWI number in `x10`, the X bit `0x20000`,
  arguments `x0`-`x9`, the V flag and an error block in `x0` for an error);
* tests: `t01hello` (start, print, exit), `t02swi` (SWIs, the environment, the error return, the data model), `t03int` (64-bit integer arithmetic), `t04fp` (hardware floating point), `t05file` (the filing system), a module
  `Mod64` with `t07mod` (its command and its SWIs), and the module tests of 0.4 below. `t03int` and `t04fp` print results that are compared with the output of the same sources built for the host (`expected/`);
* checkers for the headers (`tools/check_aif64.py`, `check_mod64.py`) and a runner for the build service (`tools/run_on_service.py`, `ws_build.py`);
* `sdk/` (`aarch64-riscos-gcc`, programs with picolibc), `kit/` (the module kit: `-mmodule`, `cmunge64`, the C library for modules, C++), `libtest/` (the C library's test as a program and as a module): see their READMEs.

## Results on Pyromaniac (15 October 2026)

| Test | Result |
|---|---|
| `t01hello` | starts, prints, exits with return code 0 |
| `t02swi` | 12 checks, none failed |
| `t03int` | 20 checks, none failed; the output is identical to the host's |
| `t04fp` | 20 checks, none failed; the output is identical to the host's |
| `t05file` | 14 checks, none failed (create, write, read back, catalogue information, delete, with 64-bit pointers in the registers) |
| `Mod64` and `t07mod` | the module loads, initialises, answers `*Mod64_Hello` and `*Help Mod64`; its SWI works (see below) |

The complete output is in [`riscos64/results-pyromaniac/`](../riscos64/results-pyromaniac). What it showed: an AIF64 absolute built to the README's layout is entered at `&8100` and runs; the RAM limit that `OS_GetEnv` gave was
`&C08000` (12 MB); an unknown SWI returns with the V flag set and the error `&1E6`; on Pyromaniac **the register block that a module's SWI handler receives has slots of 32 bits** (the SWI that reads 32-bit slots gives the right sum, the one that reads
64-bit slots does not). A module written for one slot width reads wrong values on a kernel that uses the other: the kit has a switch (`-DMODKIT_SLOT=8` for 64-bit slots).

### Module test 0.4: what a C module needs from its own memory

The first module tests covered the module *interface* (the header, the entries, the register block, errors). A module written in C also needs its *memory* to work: `.data`, a zero-initialised area, tables of pointers, and the way the kernel calls its service entry. Test 0.4 asks these:

* **`Mod64D`** (`riscos64/src/mod64d_hdr.S`, `mod64d.c`, `mod64d.ld`; built with `-fPIC -fvisibility=hidden -mgeneral-regs-only -ffixed-x18`, linked at address 4): the flags word `&14` and a zero-initialisation size; **the kernel allocates the zero-initialised area right after the file and it is zeroed and writable**; `.data` is writable and keeps its value; 16 pointers in `.data.rel`, `.data.rel.ro` and a chain are **relocated by the module itself** when it starts (`mod_reloc`: a table of image offsets that `riscos64/tools/modreloc64.py` writes, the same scheme as the 32-bit kit's `modreloc`, with `R_AARCH64_ABS64` words); what `x12` and the stack pointer are; an error block returned by a SWI or a command is **copied into the RMA by the module's veneer**, so a static block in the module works for a program; a service call table in the form that CMunge writes (a `RET` at the entry, the fast entry after it). `tools/check_mod64d.py` checks the header, the tables and the relocation table before the module is used.
* **`Mod64S`**: a plain service entry with no table, to find out which of the two forms the kernel honours.
* **`Mod64F`** (separate, run last): uses the floating point and vector registers at EL1 and sets the FPCR rounding mode; the test program compares its 32 vector registers and the FPCR before and after. GCC for AArch64 has **no soft float** (`-mgeneral-regs-only` refuses any floating point type), so whether a module may use floating point at all, and whether the kernel keeps the caller's registers, decides how floating point can work in modules.

An independent read of the code found no crash bug, and four assumptions that could fail; the tests guard them (`Mod64D` makes its SWIs from module code with its own `swi_x` that checks that `x19`-`x28` survive the `svc` and puts them back; the write phase of the zero-initialised-area test is skipped unless the area was zero; `Mod64F` saves and restores `v8`-`v15`; the small modules are linked at 4 so that their data is aligned at run time).

The tests also ask three more things that a kit needs (`*RMReInit` is a separate step: `t08data prep` and `t08data reinit`): **what the kernel gives a command** (a command `Mod64D_Args` with 0 to 3 parameters, run with none, two, extra spaces, a quoted parameter and one too many), **the RMA as a module's `malloc` sees it** (the address, its alignment, the word before a block, `OS_Module 13`), and **what `*RMReInit` does** (is `.data` kept, is the zero-initialised area zeroed again, are the pointers still right). The command table taught us something too: the information word of a command is `min | GSTrans map << 8 | max << 16 | flags << 24`; the first table had the maximum in the wrong byte and Pyromaniac refused every parameter with its own syntax error, which the test showed at once.

On Pyromaniac everything passes (`Mod64D` 56 checks, `Mod64F` 6, the `*RMReInit` step 8): the argument string starts at the first parameter (leading spaces removed, NUL terminated, a quoted parameter counts as one) and a command with too many parameters gets the kernel's own error, readable by the program; **RMA blocks are only 4-byte aligned** (the address is 4 mod 8: a 16-byte aligned `malloc` for AArch64 needs its own alignment layer, as the 32-bit kit has for 8), with the size word before the block; `OS_Module 13` moves the block and keeps its contents; **`*RMReInit` keeps `.data` and does not zero the zero-initialised area again** (so the start-up code of a C module must clear its own `.bss`), and the relocator does not add the delta twice. The rest: the module relocates itself (delta `&9004000`), the zero-initialised area is right after the image, zeroed and writable, errors are readable. Pyromaniac **honours the table** only when the service entry starts with `RET` (as in the 64-bit modules of riscos64-clib; with another first instruction it calls the entry for every service): it calls the fast entry with `x1` = the index in the table (1) and the other registers as the caller gave them (32-bit values); a plain entry gets every service with the number in `x1`, as in 32-bit RISC OS. A module's floating point use clobbers all 32 vector registers and the FPCR there (an emulator saves nothing); an ordinary SWI keeps them. Output: [`riscos64/results-pyromaniac/mod64d.txt`](../riscos64/results-pyromaniac/mod64d.txt), [`mod64f.txt`](../riscos64/results-pyromaniac/mod64f.txt), [`mod64r.txt`](../riscos64/results-pyromaniac/mod64r.txt).

## The kit's C library on AArch64 (16 October 2026, Pyromaniac)

The test of the kit's own C library (`modkit/tests/libtest/libtest.c`: 39 sections, each prints `SECTION name n=<records> hash=<hash of every result>`, and the hash must be the one that glibc gives for the same inputs, or, for the few sections with no glibc equivalent, the one of the host build of the library) was built for AArch64 as an AIF64 program and run on Pyromaniac: [`riscos64/libtest/`](../riscos64/libtest). **The 129 library sources are compiled unchanged** (GCC 16.2, `-O2 -ffixed-x18 -mstrict-align -ffp-contract=off`, the kit's own headers, no warning with `-Wall -Wextra`), with the same `mk_` names and the same `-DMODLIB_HOST` switch as the host build, so the library's SWI calls go to three small hooks; for AArch64 they are `a64hooks.c` and `a64swi.S` (the library's block of ten 32-bit register slots into `x0`-`x9`, `svc #0`, the flags and the error block back). picolibc only supplies what the host build takes from the host (`malloc`, `printf`, `puts`, `abort`).

| sections | result on Pyromaniac |
|---|---|
| 28: ctype string numbers wchar sort div sscanf printf time limits inttypes signal g1 fnmatch getline complex iconv g3 c99 gamma fparith fpconv gccrt fpprintf strtod fpscanf fpmathx fpmathsp | the same count and hash as **glibc** (getline and g3 work on files) |
| 7: ldouble fpmath rand swix clock stdio3 tmpfile | the same as the host build of the library (`swix` is the `_swi`/`_swix` marshalling of the register block) |
| 4: fd stdio stdio2 probe | not the same: see below |

One run of the 35 that agree took 93 seconds on the build service ([`results-pyromaniac/libtest64.txt`](../riscos64/results-pyromaniac/libtest64.txt)). Two things had to change. **`-ffp-contract=off`**: GCC for AArch64 fuses `a*b+c` into one instruction by default, and fdlibm's results then differ in the last bit (`fpmath` had another hash until the library was built without it). And **`modkit/include/inttypes.h`**: the format macros for 64-bit types and for `intptr_t` ("lld", "d") were those of the 32-bit target; they now follow the size of `long` and of a pointer (the 32-bit expansion is the same as before, and the host run still equals glibc). Counted from this build: 26 conversions of a pointer to or from a 32-bit integer in 8 of the library files (the SWI register block of the library has 32-bit slots, and a program's addresses are below 4 GB, so they work here), and ARM inline assembler in 5 C files (`heap`, `exit`, `scr`, `errext`, `sclcompat`) and 3 assembler files (`kirq`, `modswi`, `setjmp`): that is what a port of the library to modules has left to do.

**The four sections that differ stop at a call where Pyromaniac's file system is not RISC OS's.** The reference for what RISC OS does is the probe of 7 October on a Raspberry Pi (RISC OS 5.30, `modkit/tests/libtest/pi-probe.txt`); [`riscos64/libtest/fsdiff.c`](../riscos64/libtest/fsdiff.c) shows each difference in a few lines of SWIs ([`results-pyromaniac/fsdiff.txt`](../riscos64/results-pyromaniac/fsdiff.txt), [`fsdiff3.txt`](../riscos64/results-pyromaniac/fsdiff3.txt)):

1. `OS_Find` &CF (update, bit 3: an error if the file is not there) of a file that is not there gives a handle and makes the file (RISC OS: error &D6; &4F, for input, gives the error as it should). `stdio` differs from its case 7 on (`fopen` of a missing file with "r+b" succeeds); `stdio2` fails its checks of a missing file, leaks the handle and then writes to a NULL stream.
2. `OS_Args` 3 (set the extent) on a stream open for output or update: the next `OS_Args` 2 gives 0 (RISC OS: the new extent; the file does have the new length once it is closed). `fd` differs from its case 5 on (after an `ftruncate` the file reads as empty).
3. `OS_Args` 1 (set the pointer) beyond the end of a stream open for update does not make the file longer (RISC OS: longer, with zeros).
4. `OS_Args` 1 beyond the end of a stream open for reading is accepted (RISC OS: error &B7 "Outside file").
5. `OS_Args` 3 on a stream open for reading **stops the emulator** ("IOError: File not open for writing", return code 125); RISC OS: error &C1 "Not open for update". This is where `probe` stops.
6. A locked file (attributes &0B) can be renamed and deleted (RISC OS: error &C3, "locked"), and it can be opened for output or update and written: opening it for output empties it (RISC OS: a stream that cannot be written, status &140 or &40, the write gives &C1, the file is not changed).
7. A read-only file (attributes &01) cannot be deleted (error &BD), and `OS_Find` &8F and &CF of it give &BD (RISC OS: it can be deleted, and `OS_Find` gives a stream that cannot be written, as above). A file with attributes &00 is refused in the same way; that was not tried on RISC OS.

So the four sections say nothing about the AArch64 library while Pyromaniac's file system is what they run on; on the host the same sources, with a model of FileSwitch made from the Pi's answers, give glibc's hash for `fd` and `stdio` and no failed check in `stdio2` and `probe`. The 35 that agree do not use those calls, or use them in the ways in which the two file systems agree. What this does not show: the file sections (above), and anything beyond what Pyromaniac does: the register block of the library has 32-bit slots and the addresses given to SWIs are cut to 32 bits.

### The same library inside a module (16 October 2026)

The test runs inside a 64-bit module as well: `riscos64/libtest/build-libtestmod.sh` links the 129 library sources, `libtest.c` and the kit's runtime into **LibTest64** (333 KB, 693 relocated pointers; its header is made by `cmunge64` from `libtestmod.cmhg`), and `*LibTest64` runs the sections named in `LT_ONLY` on a 384 KB stack of its own (the module's stack is the 32 KB SVC stack). On Pyromaniac the same **35 sections** give the same hashes as in the program ([`results-pyromaniac/libtestmod.txt`](../riscos64/results-pyromaniac/libtestmod.txt)): 28 as glibc, 7 as the host build, no failed check. The library needed AArch64 branches in three files, which are in `modkit/lib` (`heap.c`: 16-byte aligned blocks of the RMA through the SWI hook; `exit.c`: `exit` and `abort` stop with an error through `__modlib_raise`; `setjmp64.S` and a larger `jmp_buf`); the 32-bit build of those files is unchanged.  Floating point is used in this module (the maths needs it), and works on Pyromaniac.

## Thanks

Thanks to **Gerph**: the formats of RISC OS 64 are written down in his [riscos64-clib](https://github.com/gerph/riscos64-clib), and **RISC OS Pyromaniac** and the hosted
[RISC OS Build service](https://build.riscos.online/) are his work too. Without a service that runs 64-bit RISC OS binaries in the open, these tests could not have been run at all.

## Running the tests

On a machine with the tool chain: `riscos64/build.sh` (needs `~/a64/prefix`, made by `tools/build-a64-toolchain.sh`), then `tools/run_on_service.py out` runs every test on the build service (it is Gerph's service, which he has said publicly is open to everyone without limits; the six first tests take about
three minutes). `tools/check_aif64.py out/*,ff8` and `tools/check_mod64.py out/Mod64,ffa` check the files without running them. The jobs for the module kit, the library test and the SDK are in the same script (`--only NAME`).
