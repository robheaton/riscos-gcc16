# aarch64-riscos: a GCC and binutils target for RISC OS 64

Until now the programs and modules of `riscos64/` were made with the freestanding `aarch64-none-elf` compiler inside a wrapper (`sdk/bin/aarch64-riscos-gcc`, a shell script). This folder makes **a real target**:
`aarch64-riscos` is known to binutils 2.45.1 and GCC 16.2.0, so the tools are called `aarch64-riscos-gcc`, `-as`, `-ld`, `-objcopy` ... and behave as the compiler of any other target.

| File | |
|---|---|
| `build-target.sh BINUTILS-SRC GCC-SRC PREFIX WORKDIR` | patches copies of the two source trees, builds binutils, GCC (C and C++) and libgcc, and installs the SDK's start-up files, linker script, system calls and picolibc into `PREFIX/aarch64-riscos` |
| `patches/binutils-2.45.1-aarch64-riscos.patch` | the target for BFD, gas and ld (an emulation `aarch64riscos`, a copy of `aarch64elf` for now) |
| `patches/gcc-16.2.0-aarch64-riscos.patch` | `config.gcc` and `libgcc/config.host` |
| `new-files/gcc/config/aarch64/riscos.h`, `riscos.opt`, `riscos.opt.urls` | the target description of GCC |

## What the target does

* `__riscos__` and `__riscos64__` are defined; `-ffixed-x18` and `-mstrict-align` are the defaults (x18 is reserved on RISC OS 64).
* Linking a program: `aarch64-riscos-gcc hello.c -o hello,ff8` links the AIF64 header (`aifhdr.o`), the start-up code (`crt0.o`, `riscos_start.o`), `crtbegin.o`/`crtend.o`, the SDK's system calls (`riscos_syscalls.o`, `swi.o`), picolibc (`-lc -lm`) and libgcc with the linker script `aif64.ld`, and then runs `objcopy -O binary` on the result (the AArch64 linker cannot write a raw image itself). The image is the AIF64 file: it is exactly the read-only plus
  the read/write size of its header. Name it `,ff8` yourself (the file type of an absolute); the wrapper did that for you.
* The C library and the SDK files are in `PREFIX/aarch64-riscos/{include,lib}` and are found without any option.

* **Modules:** `aarch64-riscos-gcc -mmodule` (and `-mmodule-fp`, with the floating point and vector registers allowed) compiles with the options of `kit/modflags.sh` (no library, no GOT, `-mgeneral-regs-only`; C++ without
  exceptions and RTTI), finds the kit's headers (`PREFIX/aarch64-riscos/modkit`), links the kit's runtime and C library with `module64.ld`, and runs `modreloc64` on the result: `aarch64-riscos-gcc -mmodule -O2 hello64.o
  hello64_hdr.o -o Hello64,ffa`. `kit/module64.mk` works with it (`CC64` and `CMUNGE64_CC=aarch64-riscos-gcc`; `cmunge64` then assembles the module header with it). The kit library (`libmodkit64.a`, the runtime) and the SDK's start-up files and system calls are built by `build-target.sh` with the new compiler (`kit/build-kit.sh` with `KITCC`, `KITAR`, `KITOUT`);
  they are byte for byte the same objects as the older tool chain makes. `build-target.sh` also installs `cmunge64` and `mkoslib` in `PREFIX/bin` and the kit's tools in `PREFIX/share/riscos64` (the make rules are
  `PREFIX/share/riscos64/module64.mk`).

* **C++ for programs:** `aarch64-riscos-g++` links libstdc++ and libsupc++ (built by `build-target.sh` with the new compiler, in `PREFIX/aarch64-riscos/lib`, headers in `include/c++`): containers, strings, iostream, exceptions,
  static constructors and destructors, `atexit`. No threads, no `std::filesystem`, no locales but "C" (as with the SDK's wrapper).

`build-target.sh` is the installer of this target (`sdk/` + `install-kit64.sh` still install the wrapper).

Not done yet: the `.ld`-level AIF64 output (no `objcopy`: the linker would have to write the image itself), debug information for RISC OS, a `--sysroot` layout (everything is in `PREFIX/aarch64-riscos`, which GCC
finds without options), and tests beyond the examples.

Tested on RISC OS Pyromaniac (17 October 2026): `sdk/tests/hello.c` and `sdk/tests/sdktest.c` (20 of 20 checks) compiled and linked with `aarch64-riscos-gcc` from this target run as the wrapper's do; the modules `Hello64` (8 of 8),
`Cxx64` (C++, 8 of 8) and `Fpmod64` (floating point, 5 of 5) built with `-mmodule` / `-mmodule-fp` pass the tests of `kit/examples`, and `cxx64` built through `module64.mk` is byte for byte the same. The whole build
(`build-target.sh`, binutils, GCC, libgcc, libstdc++ and the SDK files) takes about 12 minutes on 22 cores; `sdk/tests/cxxtest.cc` built with `aarch64-riscos-g++` gives the same output as the wrapper's build. The wrapper `sdk/bin/aarch64-riscos-gcc` is still the supported way to build until this target has modules and C++.
