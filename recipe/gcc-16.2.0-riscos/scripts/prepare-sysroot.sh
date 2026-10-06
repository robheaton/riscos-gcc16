#!/bin/bash
# Create a fresh install prefix for the GCC 16.2 cross compiler build:
#   - the UnixLib/OSLib headers and runtime pieces (libunixlib, libm, libdl, crt0.o, ld-riscos) copied from a GCCSDK 10.2.0 install
#     (NOT the GCC-specific runtime libs: libgcc_s, libstdc++, libsupc++, libgfortran, libatomic are built and installed by the new build)
#   - the binutils entries as symlinks into the binutils 2.45.1 port (tools only; nothing is ever written through them)
# usage: prepare-sysroot.sh PREFIX [OLD_INSTALL=$HOME/gccsdk/env] [NEW_BINUTILS=$HOME/gccsdk-next/binutils-2.45.1-install]
set -eu
P=${1:?usage: prepare-sysroot.sh PREFIX [OLD_INSTALL] [NEW_BINUTILS]}
OLD=${2:-$HOME/gccsdk/env}
NEW=${3:-$HOME/gccsdk-next/binutils-2.45.1-install}
T=arm-riscos-gnueabihf
[ -e "$P" ] && { echo "$P exists: refusing to touch it"; exit 1; }
mkdir -p "$P/$T/lib" "$P/$T/bin" "$P/bin" "$P/lib"
cp -a "$OLD/$T/include" "$P/$T/include"
cd "$OLD/$T/lib"
for f in crt0.o gcrt0.o ldscripts libc.a libm.* libpthread.a libunixlib.* libdl.* ; do [ -e "$f" ] && cp -a "$f" "$P/$T/lib/" || true; done
cd "$OLD/lib"
for f in ld-riscos-eabihf.so ld-riscos.so.2 libdl.*; do [ -e "$f" ] && cp -a "$f" "$P/lib/" || true; done
# the <unistd.h> fix (_SC_NPROCESSORS_ONLN used to share a value with _SC_BC_DIM_MAX and was not a macro): compiled programs then ask the
# right question; against an old libunixlib sysconf () answers -1 (unknown) instead of 2048 processors
HERE=$(cd "$(dirname "$0")/.." && pwd)
patch -p2 -N -d "$P/$T" < "$HERE/patches-unixlib/unixlib-sysconf-nprocessors.patch" > /dev/null || { echo "ERROR: unistd.h patch failed"; exit 1; }
# <semaphore.h> did not declare sem_timedwait (the UnixLib of the runtime packages has had a real one since 16.2.0-12)
patch -p2 -N -d "$P/$T" < "$HERE/patches-unixlib/unixlib-semaphore-timedwait-decl.patch" > /dev/null || { echo "ERROR: semaphore.h patch failed"; exit 1; }
# no absolute or escaping symlinks may have been copied (a later `make install` could write through them)
if find "$P" -type l \( -lname '/*' -o -lname '../../*' \) | grep -q .; then echo "ERROR: escaping symlinks in $P"; exit 1; fi
for t in ar as ld ld.bfd nm objcopy objdump ranlib readelf strip; do ln -s "$NEW/$T/bin/$t" "$P/$T/bin/$t"; done
for t in addr2line ar as c++filt elfedit gprof ld ld.bfd nm objcopy objdump ranlib readelf size strings strip; do ln -s "$NEW/bin/$T-$t" "$P/bin/$T-$t"; done
echo "prepared $P: $(du -sh "$P" | cut -f1)"; "$P/$T/bin/ld" --version | head -1
