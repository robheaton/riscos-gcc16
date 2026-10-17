#!/bin/bash
# build-target.sh BINUTILS-SRC GCC-SRC PREFIX WORKDIR: builds binutils 2.45.1 and GCC 16.2.0 for the target aarch64-riscos (a real RISC OS 64 target, not aarch64-none-elf in a wrapper) and installs them in PREFIX, with the SDK's
# start-up files, linker script and system calls and picolibc in PREFIX/aarch64-riscos, so that   aarch64-riscos-gcc hello.c -o hello   makes an AIF64 program (a file you name hello,ff8).
#   BINUTILS-SRC, GCC-SRC = pristine source trees (copies are patched here: the originals are not changed); WORKDIR = a new folder.
#   Needs: picolibc (tools/build-picolibc.sh; PICOLIBC, default /var/tmp/a64/picolibc), GMP, MPFR and MPC for the host (HOSTLIBS = their prefix), python3. Nothing else of the older tool chain: the SDK's start-up files
#   and system calls and the module kit are compiled with the compiler that this script has just built.
# What the patches do: target names for BFD, gas and ld (an emulation aarch64riscos, a copy of aarch64elf for now), a target for GCC (aarch64/riscos.h: __riscos__, __riscos64__, x18 fixed, strict alignment, the
# start files, the link spec with aif64.ld, and objcopy -O binary as the last step) and libgcc.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SDK=$(cd "$HERE/../sdk" && pwd)
BU=$(cd "${1:?binutils source}" && pwd); GCC=$(cd "${2:?gcc source}" && pwd); PREFIX=${3:?prefix}; W=${4:?workdir (new)}
[ -e "$W" ] && { echo "$W exists: give a new folder"; exit 1; }
mkdir -p "$W" "$PREFIX" && W=$(cd "$W" && pwd) && PREFIX=$(cd "$PREFIX" && pwd)
PICO=${PICOLIBC:-/var/tmp/a64/picolibc}; H=${HOSTLIBS:?HOSTLIBS = prefix of the host GMP, MPFR and MPC}
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
  --disable-threads --disable-libssp --disable-libgomp --disable-libquadmath --disable-libatomic --disable-multilib --disable-bootstrap --disable-werror \
  --disable-libstdcxx-pch --with-default-libstdcxx-abi=new --disable-libstdcxx-threads --disable-libstdcxx-filesystem-ts --disable-libstdcxx-backtrace > configure.log 2>&1
make -j"$J" all-gcc > make-gcc.log 2>&1 && make install-gcc > install-gcc.log 2>&1
make -j"$J" all-target-libgcc > make-libgcc.log 2>&1 && make install-target-libgcc > install-libgcc.log 2>&1
step "the SDK files and picolibc into $PREFIX/aarch64-riscos (before libstdc++: its configure links test programs)"
T=$PREFIX/aarch64-riscos
mkdir -p "$T/lib" "$T/include"
cp -a "$PICO/include/." "$T/include/" && cp "$PICO/lib/libc.a" "$PICO/lib/libm.a" "$T/lib/"
# the start-up files and the system calls of the SDK (sdk/crt), compiled with the new compiler
CC=$PREFIX/bin/aarch64-riscos-gcc
for f in aifhdr.S crt0.S swi.S riscos_start.c riscos_syscalls.c; do
  $CC -O2 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -c "$SDK/crt/$f" -o "$T/lib/${f%.*}.o"
done
cp "$SDK/aif64.ld" "$T/lib/"
# libriscosstub.a: ENOSYS stubs of functions that only UnixLib 64 provides (one per object, so that libunixlib64.a, searched before it, wins)
mkdir -p "$W/stubs"
for f in "$SDK"/crt/stubs/*.c; do $CC -O2 -ffreestanding -fno-pic -fno-pie -Wall -Wextra -c "$f" -o "$W/stubs/$(basename "${f%.c}").o"; done
"$PREFIX/bin/aarch64-riscos-ar" rcs "$T/lib/libriscosstub.a" "$W"/stubs/*.o
# UnixLib 64 (unixlib/): -munixlib links it
UL=$SDK/../unixlib
CC=$CC AR=$PREFIX/bin/aarch64-riscos-ar "$UL/build.sh" "$W/unixlib" > "$W/unixlib-build.log" 2>&1
cp "$W/unixlib/libunixlib64.a" "$T/lib/" && mkdir -p "$T/include/unixlib" && cp "$UL/include/unixlib/local.h" "$T/include/unixlib/"
step "libstdc++ (C++ for programs)"
make -j"$J" all-target-libstdc++-v3 > make-libstdcxx.log 2>&1 && make install-target-libstdc++-v3 > install-libstdcxx.log 2>&1
step "the module kit (aarch64-riscos-gcc -mmodule): runtime and C library built with the new compiler, linker script, headers, modreloc64, cmunge64"
KIT=$SDK/../kit; OUT=$W/kit-lib
KITCC=$PREFIX/bin/aarch64-riscos-gcc KITAR=$PREFIX/bin/aarch64-riscos-ar KITOUT=$OUT "$KIT/build-kit.sh" > "$W/kit-build.log" 2>&1
cp "$OUT/modrt64.o" "$OUT/modswi64.o" "$OUT/libmodkit64.a" "$OUT/libmodcxx64.a" "$KIT/module64.ld" "$KIT/../tools/modreloc64.py" "$T/lib/"
mkdir -p "$T/modkit/include" "$T/modkit/riscos64/include"
cp -a "$KIT/../../modkit/include/." "$T/modkit/include/" && cp -a "$KIT/include/." "$T/modkit/riscos64/include/" && cp "$KIT/modcore64.S" "$T/modkit/riscos64/"
printf '#!/bin/sh\n# modreloc64 [-q] ELF: makes the module image from the linked ELF file, in place (the last step of aarch64-riscos-gcc -mmodule)\nq=; [ "$1" = -q ] && { q=-q; shift; }\nd=$(dirname "$(readlink -f "$0")")\ntmp="$1.tmp.$$"\npython3 -I "$d/../lib/modreloc64.py" $q "$1" "$tmp" && mv "$tmp" "$1"\n' > "$T/bin/modreloc64" && chmod +x "$T/bin/modreloc64"
# the tools of the kit in PREFIX/share/riscos64, laid out as in the repository (cmunge64 finds modkit/bin/mkmodhdr.py relative to itself), and commands for them in PREFIX/bin
R=$PREFIX/share/riscos64
mkdir -p "$R/riscos64/kit" "$R/modkit/bin"
cp "$KIT/cmunge64.py" "$KIT/module64.mk" "$KIT/modflags.sh" "$R/riscos64/kit/" && cp "$KIT/../../modkit/bin/mkmodhdr.py" "$KIT/../../modkit/bin/mkoslib.py" "$R/modkit/bin/"
printf '#!/bin/sh\nCMUNGE64_CC=%s exec python3 "%s/riscos64/kit/cmunge64.py" "$@"\n' "$PREFIX/bin/aarch64-riscos-gcc" "$R" > "$PREFIX/bin/cmunge64"
printf '#!/bin/sh\nMKOSLIB_NM=%s exec python3 "%s/modkit/bin/mkoslib.py" "$@"\n' "$PREFIX/bin/aarch64-riscos-nm" "$R" > "$PREFIX/bin/mkoslib"
printf '# the make rules for a module: include this file (see the README of riscos64/kit)\ninclude %s/riscos64/kit/module64.mk\n' "$R" > "$R/module64.mk"
chmod +x "$PREFIX/bin/cmunge64" "$PREFIX/bin/mkoslib"
step "done: $PREFIX/bin/aarch64-riscos-gcc"
