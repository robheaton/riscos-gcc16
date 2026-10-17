#!/bin/bash
# build-native-binutils.sh PREFIX WORKDIR [SRC]: builds binutils (readelf, objdump, ar, as, ld ... ) as PROGRAMS FOR RISC OS 64 with the aarch64-riscos tool chain in PREFIX (--host=aarch64-riscos, -munixlib), in WORKDIR.
# SRC = the binutils source with the aarch64-riscos patch applied (default PREFIX/../target6-work/binutils-src as made by riscos64/target/build-target.sh).  The source is copied, and three small changes are made to the copy:
# O_BINARY is not wanted (RISC OS has one kind of file), a pointer difference in dwarf.c needs a cast for GCC 16, and memrchr is declared by picolibc as a macro.  Only compiles and links; the programs run on Pyromaniac.
set -e
PREFIX=$(cd "${1:?PREFIX of the aarch64-riscos tool chain}" && pwd); W=${2:?WORKDIR}; mkdir -p "$W"; W=$(cd "$W" && pwd)
SRC=${3:-$PREFIX/../target6-work/binutils-src}
[ -d "$SRC" ] || { echo "no source at $SRC"; exit 1; }
export PATH=$PREFIX/bin:$PATH
if [ ! -d "$W/src" ]; then
  cp -a "$SRC" "$W/src"
  sed -i 's|^#if O_BINARY$|#if 0 /* RISC OS: no text mode */|' "$W/src/include/binary-io.h"
  sed -i 's|(int) (strrchr (main_filename, .\/.) - main_filename)|(int) (strrchr (main_filename, '"'"'/'"'"') - (const char *) main_filename)|' "$W/src/binutils/dwarf.c"
fi
mkdir -p "$W/build" && cd "$W/build"
[ -f Makefile ] || "$W/src/configure" --host=aarch64-riscos --build=$(gcc -dumpmachine) --target=aarch64-riscos --prefix="$W/inst" --disable-nls --disable-werror --disable-gdb --disable-sim --disable-gprof --disable-plugins --disable-shared --enable-static \
  CC="aarch64-riscos-gcc -munixlib" CFLAGS="-O2 -DHAVE_MEMRCHR=1" > configure.log 2>&1
make -k -j"${J:-8}" all-binutils all-gas all-ld > make.log 2>&1 || true
ls -l binutils/readelf binutils/objdump binutils/ar gas/as-new ld/ld-new 2>&1 | sed 's/^/  /'
