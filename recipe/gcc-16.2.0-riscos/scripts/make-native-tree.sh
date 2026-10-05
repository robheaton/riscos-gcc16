#!/bin/bash
# Stage the NATIVE (RISC OS-hosted) GCC 16.2.0 tool chain as one directory tree: the Linux-hosted install layout (Unix file names; the compiler finds its files
# relative to the place of the gcc program), executables named ,e1f (the NAS / Samba convention for the file type; the packaging script turns that into RISC OS
# types), everything stripped.  Run build-native-all.sh (compilers) and build-binutils-native.sh (as, ld, ...) first.
#
# usage: make-native-tree.sh OUTDIR [GCC_STAGE] [BINUTILS_INSTALL] [MAKE_BINARY]
#   OUTDIR            the tree is made in OUTDIR/gcc16 (an existing one is renamed gcc16-prev-DATE, never deleted)
#   GCC_STAGE         default $HOME/gccsdk-next/native-stage2-O2     (build-native-all.sh O2: `make install-gcc DESTDIR=...`)
#   BINUTILS_INSTALL  default $HOME/gccsdk-next/binutils-native-install2
#   MAKE_BINARY       default $HOME/gccsdk-next/make-riscos/install/bin/make   (recipe/make-4.4.1-riscos/scripts/build-make.sh); left out when it does not exist
#
# What goes where (T = arm-riscos-gnueabihf, V = 16.2.0):
#   bin/{gcc,g++,cpp,gfortran,make}         the drivers (gfortran and libexec/.../f951 when built) and GNU make
#   libexec/gcc/T/V/{cc1,cc1plus,collect2}  the compiler proper (and lto1, lto-wrapper when built with LTO)
#   T/bin/{as,ld,ar,nm,...}                 binutils 2.45.1 (the driver finds as and ld here; put this directory on Run$Path for the rest)
#   lib/gcc/T/V/                            GCC's own headers (include, include-fixed, with unwind.h and gcov.h, which libgcc installs), crt*.o, libgcc.a, libgcc_eh.a, libgcov.a
#   T/lib/                                  crt0.o, libunixlib.so, libgcc_s*, libdl, libstdc++.a, libsupc++.a, libstdc++exp.a, libstdc++fs.a; an EMPTY libm.so (math is in libunixlib)
#   T/include/                              the UnixLib headers
#   include/c++/V/                          the libstdc++ headers, in the layout the NATIVE compiler searches (include/c++/V, T/bits, backward)
set -eu
OUT=${1:?usage: make-native-tree.sh OUTDIR [GCC_STAGE] [BINUTILS_INSTALL]}
G=${GCCNEXT:-$HOME/gccsdk-next}
NS=${2:-$G/native-stage2-O2}/RISCOS/gccnative
BI=${3:-$G/binutils-native-install2}
MK=${4:-$G/make-riscos/install/bin/make}
E=$G/env-f
HERE=$(cd "$(dirname "$0")/.." && pwd)
ST=$E/bin/arm-riscos-gnueabihf-strip
T=arm-riscos-gnueabihf
V=16.2.0
R=$OUT/gcc16
mkdir -p "$OUT"
[ -d "$R" ] && mv "$R" "$OUT/gcc16-prev-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$R/bin" "$R/libexec/gcc/$T/$V" "$R/lib/gcc/$T/$V" "$R/$T/bin" "$R/$T/lib" "$R/include/c++"

exe() { $ST --strip-all -o "$2,e1f" "$1"; }                 # an executable, stripped, file type e1f
for p in gcc g++ cpp; do exe "$NS/bin/$p" "$R/bin/$p"; done
for p in cc1 cc1plus collect2; do exe "$NS/libexec/gcc/$T/$V/$p" "$R/libexec/gcc/$T/$V/$p"; done
# LTO (only when the compilers were built with NATIVE_LTO=yes, build-native-lto.sh): lto1 and lto-wrapper.  The linker plugin is NOT used: the native ld has no plugin support, the driver is
# configured with HAVE_LTO_PLUGIN 0, so -flto links through collect2, which runs lto-wrapper on the LTO objects and links what comes back.
LTO=0
for p in lto1 lto-wrapper; do
  if [ -f "$NS/libexec/gcc/$T/$V/$p" ]; then exe "$NS/libexec/gcc/$T/$V/$p" "$R/libexec/gcc/$T/$V/$p"; LTO=1; fi
done
for p in as ld ar nm objdump objcopy ranlib readelf strip size strings addr2line c++filt elfedit; do exe "$BI/bin/$p" "$R/$T/bin/$p"; done
[ -f "$MK" ] && exe "$MK" "$R/bin/make"                     # GNU make 4.4.1 (cross-built for RISC OS)
# Fortran (only when the compilers were built with it): the driver, f951, the intrinsic modules, libgfortran (STATIC: no libgfortran.so in the tree, so programs do not need the
# Fortran runtime package), its spec file and ISO_Fortran_binding.h
FORTRAN=0
if [ -f "$NS/libexec/gcc/$T/$V/f951" ]; then
  FORTRAN=1
  exe "$NS/bin/gfortran" "$R/bin/gfortran"
  exe "$NS/libexec/gcc/$T/$V/f951" "$R/libexec/gcc/$T/$V/f951"
fi

# GCC's headers: install-gcc does not install unwind.h and gcov.h (libgcc does): take them from the cross install (the same sources)
GL=$NS/lib/gcc/$T/$V
cp -r "$GL/include" "$GL/include-fixed" "$R/lib/gcc/$T/$V/"
for f in unwind.h gcov.h; do cp "$E/lib/gcc/$T/$V/include/$f" "$R/lib/gcc/$T/$V/include/"; done
# crt files and the compiler support libraries (target code: from the cross install)
for f in crtbegin.o crtbeginS.o crtbeginT.o crtend.o crtendS.o crti.o crtn.o; do cp "$E/lib/gcc/$T/$V/$f" "$R/lib/gcc/$T/$V/"; done
for f in libgcc.a libgcc_eh.a libgcov.a; do cp "$E/lib/gcc/$T/$V/$f" "$R/lib/gcc/$T/$V/"; $ST --strip-debug "$R/lib/gcc/$T/$V/$f"; done

if [ $FORTRAN = 1 ]; then
  mkdir -p "$R/lib/gcc/$T/$V/finclude"
  cp "$E/lib/gcc/$T/$V/finclude/"*.mod "$R/lib/gcc/$T/$V/finclude/"
  cp "$E/lib/gcc/$T/$V/include/ISO_Fortran_binding.h" "$R/lib/gcc/$T/$V/include/"
  cp "$E/lib/gcc/$T/$V/libcaf_single.a" "$R/lib/gcc/$T/$V/"; $ST --strip-debug "$R/lib/gcc/$T/$V/libcaf_single.a"
fi
L=$E/$T/lib
cp "$L/crt0.o" "$L/gcrt0.o" "$L/libgcc_s.so" "$L/libgcc_s_asneeded.so" "$R/$T/lib/"
cp "$L/libgcc_s.so.1" "$R/$T/lib/"; $ST --strip-unneeded "$R/$T/lib/libgcc_s.so.1"
cp "$L/libunixlib.so.5.0.0" "$R/$T/lib/libunixlib.so"
cp "$L/libc.a" "$L/libpthread.a" "$R/$T/lib/"                  # empty archives: -lc and -lpthread find them
cp "$HERE/data/nolibm/libm.so" "$R/$T/lib/libm.so"             # EMPTY: libm.so.1 is a stub and must stay out of DT_NEEDED (see data/riscos-da.c's neighbour, configure-gcc16-native.sh)
cp "$E/lib/libdl.2.0.0.so" "$R/$T/lib/libdl.so"; cp "$E/lib/libdl.a" "$R/$T/lib/libdl.a"
for f in libstdc++.a libsupc++.a libstdc++exp.a libstdc++fs.a; do cp "$L/$f" "$R/$T/lib/"; $ST --strip-debug "$R/$T/lib/$f"; done

# the UnixLib headers; the cross layout has the libstdc++ headers in T/include/c++, the native compiler looks in include/c++/V (the second copy below)
if [ $FORTRAN = 1 ]; then
  cp "$L/libgfortran.a" "$L/libgfortran.spec" "$R/$T/lib/"; $ST --strip-debug "$R/$T/lib/libgfortran.a"
fi
cp -r "$E/$T/include/c++/$V" "$R/include/c++/$V"
cp -r "$E/$T/include" "$R/$T/include"
rm -rf "$R/$T/include/c++"
du -sh "$R"; find "$R" -type f | wc -l
