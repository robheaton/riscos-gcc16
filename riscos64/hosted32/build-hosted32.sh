#!/bin/bash
# build-hosted32.sh WORK: builds the AArch64 (RISC OS 64) cross compiler as a set of programs for 32 bit RISC OS (host arm-riscos-gnueabihf, target aarch64-riscos) on Linux, then stages the tree.
# Needs: the cross compiler for 32 bit RISC OS (~/gccsdk-next/env-f), GMP/MPFR/MPC for it (~/gccsdk-next/hostlibs-riscos), the AArch64 tool chain (T64, default /var/tmp/a64/target7-inst) with its work
# directory target7-work (patched GCC and binutils sources), and the recipe's data/ files.  Result: WORK/tree-ro/gcc16a64r  (see stage-tree.sh).  About 25 minutes on 22 cores.
set -eu
W=${1:?WORK}; mkdir -p "$W"; W=$(cd "$W" && pwd)
G=${GCCNEXT:-$HOME/gccsdk-next}; E=$G/env-f; RD=$G/recipe/gcc-16.2.0-riscos/data; H=$G/hostlibs-riscos
T64=${T64:-/var/tmp/a64/target7-inst}; T64W=${T64W:-/var/tmp/a64/target7-work}
HERE=$(cd "$(dirname "$0")" && pwd)
export PATH=$E/bin:/usr/bin:/bin:/usr/local/bin
export ac_cv_func_shl_load=no ac_cv_lib_dld_shl_load=no ac_cv_func_dlopen=yes
cd "$W"
# 1. the sources: the AArch64 target source plus the host side of the 32 bit RISC OS native compiler
rm -rf gcc-src binutils-src; cp -a "$T64W/gcc-src" gcc-src; cp -a "$T64W/binutils-src" binutils-src
( cd gcc-src && patch -p1 -l < "$HERE/patches/gcc-hosted-riscos32.patch" )
( cd binutils-src && patch -p1 < "$HERE/patches/ld-aif64-filetype.patch" )     # ld on 32 bit RISC OS gives the AIF64 output file type FF8
# 2. configure-time tools: configure must run an assembler and a linker for AArch64 to learn what they can do; the Linux-hosted ones do
mkdir -p probe; for t in as ld nm ar ranlib objdump objcopy strip readelf; do ln -sf $T64/aarch64-riscos/bin/$t probe/$t; done
# 3. GCC (all-gcc: the `specs` step cannot run an ARM program here and fails: that is expected, stage-tree.sh makes the specs file)
mkdir -p build-gcc2; cd build-gcc2
$E/bin/arm-riscos-gnueabihf-gcc -O2 -c $RD/riscos-da.c -o riscos-da.o
"$W/gcc-src/configure" --build=x86_64-pc-linux-gnu --host=arm-riscos-gnueabihf --target=aarch64-riscos --prefix=/GCC16a64 --with-build-time-tools="$W/probe" \
  --with-gmp=$H --with-mpfr=$H --with-mpc=$H --enable-languages=c,c++ --without-headers --with-newlib --disable-nls --disable-shared --enable-threads=posix --disable-libssp --disable-libgomp \
  --disable-libquadmath --disable-libatomic --disable-multilib --disable-bootstrap --disable-werror --disable-lto --disable-plugin --disable-libcc1 --disable-libstdcxx-pch \
  --with-default-libstdcxx-abi=new --disable-libstdcxx-filesystem-ts --disable-libstdcxx-backtrace \
  CFLAGS=-O2 CXXFLAGS=-O2 LDFLAGS="-L$RD/nolibm $W/build-gcc2/riscos-da.o -static-libstdc++ -static-libgcc -Wl,--allow-shlib-undefined" > configure.log 2>&1
make MAKEINFO=true configure-gcc > cfg-gcc.log 2>&1
"$G/make-gcc16-native.sh" "$W/build-gcc2" "$E" -k all-gcc > "$W/make-gcc.log" 2>&1 || true
# 4. binutils for the same host and target.  qsort_r: UnixLib exports it but does not declare it (declared by qsortr.h, forced into libctf)
cd "$W"; mkdir -p build-bu; cd build-bu
$E/bin/arm-riscos-gnueabihf-gcc -O2 -c $RD/riscos-da.c -o riscos-da.o; $E/bin/arm-riscos-gnueabihf-gcc -O2 -c $RD/riscos-da-big.c -o riscos-da-big.o
"$W/binutils-src/configure" --build=x86_64-pc-linux-gnu --host=arm-riscos-gnueabihf --target=aarch64-riscos --prefix=/GCC16a64 --disable-nls --disable-werror --disable-gdb --disable-gprofng --disable-gprof --disable-sim \
  --disable-gold --disable-plugins --without-zstd --without-debuginfod --disable-multilib --disable-shared --enable-static CFLAGS=-O2 CXXFLAGS=-O2 \
  LDFLAGS="$W/build-bu/riscos-da.o $W/build-bu/riscos-da-big.o -static-libgcc -Wl,--allow-shlib-undefined" > configure.log 2>&1
printf '#include <stddef.h>\nvoid qsort_r (void *, size_t, size_t, int (*) (const void *, const void *, void *), void *);\n' > "$W/qsortr.h"
make -j"$(nproc)" MAKEINFO=true -C libctf CFLAGS="-O2 -include $W/qsortr.h" > make.log 2>&1 || true
make -j"$(nproc)" MAKEINFO=true all-binutils all-gas all-ld >> make.log 2>&1
# 5. the tree
"$HERE/stage-tree.sh" "$W"
