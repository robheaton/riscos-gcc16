# The 32-bit RISC OS compiler that runs on RISC OS 64

`build-hosted64.sh WORK` builds GCC 16.2.0 (C and C++) and binutils 2.45.1 for host `aarch64-riscos` (programs for RISC OS 64, built with the 64-bit cross compiler and UnixLib 64) and target `arm-riscos-gnueabihf`
(programs for 32-bit RISC OS), so that a RISC OS 64 machine can make programs for 32-bit machines.  `make-tree32.py` lays the programs out with the 32-bit target's headers and libraries (taken from the 32-bit native compiler's
tree: they are data files) in the flat layout of `../ports/make-native-tree.py`, and `../ports/finish-native-package.py native32` adds the launchers, the sample scripts, zlib, the ReadMe and the licences and makes the zip
(`riscos64-native32-0.1`).  `scripts/RunTo32,feb` is the script of the C and C++ test, `tests/` has its two source files.  What the package is, how to use it and what was tested: [../../docs/NATIVE-ON-RISCOS64.md](../../docs/NATIVE-ON-RISCOS64.md).

* The object files of a C and a C++ program are byte for byte the Linux cross compiler's (the same source name); the linked programs run on a 32-bit machine.  The linker (the 32-bit port of ld, running on RISC OS 64) gives the output the ELF file type &E1F itself.
* `lib/specs` (written by `make-tree32.py`) makes a plain `xgcc -B bin/ -B lib/ -L lib` link work: it passes each -L directory as -rpath-link (libunixlib.so refers to libgcc_s.so.1, and the 32-bit driver's `/SharedLibs:` path does not exist here) and links the shared libgcc.
* `-B bin/ -B lib/ -L lib -isystem include` are needed in the flat layout, with names relative to the folder of the tree; start the driver by its full path, in a big application slot (`WimpSlot -min 128M -max 128M`, 320M for large C++ files), with `UnixEnv$swap` set to 1.
* The driver and `collect2` must be relinked with `-Wl,--defsym=__ul64_heap_max=0x1000000` (a small heap: they only wait for the compiler and the linker), or the second compile fails or the driver crashes (each dynamic area reserves its whole maximum of the shared logical space).
* The programs link the static libstdc++.a of the 32-bit native package (780 KB for a C++ hello world); the Linux cross compiler's default is the shared one.
