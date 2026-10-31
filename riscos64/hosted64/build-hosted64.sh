#!/bin/bash
# build-hosted64.sh WORK: builds the compiler that RUNS on RISC OS 64 (host aarch64-riscos) and makes programs for 32 bit RISC OS (target arm-riscos-gnueabihf), C and C++, and the binutils for it, on Linux with the
# AArch64 cross compiler of this repository (-munixlib).  Result: WORK/tree32 (the flat layout of ports/make-native-tree.py).  About 15 minutes on 22 cores.
# Needs: T64 = the installed aarch64-riscos tool chain (its SDK headers must have had fix-picolibc-headers.py run on them: <sys/wait.h> declares wait4), GMP/MPFR/MPC for AArch64 RISC OS
# (GMPDIR = ports/build-gmp-mpfr-mpc.sh's inst), the recipe of the 32 bit port (~/gccsdk-next/recipe), the Linux-hosted arm-riscos-gnueabihf tools in env-f (configure runs them to learn what the assembler can do) and the tree of the
# 32 bit native compiler (native-tree/gcc16: its headers and libraries are the target's sysroot).
set -eu
W=${1:?WORK}; mkdir -p "$W"; W=$(cd "$W" && pwd)
G=${GCCNEXT:?GCCNEXT = the folder that has recipe/ and env-f/ of the 32 bit port}; R=$G/recipe; T64=${T64:?T64 = the installed aarch64-riscos tool chain}; GMP=${GMPDIR:?GMPDIR = the GMP, MPFR and MPC for AArch64 RISC OS}
HERE=$(cd "$(dirname "$0")" && pwd)
export PATH=$T64/bin:/usr/bin:/bin:/usr/local/bin
cd "$W"
# 1. GCC with the 32 bit RISC OS port (C and C++), no host patches: UnixLib 64 is a POSIX host
rm -rf gcc-16.2.0; tar -xf "$G/src/gcc-16.2.0.tar.xz"
"$R/gcc-16.2.0-riscos/scripts/apply-port.sh" gcc-16.2.0 > apply1.log 2>&1
"$R/gcc-16.2.0-riscos/scripts/apply-port-cxx.sh" gcc-16.2.0 > apply2.log 2>&1
mkdir -p build-gcc; cd build-gcc
"$W/gcc-16.2.0/configure" --host=aarch64-riscos --build=x86_64-linux-gnu --target=arm-riscos-gnueabihf --prefix=/GCC32 --with-build-time-tools=$G/env-f/arm-riscos-gnueabihf/bin \
  --with-gmp=$GMP --with-mpfr=$GMP --with-mpc=$GMP --enable-languages=c,c++ --disable-nls --disable-shared --enable-threads=posix --enable-sjlj-exceptions=no --enable-__cxa_atexit --enable-c99 --enable-cmath \
  --disable-libstdcxx-pch --disable-wchar_t --disable-libquadmath --disable-libssp --disable-libgomp --disable-libitm --disable-libatomic --disable-libsanitizer --disable-libvtv --disable-libcc1 \
  --disable-multilib --disable-bootstrap --disable-werror --disable-plugin --disable-lto --without-zstd --without-isl --with-system-zlib=no \
  --with-pkgversion='GCCSDK GCC 16.2.0 (experimental forward-port)' --with-bugurl=https://github.com/robheaton/riscos-gcc16/issues --with-abi=aapcs-linux --with-float=hard --with-fpu=vfpv3 --with-arch=armv7-a \
  ac_cv_func_getrusage=no CC="aarch64-riscos-gcc -munixlib" CXX="aarch64-riscos-g++ -munixlib" CFLAGS="-O2 -DHAVE_MEMRCHR=1" CXXFLAGS="-O2 -DHAVE_MEMRCHR=1" > configure.log 2>&1
make MAKEINFO=true configure-gcc > cfg-gcc.log 2>&1
make -k -j"$(nproc)" MAKEINFO=true all-gcc > make.log 2>&1 || true      # (the `specs` step runs the AArch64 driver on Linux and fails: expected)
# the driver and collect2 only wait: a 16 MB heap for them (see ports/build-native-gcc.sh)
rm -f gcc/xgcc gcc/xg++ gcc/collect2
( cd gcc && make -k xgcc xg++ collect2 MAKEINFO=true LDFLAGS="-static-libstdc++ -static-libgcc -Wl,--defsym=__ul64_heap_max=0x1000000" >> ../make.log 2>&1 || true )
# 2. binutils for the same host and target (the 32 bit port of binutils)
cd "$W"; rm -rf binutils-2.45.1; tar -xf "$G/src/binutils-2.45.1.tar.xz"
"$R/binutils-2.45.1-riscos/scripts/apply-port.sh" "$W/binutils-2.45.1" > bu-apply.log 2>&1
TARGET=arm-riscos-gnueabihf J=$(nproc) "$G/riscos64/ports/build-native-binutils.sh" "$T64" "$W/bu64" "$W/binutils-2.45.1" > bu64.out 2>&1
# 3. the tree
python3 "$HERE/make-tree32.py" "$W/tree32" --gcc "$W/build-gcc" --binutils "$W/bu64/build" --sysroot "$G/native-tree/gcc16" --cxx
