#!/bin/bash
# install-unixlib-sysroot.sh PREFIX UNIXLIB_BUILD_DIR
# Puts the UnixLib that build-unixlib.sh made (libunixlib.so / .a, libm, crt0.o, gcrt0.o) into the sysroot of the cross compiler, in place of the GCCSDK 10.2.0 files that prepare-sysroot.sh
# copied there: programs are linked against these (the link only needs the symbols; the program runs against the SharedLibs-C-armeabihf package on RISC OS), so the fixed library must be the one
# they see: from 16.2.0-13 a program built with -pg calls __gnu_mcount_nc, which only the fixed libunixlib.so exports, and links gcrt0.o.
# usage: install-unixlib-sysroot.sh ~/gccsdk-next/env-f ~/gccsdk-next/unixlib/build
set -eu
P=${1:?usage: install-unixlib-sysroot.sh PREFIX UNIXLIB_BUILD_DIR}
U=${2:?}
T=arm-riscos-gnueabihf
L=$P/$T/lib
[ -d "$L" ] || { echo "ERROR: $L does not exist: run prepare-sysroot.sh first"; exit 1; }
for f in .libs/libunixlib.so.5.0.0 .libs/libunixlib.a .libs/libm.so.1.0.0 .libs/libm.a crt0.o gcrt0.o; do
  [ -f "$U/$f" ] || { echo "ERROR: $U/$f not found: build UnixLib first (build-unixlib.sh)"; exit 1; }
done
# a new file in place of the old one (the old ones may be hard links into another tree)
for f in .libs/libunixlib.so.5.0.0 .libs/libunixlib.a .libs/libm.so.1.0.0 .libs/libm.a crt0.o gcrt0.o; do
  b=$(basename "$f")
  rm -f "$L/$b.new" && cp "$U/$f" "$L/$b.new" && mv -f "$L/$b.new" "$L/$b"
done
# the symbolic links the linker follows (they exist already; make sure)
ln -sf libunixlib.so.5.0.0 "$L/libunixlib.so"; ln -sf libunixlib.so.5.0.0 "$L/libunixlib.so.5"
ln -sf libm.so.1.0.0 "$L/libm.so"; ln -sf libm.so.1.0.0 "$L/libm.so.1"
if ! "$P/bin/$T-nm" -D --defined-only "$L/libunixlib.so.5.0.0" | grep -q ' __gnu_mcount_nc$'; then
  echo "WARNING: the installed libunixlib.so does not export __gnu_mcount_nc: it is not the library of 16.2.0-13 or later (programs built with -pg will not link)"
fi
echo "installed UnixLib from $U into $L"
ls -la "$L/libunixlib.so.5.0.0" "$L/crt0.o" "$L/gcrt0.o"
