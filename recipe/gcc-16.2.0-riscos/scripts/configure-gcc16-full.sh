#!/bin/bash
# Configure the complete GCC 16.2.0 arm-riscos-gnueabihf cross compiler: C, C++, Fortran, LTO; shared libgcc/libstdc++/libgfortran.
# usage: configure-gcc16-full.sh SRC_TREE BUILD_DIR PREFIX      (PREFIX prepared with prepare-sysroot.sh; BUILD_DIR must be empty or new)
# Clean PATH on purpose: nothing from ~/gccsdk may shadow host tools.
set -eu
SRC=${1:?usage: configure-gcc16-full.sh SRC_TREE BUILD_DIR PREFIX}
B=${2:?}
P=${3:?}
export PATH=$P/bin:/usr/bin:/bin:/usr/local/bin
H=${HOSTLIBS:-$HOME/gccsdk-next/hostlibs}     # GMP/MPFR/MPC (build-host-prereqs.sh)
# overrides from the 10.2.0 recipe's setvars (cross-compiling: configure cannot run these tests); they must also be in make's environment
export ac_cv_func_shl_load=no ac_cv_lib_dld_shl_load=no ac_cv_func_dlopen=yes glibcxx_cv_c99_math_tr1=yes glibcxx_cv_c99_math_funcs=yes
mkdir -p "$B"; cd "$B"
"$SRC/configure" \
  --prefix="$P" \
  --target=arm-riscos-gnueabihf \
  --with-gmp=$H --with-mpfr=$H --with-mpc=$H \
  --enable-languages=c,c++,fortran --enable-lto \
  --enable-shared=libgcc,libstdc++,libgfortran,libbacktrace \
  --enable-threads=posix \
  --enable-sjlj-exceptions=no \
  --enable-__cxa_atexit \
  --enable-c99 \
  --enable-cmath --disable-libstdcxx-pch --disable-wchar_t --disable-libquadmath --disable-nls --disable-tls \
  --disable-libssp --disable-libgomp --disable-libitm --disable-libatomic \
  --disable-libsanitizer --disable-libvtv --disable-libcc1 \
  --with-pkgversion='GCCSDK GCC 16.2.0 (experimental forward-port)' \
  --with-bugurl=https://github.com/robheaton/riscos-gcc16/issues \
  --with-abi=aapcs-linux --with-float=hard --with-fpu=vfpv3 --with-arch=armv7-a \
  CFLAGS_FOR_TARGET="-O2 -g" CXXFLAGS_FOR_TARGET="-O2 -g"
