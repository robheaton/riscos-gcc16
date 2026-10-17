#!/bin/bash
# build-target.sh BINUTILS-SRC GCC-SRC PREFIX WORKDIR: builds binutils 2.45.1 and GCC 16.2.0 for the target aarch64-riscos (a real RISC OS 64 target, not aarch64-none-elf in a wrapper) and installs them in PREFIX, with the SDK's
# start-up files, linker script and system calls and picolibc in PREFIX/aarch64-riscos, so that   aarch64-riscos-gcc hello.c -o hello   makes an AIF64 program (a file you name hello,ff8).
#   BINUTILS-SRC, GCC-SRC = pristine source trees (copies are patched here: the originals are not changed); WORKDIR = a new folder.
#   Needs: the SDK built (sdk/build-sdk.sh: sdk/lib/*.o), picolibc (tools/build-picolibc.sh; PICOLIBC, default /var/tmp/a64/picolibc), the cross archiver of tools/build-a64-toolchain.sh (A64), GMP, MPFR and MPC for the
#   host (HOSTLIBS = their prefix).
# What the patches do: target names for BFD, gas and ld (an emulation aarch64riscos, a copy of aarch64elf for now), a target for GCC (aarch64/riscos.h: __riscos__, __riscos64__, x18 fixed, strict alignment, the
# start files, the link spec with aif64.ld, and objcopy -O binary as the last step) and libgcc.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SDK=$(cd "$HERE/../sdk" && pwd)
BU=$(cd "${1:?binutils source}" && pwd); GCC=$(cd "${2:?gcc source}" && pwd); PREFIX=${3:?prefix}; W=${4:?workdir (new)}
[ -e "$W" ] && { echo "$W exists: give a new folder"; exit 1; }
mkdir -p "$W" "$PREFIX" && W=$(cd "$W" && pwd) && PREFIX=$(cd "$PREFIX" && pwd)
PICO=${PICOLIBC:-/var/tmp/a64/picolibc}; A64=${A64:-/var/tmp/a64/prefix/bin}; H=${HOSTLIBS:?HOSTLIBS = prefix of the host GMP, MPFR and MPC}
J=${JOBS:-$(nproc)}
step () { echo "$(date +%T) $*"; }
step "copy and patch the sources"
cp -a "$BU" "$W/binutils-src" && cp -a "$GCC" "$W/gcc-src"
( cd "$W/binutils-src" && patch -p1 < "$HERE/patches/binutils-2.45.1-aarch64-riscos.patch" > "$W/patch-binutils.log" )
( cd "$W/gcc-src" && patch -p1 < "$HERE/patches/gcc-16.2.0-aarch64-riscos.patch" > "$W/patch-gcc.log" && cp -r "$HERE/new-files/gcc/." gcc/ )
step "binutils"
mkdir -p "$W/binutils-build" && cd "$W/binutils-build"
"$W/binutils-src/configure" --target=aarch64-riscos --prefix="$PREFIX" --disable-nls --disable-werror --disable-gdb --disable-sim --disable-gprof > configure.log 2>&1
make -j"$J" > make.log 2>&1 && make install > install.log 2>&1
export PATH=$PREFIX/bin:$PATH
step "GCC and libgcc"
mkdir -p "$W/gcc-build" && cd "$W/gcc-build"
"$W/gcc-src/configure" --target=aarch64-riscos --prefix="$PREFIX" --with-gmp="$H" --with-mpfr="$H" --with-mpc="$H" --enable-languages=c,c++ --without-headers --with-newlib --disable-nls --disable-shared \
  --disable-threads --disable-libssp --disable-libgomp --disable-libquadmath --disable-libatomic --disable-multilib --disable-bootstrap --disable-werror > configure.log 2>&1
make -j"$J" all-gcc > make-gcc.log 2>&1 && make install-gcc > install-gcc.log 2>&1
make -j"$J" all-target-libgcc > make-libgcc.log 2>&1 && make install-target-libgcc > install-libgcc.log 2>&1
step "the SDK files and picolibc into $PREFIX/aarch64-riscos"
T=$PREFIX/aarch64-riscos
mkdir -p "$T/lib" "$T/include"
cp -a "$PICO/include/." "$T/include/" && cp "$PICO/lib/libc.a" "$PICO/lib/libm.a" "$T/lib/"
cp "$SDK/lib/aifhdr.o" "$SDK/lib/crt0.o" "$SDK/lib/riscos_start.o" "$SDK/lib/riscos_syscalls.o" "$SDK/lib/swi.o" "$T/lib/" && cp "$SDK/aif64.ld" "$T/lib/"
step "the module kit (aarch64-riscos-gcc -mmodule): runtime, C library, linker script, headers, modreloc64"
KIT=$SDK/../kit; OUT=$KIT/../out/kit-lib
[ -f "$OUT/libmodkit64.a" ] || { echo "run kit/build-kit.sh first (out/kit-lib)"; exit 1; }
cp "$OUT/modrt64.o" "$OUT/modswi64.o" "$OUT/libmodkit64.a" "$OUT/libmodcxx64.a" "$KIT/module64.ld" "$KIT/../tools/modreloc64.py" "$T/lib/"
mkdir -p "$T/modkit/include" "$T/modkit/riscos64/include"
cp -a "$KIT/../../modkit/include/." "$T/modkit/include/" && cp -a "$KIT/include/." "$T/modkit/riscos64/include/" && cp "$KIT/modcore64.S" "$T/modkit/riscos64/"
printf '#!/bin/sh\n# modreloc64 [-q] ELF: makes the module image from the linked ELF file, in place (the last step of aarch64-riscos-gcc -mmodule)\nq=; [ "$1" = -q ] && { q=-q; shift; }\nd=$(dirname "$(readlink -f "$0")")\ntmp="$1.tmp.$$"\npython3 -I "$d/../lib/modreloc64.py" $q "$1" "$tmp" && mv "$tmp" "$1"\n' > "$T/bin/modreloc64" && chmod +x "$T/bin/modreloc64"
step "done: $PREFIX/bin/aarch64-riscos-gcc"
