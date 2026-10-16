# libtest64: the library test of the 32-bit kit, built for AArch64 and run on Pyromaniac

`modkit/tests/libtest/libtest.c` is the test of the kit's own C library (`modkit/lib`, 129 sources used here): 39 sections, each prints `SECTION name n=<records> hash=<FNV of every result>`, and the
hash must be the same as the one that glibc gives for the same inputs (the "oracle"). On the 32-bit side it runs on the host (the library compiled for x86-64, with ASan and UBSan), on an interpreter and on a
Raspberry Pi. This directory builds it for **AArch64** as an **AIF64 program** (`libtest64,ff8`) with the SDK of `../sdk/` and runs it on RISC OS Pyromaniac through the build service.

What is compiled, and how:

* the library sources, **unchanged**, with `aarch64-none-elf-gcc -O2 -ffp-contract=off -ffixed-x18 -mstrict-align -DMODLIB_HOST` and the kit's own headers (`-nostdinc -I modkit/include`); every name is renamed `mk_NAME`
  (`mkrename.py`'s `mk_rename.h`), as the host build does, so that the test calls the library and picolibc's own functions are not in the way. `-DMODLIB_HOST` is the host build's switch: the library then asks
  the three hooks (`__modlib_xswi`, `mk_ptr_in`, `mk_host_*`) for the SWIs instead of using ARM inline assembler;
* `libtest.c` itself, against the kit's headers, `-DT_HOSTLIB -Dmain=libtest_main`;
* `a64hooks.c` (the hooks: the same models of the clock, the screen and the keyboard as `hosthooks.c`, `OS_ReadVarVal` for the test's own variables `LT_ONLY` and `LT_VERBOSE`, and, with `SWIS=1`, every other SWI goes
  to the kernel through `a64swi.S`: the library's 32-bit register block into `x0`-`x9`, `svc #0` with the number in `x10`, the flags and the error block back), `a64main.c` (`-only=a,b,c` and `-v`);
* the SDK's AIF64 header, start-up and system calls, and picolibc for the names that the host build takes from the host (`malloc`, `free`, `printf`, `puts`, `abort` ...).

`./build-libtest64.sh` (3 seconds with 8 cores) builds `../out/libtest64/libtest64,ff8`; `SWIS=1 ./build-libtest64.sh` is the build that uses the kernel's SWIs (the files). `LT64_SECTIONS=a,b,c SERVICE_TIMEOUT=300
python3 ../tools/run_on_service.py ../out/libtest64 --only lt64-a lt64-b lt64-c` runs one job on the build service for each section; `compare.py ORACLE.out HOSTLIB.out ../out/libtest64/service-results/lt64-*.txt` compares
the `SECTION` lines (`ORACLE.out` and `HOSTLIB.out` are made by `modkit/tests/libtest/run-host.sh`).

Results on Pyromaniac (16 October 2026, `../results-pyromaniac/libtest64.txt`): 35 of the 39 sections give the hash of glibc (28) or of the host build (7); `fd`, `stdio`, `stdio2` and `probe` differ or stop at calls where Pyromaniac's
file system is not RISC OS's. `fsdiff.c` (built with `../sdk/bin/aarch64-riscos-gcc`, run with `tools/run_on_service.py`, jobs `fsdiff` and `fsdiff3`) shows each of those differences in a few SWIs. The details are in the section
"The kit's C library on AArch64" of [`docs/RISCOS64.md`](../../docs/RISCOS64.md).

**LibTest64, the same test as a module** (`build-libtestmod.sh`, `libtestmod.cmhg`, `libtestmod.c`, `libtestmod.S`): the library, `libtest.c` and the kit's runtime in one 64-bit module; `Set LT_ONLY ctype,string`, `RMLoad LibTest64`, `LibTest64`. 35 sections agree on Pyromaniac (`../results-pyromaniac/libtestmod.txt`).
