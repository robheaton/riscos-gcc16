#!/bin/bash
# build-native-binutils.sh PREFIX WORKDIR [SRC]: builds binutils (readelf, objdump, ar, as, ld ... ) as PROGRAMS FOR RISC OS 64 with the aarch64-riscos tool chain in PREFIX (--host=aarch64-riscos, -munixlib), in WORKDIR.
# TARGET=arm-riscos-gnueabihf (with SRC = the binutils source with the RISC OS 32 bit port applied) builds the tools that make 32 bit code.  SRC = the binutils source with the aarch64-riscos patch applied (default PREFIX/../target7-work/binutils-src as made by riscos64/target/build-target.sh).  The source is copied, and three small changes are made to the copy:
# O_BINARY is not wanted (RISC OS has one kind of file), a pointer difference in dwarf.c needs a cast for GCC 16, memrchr is declared by picolibc as a macro, and getrusage is left out (picolibc's struct rusage has only the times).  Only compiles and links; the programs run on Pyromaniac.
set -e
PREFIX=$(cd "${1:?PREFIX of the aarch64-riscos tool chain}" && pwd); W=${2:?WORKDIR}; mkdir -p "$W"; W=$(cd "$W" && pwd)
SRC=${3:-$PREFIX/../target7-work/binutils-src}
[ -d "$SRC" ] || { echo "no source at $SRC"; exit 1; }
export PATH=$PREFIX/bin:$PATH
if [ ! -d "$W/src" ]; then
  cp -a "$SRC" "$W/src"
  sed -i 's|^#if O_BINARY$|#if 0 /* RISC OS: no text mode */|' "$W/src/include/binary-io.h"
  python3 - "$W/src/binutils/dwarf.c" <<'PY'
import sys
p = sys.argv[1]; t = open(p).read()
t = t.replace("(int) (strrchr (main_filename, '/') - main_filename)", "(int) ((const char *) strrchr (main_filename, '/') - (const char *) main_filename)")
t = t.replace("(int) (strrchr (main_filename, '/') - (const char *) main_filename)", "(int) ((const char *) strrchr (main_filename, '/') - (const char *) main_filename)")
open(p, 'w').write(t)
PY
fi
mkdir -p "$W/build" && cd "$W/build"
[ -f Makefile ] || "$W/src/configure" --host=aarch64-riscos --build=$(gcc -dumpmachine) --target=${TARGET:-aarch64-riscos} --prefix="$W/inst" --disable-nls --disable-werror --disable-gdb --disable-sim --disable-gprof --disable-plugins --disable-shared --enable-static \
  ac_cv_func_getrusage=no CC="aarch64-riscos-gcc -munixlib" CFLAGS="-O2 -DHAVE_MEMRCHR=1" > configure.log 2>&1
# configure ignores the cache variable in some directories: take getrusage out of the config.h files that mention it
grep -rl "define HAVE_GETRUSAGE" --include=config.h . 2>/dev/null | xargs -r sed -i 's|^#define HAVE_GETRUSAGE 1|/* #undef HAVE_GETRUSAGE */|'
make -k -j"${J:-8}" all-binutils all-gas all-ld > make.log 2>&1 || true
# the first pass runs config.status in some directories, which writes config.h again: take getrusage out once more, and build what failed
grep -rl "define HAVE_GETRUSAGE" --include=config.h . 2>/dev/null | xargs -r sed -i 's|^#define HAVE_GETRUSAGE 1|/* #undef HAVE_GETRUSAGE */|'
find . -name stamp-h1 -exec touch {} +
make -k -j"${J:-8}" all-binutils all-gas all-ld >> make.log 2>&1 || true
ls -l binutils/readelf binutils/objdump binutils/ar gas/as-new ld/ld-new 2>&1 | sed 's/^/  /'
