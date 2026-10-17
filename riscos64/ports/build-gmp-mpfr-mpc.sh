#!/bin/bash
# build-gmp-mpfr-mpc.sh SOURCES WORKDIR: builds GMP, MPFR and MPC as static libraries for AArch64 RISC OS with the SDK (sdk/bin/aarch64-riscos-gcc), and their test programs.
#   SOURCES = a folder with gmp-X, mpfr-X and mpc-X (the versions that GCC's contrib/download_prerequisites fetches: gmp-6.1.0, mpfr-3.1.4, mpc-1.0.3 were used)
#   WORKDIR = a new folder (it must not exist): the wrappers, the build folders (gmp/b, mpfr/b, mpc/b) and the installed libraries (inst/ for GMP, MPFR and MPC)
# Needs: the SDK built (sdk/build-sdk.sh) and the cross compiler of tools/build-a64-toolchain.sh in A64 (default /var/tmp/a64/prefix/bin).
# The tests are built last, with -munixlib (UnixLib 64, ../unixlib): their Unix file names ("io.tmp", "tmul.dat") are translated to RISC OS names.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SDK=$(cd "$HERE/../sdk" && pwd)
SRC=$(cd "${1:?SOURCES folder}" && pwd); W=${2:?WORKDIR (new)}
A64=${A64:-/var/tmp/a64/prefix/bin}
[ -e "$W" ] && { echo "$W exists: give a new folder"; exit 1; }
mkdir -p "$W" && W=$(cd "$W" && pwd)
echo "$SRC" > "$W/sources.path"
export PATH=$A64:$PATH
J=${JOBS:-$(nproc)}
GMP=$(cd "$SRC"/gmp-* && pwd); MPFR=$(cd "$SRC"/mpfr-* && pwd); MPC=$(cd "$SRC"/mpc-* && pwd)
# the compiler for the configure scripts of old GNU packages: the file name as asked for (RISCOS64_RAW; the SDK would add ",ff8"), and C of 2016 (GCC 16 makes C23 the default: K&R declarations and implicit int are errors)
cat > "$W/cc64" <<EOT
#!/bin/bash
export RISCOS64_RAW=1
exec $SDK/bin/aarch64-riscos-gcc -std=gnu99 -fpermissive -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types -Wno-implicit-int "\$@"
EOT
# the same with UnixLib 64 (-munixlib), for the test programs: Unix file names.  MPFR's tests have the build directory compiled in as SRCDIR (they look for their data files there): "." instead.
cat > "$W/cc64u" <<EOT
#!/bin/bash
exec $W/cc64 -munixlib "\$@" -USRCDIR '-DSRCDIR="."'
EOT
chmod +x "$W/cc64" "$W/cc64u"
COMMON="--host=aarch64-none-elf --build=$(gcc -dumpmachine) --disable-shared --enable-static CC=$W/cc64 CC_FOR_BUILD=gcc AR=aarch64-none-elf-ar RANLIB=aarch64-none-elf-ranlib"
step () { echo "$(date +%T) $*"; }
# libtool must not find a libgmp.la of the host: the .la files of the installed libraries are removed, the .a files are enough
step "GMP"
mkdir -p "$W/gmp/b" && cd "$W/gmp/b"
"$GMP/configure" $COMMON --prefix="$W/inst" --disable-assembly > configure.log 2>&1
make -j"$J" > make.log 2>&1 && make install > install.log 2>&1
rm -f "$W/inst/lib/libgmp.la"
step "MPFR"
mkdir -p "$W/mpfr/b" && cd "$W/mpfr/b"
"$MPFR/configure" $COMMON --prefix="$W/inst" --with-gmp="$W/inst" > configure.log 2>&1
make -j"$J" > make.log 2>&1 && make install > install.log 2>&1
rm -f "$W/inst/lib/libmpfr.la"
step "MPC"
mkdir -p "$W/mpc/b" && cd "$W/mpc/b"
"$MPC/configure" $COMMON --prefix="$W/inst" --with-gmp="$W/inst" --with-mpfr="$W/inst" > configure.log 2>&1
make -j"$J" > make.log 2>&1 && make install > install.log 2>&1
rm -f "$W/inst/lib/libmpc.la"
step "UnixLib 64 (for -munixlib)"
CC="$SDK/bin/aarch64-riscos-gcc" AR="$A64/aarch64-none-elf-ar" "$HERE/../unixlib/build.sh" > "$W/unixlib.log" 2>&1
step "the test programs (they are only built here; run-tests.sh runs them on RISC OS Pyromaniac)"
# the test makefiles link with libgmp.la and libmpfr.la: stand-ins for the installed ones, made from the build tree's
sed -e "s/^installed=no/installed=yes/" -e "s|^libdir=.*|libdir='$W/inst/lib'|" "$W/gmp/b/libgmp.la" > "$W/inst/lib/libgmp.la"
sed -e "s/^installed=no/installed=yes/" -e "s|^libdir=.*|libdir='$W/inst/lib'|" "$W/mpfr/b/src/libmpfr.la" > "$W/inst/lib/libmpfr.la"
( cd "$W/gmp/b" && make -j"$J" check TESTS= CC="$W/cc64u" > tests.log 2>&1 )
( cd "$W/mpfr/b/tests" && make -j"$J" check TESTS= CC="$W/cc64u" > ../tests.log 2>&1 )
( cd "$W/mpc/b/tests" && make -j"$J" check TESTS= CC="$W/cc64u" > ../tests.log 2>&1 )
step "done: libraries in $W/inst; test programs in $W/{gmp,mpfr,mpc}/b/tests (gmp: tests/mpz, mpq, mpf, misc, ...)"
