#!/bin/bash
# Host-side prerequisites for GCC 16 (static, PIC), built with host GCC 15 workarounds
# (the old GMP / MPFR / MPC sources need -std=gnu17 and the implicit-declaration warnings off with a GCC 15 host compiler). Not exported to the GCC build.
set -e
# H: where the static libraries are installed (configure-gcc16-full.sh reads it from $HOSTLIBS); S: the extracted gmp-6.1.0, mpfr-3.1.4, mpc-1.0.3; B: scratch directory
H=${HOSTLIBS:-$HOME/gccsdk-next/hostlibs}
S=${PREREQ_SRC:-$HOME/gccsdk-next/src/prereq}
B=${PREREQ_BUILD:-$HOME/gccsdk-next/build-host}
J=${JOBS:-$(nproc)}
export CFLAGS="-O2 -fPIC -std=gnu17 -Wno-implicit-int -Wno-implicit-function-declaration"
export CXXFLAGS="-O2 -fPIC -std=gnu++17"
mkdir -p $B/gmp $B/mpfr $B/mpc
cd $B/gmp  && $S/gmp-6.1.0/configure --prefix=$H --disable-shared --enable-static --with-pic > configure.log 2>&1 && make -j"$J" > make.log 2>&1 && make install > install.log 2>&1
echo "gmp done $(date +%T)"
cd $B/mpfr && $S/mpfr-3.1.4/configure --prefix=$H --disable-shared --enable-static --with-pic --with-gmp=$H > configure.log 2>&1 && make -j"$J" > make.log 2>&1 && make install > install.log 2>&1
echo "mpfr done $(date +%T)"
cd $B/mpc  && $S/mpc-1.0.3/configure --prefix=$H --disable-shared --enable-static --with-pic --with-gmp=$H --with-mpfr=$H > configure.log 2>&1 && make -j"$J" > make.log 2>&1 && make install > install.log 2>&1
echo "mpc done $(date +%T)"
echo ALLDONE
