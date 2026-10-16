#!/bin/bash
# a freestanding AArch64 cross tool chain (binutils 2.45.1 + GCC 16.2.0, C and C++ (the compiler only: no libstdc++, the kit has its own runtime)) from the pristine tarballs, for the RISC OS 64 tests.  usage: build-a64-toolchain.sh   (A64ROOT=<work folder>, SRCDIR=<the tarballs>, HOSTLIBS=<GMP, MPFR, MPC>)
set -e
S=${A64ROOT:-$HOME/a64}; P=$S/prefix; H=${HOSTLIBS:-$HOME/gccsdk-next/hostlibs}   # H: static GMP, MPFR, MPC for the host (build-host-prereqs.sh of the recipe)
cd $S && mkdir -p src build-bu build-gcc
[ -d src/binutils-2.45.1 ] || tar -xf ${SRCDIR:-$HOME/gccsdk-next/src}/binutils-2.45.1.tar.xz -C src
[ -d src/gcc-16.2.0 ] || tar -xf ${SRCDIR:-$HOME/gccsdk-next/src}/gcc-16.2.0.tar.xz -C src
echo "$(date +%T) binutils"; cd $S/build-bu
../src/binutils-2.45.1/configure --target=aarch64-none-elf --prefix=$P --disable-nls --disable-werror --disable-gdb --disable-sim --disable-gprofng > conf-bu.log 2>&1
make -j22 > make-bu.log 2>&1 && make install > inst-bu.log 2>&1
echo "$(date +%T) gcc"; cd $S/build-gcc
PATH=$P/bin:$PATH ../src/gcc-16.2.0/configure --target=aarch64-none-elf --prefix=$P --with-gmp=$H --with-mpfr=$H --with-mpc=$H --enable-languages=c,c++ --without-headers --with-newlib --disable-nls --disable-shared --disable-threads --disable-libssp --disable-libgomp --disable-libquadmath --disable-libatomic --disable-multilib --disable-bootstrap --disable-werror > conf-gcc.log 2>&1
PATH=$P/bin:$PATH make -j22 all-gcc > make-gcc.log 2>&1
PATH=$P/bin:$PATH make -j22 all-target-libgcc > make-libgcc.log 2>&1
PATH=$P/bin:$PATH make install-gcc install-target-libgcc > inst-gcc.log 2>&1
echo "$(date +%T) DONE"
