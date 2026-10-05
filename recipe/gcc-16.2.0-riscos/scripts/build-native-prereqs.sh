#!/bin/bash
# GMP / MPFR / MPC cross-built FOR RISC OS (static; the host libraries of a native, RISC OS-hosted GCC).  Uses the clean from-recipe toolchain (env-f) as the compiler.
# Same sources as build-host-prereqs.sh (the versions the GCCSDK 10.2.0 recipe uses).  GMP without assembly (like the 10.2.0 in-tree build: ABI=standard).
# usage: build-native-prereqs.sh            result: $HOME/gccsdk-next/hostlibs-riscos/{include,lib}
set -eu
E=${E:-$HOME/gccsdk-next/env-f}
S=$HOME/gccsdk-next/src/prereq
H=${H:-$HOME/gccsdk-next/hostlibs-riscos}
B=$HOME/gccsdk-next/build-native-libs
export PATH=$E/bin:/usr/bin:/bin:/usr/local/bin
T=arm-riscos-gnueabihf
# C23 is the default of GCC 15/16: these old configure scripts want K&R-style test programs and implicit declarations
W="-std=gnu17 -Wno-implicit-int -Wno-implicit-function-declaration -Wno-incompatible-pointer-types -Wno-int-conversion"
export CC="$E/bin/$T-gcc" AR="$E/bin/$T-ar" RANLIB="$E/bin/$T-ranlib" NM="$E/bin/$T-nm" STRIP="$E/bin/$T-strip"
export CFLAGS="-O2 $W" LDFLAGS="-Wl,--allow-shlib-undefined"
export CC_FOR_BUILD="gcc -O1 $W" CPP_FOR_BUILD="gcc -E"
mkdir -p "$B/gmp" "$B/mpfr" "$B/mpc" "$H"
cd "$B/gmp"  && "$S/gmp-6.1.0/configure"  --build=x86_64-pc-linux-gnu --host=none-riscos-gnueabihf --prefix="$H" --disable-shared --enable-static > configure.log 2>&1 && make -j"$(nproc)" > make.log 2>&1 && make install > install.log 2>&1
echo "gmp done $(date +%T)"
cd "$B/mpfr" && "$S/mpfr-3.1.4/configure" --build=x86_64-pc-linux-gnu --host=$T --prefix="$H" --disable-shared --enable-static --with-gmp="$H" > configure.log 2>&1 && make -j"$(nproc)" > make.log 2>&1 && make install > install.log 2>&1
echo "mpfr done $(date +%T)"
cd "$B/mpc"  && "$S/mpc-1.0.3/configure"  --build=x86_64-pc-linux-gnu --host=$T --prefix="$H" --disable-shared --enable-static --with-gmp="$H" --with-mpfr="$H" > configure.log 2>&1 && make -j"$(nproc)" > make.log 2>&1 && make install > install.log 2>&1
echo "mpc done $(date +%T)"
ls -la "$H/lib" | grep "\.a$"
