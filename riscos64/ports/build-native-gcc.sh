#!/bin/bash
# build-native-gcc.sh PREFIX WORKDIR GMPDIR [SRC]: builds the GCC compiler proper (xgcc, cc1) as a PROGRAM FOR RISC OS 64 with the aarch64-riscos tool chain in PREFIX (--host=aarch64-riscos, -munixlib), in WORKDIR.
# GMPDIR = where build-gmp-mpfr-mpc.sh installed GMP, MPFR and MPC for AArch64 RISC OS (WORKDIR/inst of that script).  SRC = the GCC source with the aarch64-riscos patch (default PREFIX/../target6-work/gcc-src).
# Only compiles and links: the programs are meant to run on Pyromaniac (cc1 is large).  Needs the native binutils too (build-native-binutils.sh) for a full native tool chain.
set -e
PREFIX=$(cd "${1:?PREFIX of the aarch64-riscos tool chain}" && pwd); W=${2:?WORKDIR}; mkdir -p "$W"; W=$(cd "$W" && pwd)
GMP=$(cd "${3:?GMPDIR}" && pwd)
SRC=${4:-$PREFIX/../target6-work/gcc-src}
[ -d "$SRC" ] || { echo "no source at $SRC"; exit 1; }
export PATH=$PREFIX/bin:$PATH
mkdir -p "$W/build" && cd "$W/build"
[ -f Makefile ] || "$SRC/configure" --host=aarch64-riscos --build=$(gcc -dumpmachine) --target=aarch64-riscos --prefix="$W/inst" --with-gmp="$GMP" --with-mpfr="$GMP" --with-mpc="$GMP" \
  --enable-languages=c --disable-nls --disable-shared --disable-threads --disable-libssp --disable-libgomp --disable-libquadmath --disable-libatomic --disable-multilib --disable-bootstrap --disable-werror \
  --disable-plugin --disable-lto --disable-libcc1 --without-zstd --without-isl --with-system-zlib=no \
  ac_cv_func_getrusage=no CC="aarch64-riscos-gcc -munixlib" CXX="aarch64-riscos-g++ -munixlib" CFLAGS="-O2 -DHAVE_MEMRCHR=1" CXXFLAGS="-O2 -DHAVE_MEMRCHR=1" > configure.log 2>&1
make -k -j"${J:-8}" all-gcc > make.log 2>&1 || true
ls -l gcc/xgcc gcc/cc1 2>&1 | sed 's/^/  /'
