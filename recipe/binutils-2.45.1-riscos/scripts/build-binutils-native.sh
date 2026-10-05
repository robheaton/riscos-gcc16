#!/bin/bash
# Binutils 2.45.1 (the RISC OS port) cross-built FOR RISC OS: native as, ld, ar, ... for a RISC OS-hosted GCC.
# usage: build-binutils-native.sh [SRC=$HOME/gccsdk-next/src/binutils-2.45.1] [BUILD=$HOME/gccsdk-next/build-binutils-native] [PREFIX=$HOME/gccsdk-next/binutils-native-install] [CROSS=$HOME/gccsdk-next/env-f]
set -eu
SRC=${1:-$HOME/gccsdk-next/src/binutils-2.45.1}
B=${2:-$HOME/gccsdk-next/build-binutils-native}
P=${3:-$HOME/gccsdk-next/binutils-native-install}
E=${4:-$HOME/gccsdk-next/env-f}
export PATH=$E/bin:/usr/bin:/bin:/usr/local/bin
T=arm-riscos-gnueabihf
export ac_cv_func_shl_load=no ac_cv_lib_dld_shl_load=no ac_cv_func_dlopen=yes
# UnixLib exports qsort_r but its <stdlib.h> does not declare it: libctf would call it implicitly (an error since GCC 14); let libctf use its own
export ac_cv_func_qsort_r=no
mkdir -p "$B"; cd "$B"
# riscos-da.o: the heap of every program in a dynamic area (the GCC recipe's data/riscos-da.c says why); riscos-da-big.o: the big (512 MB) maximum for it, which the binutils
# programs (as, ld, objcopy, strip ... can need a lot of memory; unlike the drivers they are leaves of the process chain) get; both linked into all of them through LDFLAGS
GD="$(cd "$(dirname "$0")/../.." && pwd)/gcc-16.2.0-riscos/data"
"$E/bin/$T-gcc" -O2 -c "$GD/riscos-da.c" -o "$B/riscos-da.o"
"$E/bin/$T-gcc" -O2 -c "$GD/riscos-da-big.c" -o "$B/riscos-da-big.o"
"$SRC/configure" --build=x86_64-pc-linux-gnu --host=$T --target=$T --prefix="$P" \
  --disable-nls --disable-werror --disable-gdb --disable-gprofng --disable-gprof --disable-sim --disable-gold --disable-plugins \
  --without-zstd --without-debuginfod --disable-multilib --disable-shared --enable-static \
  CFLAGS="-O2" CXXFLAGS="-O2" LDFLAGS="$B/riscos-da.o $B/riscos-da-big.o -static-libgcc -Wl,--allow-shlib-undefined" > configure.log 2>&1
make -j"${JOBS:-8}" MAKEINFO=true all-binutils all-gas all-ld > make.log 2>&1
make MAKEINFO=true install-binutils install-gas install-ld > install.log 2>&1
ls -la "$P/bin" "$P/$T/bin" 2>/dev/null | head -40
