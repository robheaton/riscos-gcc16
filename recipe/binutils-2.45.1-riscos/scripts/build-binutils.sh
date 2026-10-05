#!/bin/bash
# build-binutils.sh <patched-source-dir> <build-dir> <install-prefix>
# Builds a cross binutils for arm-riscos-gnueabihf.  Plugins are enabled (needed for GCC's -flto).
# Host requirements: a C compiler, make; no texinfo/bison/flex needed (MAKEINFO=true).
set -e
SRC=$(readlink -f "${1:?source dir}"); B=${2:?build dir}; PREFIX=${3:?install prefix}
mkdir -p "$B"; cd "$B"
"$SRC/configure" --target=arm-riscos-gnueabihf --prefix="$PREFIX" \
  --disable-nls --disable-werror --disable-gdb --disable-gprofng --disable-gprof --disable-sim --disable-gold \
  --enable-plugins --without-zstd --without-debuginfod --disable-multilib
make -j"$(nproc)" MAKEINFO=true all
make MAKEINFO=true install
# GCC finds as/ld via its -B / tool directories: either point a GCC install's
# <prefix>/arm-riscos-gnueabihf/bin/{as,ld,ar,nm,objcopy,objdump,ranlib,readelf,strip} at these, or pass
# -B$PREFIX/arm-riscos-gnueabihf/bin/ (the first -B directory wins).
