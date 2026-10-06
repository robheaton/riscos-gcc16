#!/bin/bash
# Configure a NATIVE (RISC OS-hosted) GCC 16.2.0 for arm-riscos-gnueabihf, cross-built on Linux (host = target = arm-riscos-gnueabihf, build = this machine).
# The TARGET options are exactly those of configure-gcc16-full.sh, so the native cc1 is the same compiler as the cross cc1.
# usage: [HOST_OPT=-Os] [NATIVE_LTO=yes] configure-gcc16-native.sh SRC_TREE BUILD_DIR CROSS_PREFIX NATIVE_PREFIX
#   HOST_OPT      optimisation flags for the HOST code (cc1, cc1plus, the driver); default -O2.  -Os makes cc1plus markedly smaller, which matters because an
#                 EABI program's whole image has to fit in the application space (the Wimp slot) next to the driver that vforks it.
#   EXTRA_CONFIGURE_ARGS  more configure arguments (word split), e.g. --with-plugin-ld=PROGRAM: the native ld has no linker plugin support (see build-native-lto.sh)
#   NATIVE_LTO    yes = --enable-lto: also builds lto1 and lto-wrapper and compiles cc1 / cc1plus / f951 with ENABLE_LTO (default no: the native compiler of Gcc16 16.2.0-8 and earlier has no LTO)
#   SRC_TREE      the ported tree plus patches-native/ (scripts/apply-port-native.sh)
#   CROSS_PREFIX  the cross toolchain used to BUILD it (env-f)
#   NATIVE_PREFIX the install prefix baked into the native compiler (make install DESTDIR=... stages it)
set -eu
SRC=${1:?usage: configure-gcc16-native.sh SRC_TREE BUILD_DIR CROSS_PREFIX NATIVE_PREFIX}
B=${2:?}
P=${3:?}
NP=${4:?}
export PATH=$P/bin:/usr/bin:/bin:/usr/local/bin
H=${HOSTLIBS_RISCOS:-$HOME/gccsdk-next/hostlibs-riscos}     # GMP/MPFR/MPC for RISC OS (build-native-prereqs.sh)
HERE=$(cd "$(dirname "$0")/.." && pwd)
NOLIBM=$HERE/data/nolibm        # an EMPTY libm.so first in the link path: libm.so.1 is an empty stub on RISC OS (math is in libunixlib), but a program with it in DT_NEEDED
                                # cannot be started as a vfork child (hangs / aborts), and the native compiler programs are exactly that
# riscos-da.o: the heap of every program in a dynamic area (data/riscos-da.c says why); linked into all of them through LDFLAGS
mkdir -p "$B"
"$P/bin/arm-riscos-gnueabihf-gcc" -O2 -c "$HERE/data/riscos-da.c" -o "$B/riscos-da.o"
export ac_cv_func_shl_load=no ac_cv_lib_dld_shl_load=no ac_cv_func_dlopen=yes
mkdir -p "$B"; cd "$B"
"$SRC/configure" \
  --build=x86_64-pc-linux-gnu --host=arm-riscos-gnueabihf --target=arm-riscos-gnueabihf \
  --prefix="$NP" \
  --with-gmp=$H --with-mpfr=$H --with-mpc=$H \
  --enable-languages=${LANGS:-c,c++,fortran} --$([ "${NATIVE_LTO:-no}" = yes ] && echo enable || echo disable)-lto --disable-plugin \
  --enable-shared=libgcc,libstdc++,libgfortran,libbacktrace \
  --enable-threads=posix \
  --enable-sjlj-exceptions=no \
  --enable-__cxa_atexit \
  --enable-c99 \
  --enable-cmath --disable-libstdcxx-pch --disable-wchar_t --disable-libquadmath --disable-nls --disable-tls \
  --disable-libssp --disable-libgomp --disable-libitm --disable-libatomic \
  --disable-libsanitizer --disable-libvtv --disable-libcc1 --disable-multilib \
  --with-pkgversion='GCCSDK GCC 16.2.0 (experimental forward-port)' \
  --with-bugurl=https://github.com/robheaton/riscos-gcc16/issues \
  --with-abi=aapcs-linux --with-float=hard --with-fpu=vfpv3 --with-arch=armv7-a \
  ${EXTRA_CONFIGURE_ARGS:-} \
  CFLAGS="${HOST_OPT:--O2}" CXXFLAGS="${HOST_OPT:--O2}" LDFLAGS="-L$NOLIBM $B/riscos-da.o -static-libstdc++ -static-libgcc -Wl,--allow-shlib-undefined" \
  CFLAGS_FOR_TARGET="-O2 -g" CXXFLAGS_FOR_TARGET="-O2 -g"
