# riscos64: tests and tools for RISC OS 64 (AArch64)

Written from the formats in the README of https://github.com/gerph/riscos64-clib (no code of it is used; the repository declares no licence): AIF64 absolutes, 64-bit modules, and the SWI interface
(`SVC #0`, the SWI number in `x10`, X bit `0x20000`, arguments `x0`-`x9`, the V flag for an error with the error block in `x0`).

* `build.sh` builds the tests with the AArch64 tool chain of `tools/build-a64-toolchain.sh` (GCC 16.2.0 and binutils 2.45.1, `aarch64-none-elf`, freestanding), and the host builds that give the expected output.
* `tools/check_aif64.py`, `check_mod64.py` check the headers against the README; `tools/ws_build.py` and `run_on_service.py` run the tests on RISC OS Pyromaniac through the RISC OS Build service
  (`ws://jfpatch.riscos.online/ws`, architecture `aarch64`); the HTTP JSON interface needs a multipart POST.

Findings on Pyromaniac (15 October 2026): an AIF64 absolute is entered at &8100 and runs; the RAM limit of OS_GetEnv was &C08000 for an 12 MB slot; an unknown SWI returns the V flag and error &1E6;
integer and hardware floating point results are identical to the host's; the filing system calls work with 64-bit pointers in the registers; a 64-bit module loads, initialises, answers its command and `Help`,
and **the register block of its SWI handler has slots of 32 bits** (Mod64_Add32 works, Mod64_Add does not).

Thanks to Gerph: the formats are written down in his riscos64-clib, and RISC OS Pyromaniac and the RISC OS Build service, which ran these tests, are his work.

**Programs in C** (16 October 2026): `sdk/` has `aarch64-riscos-gcc`, which makes AIF64 programs with picolibc as the C library: `aarch64-riscos-gcc hello.c -o hello` gives `hello,ff8`. See `sdk/README.md`; `tools/build-picolibc.sh` builds the library. It runs on Pyromaniac.

**The kit's C library on AArch64** (16 October 2026): `libtest/` builds the library test of the 32-bit kit (`modkit/tests/libtest/libtest.c`) for AArch64 as an AIF64 program, with the 129 library sources unchanged, and runs it on Pyromaniac: 35 of its 39 sections give the hash of glibc (28) or of the host build of the library (7); the other four (`fd`, `stdio`, `stdio2`, `probe`) depend on details of FileSwitch where Pyromaniac's file system differs from RISC OS (`libtest/fsdiff.c` shows seven of them). See `libtest/README.md` and the section "The kit's C library on AArch64" of `docs/RISCOS64.md`.


**C modules** (16 October 2026): `kit/` has the first module kit for AArch64: `aarch64-riscos-gcc -mmodule` makes a 64-bit module from C (`kit/README.md`, the example `kit/examples/hello64`). It runs on Pyromaniac (8 checks).

`kit/cmunge64.py` (16 October 2026) writes the header of a 64-bit module from a CMHG file (`kit/examples/hello64c`); see `kit/README.md`.

**Install and use** (16 October 2026): `install-kit64.sh PREFIX` installs `aarch64-riscos-gcc`, `cmunge64`, `modreloc64`, `mkaif64` and `mkoslib` and builds the libraries; `kit/module64.mk` has make rules for a module (`MODULE`, `CMHG`, `SRCS`, `FP`). The guide is [`docs/MODULES64.md`](../docs/MODULES64.md).
