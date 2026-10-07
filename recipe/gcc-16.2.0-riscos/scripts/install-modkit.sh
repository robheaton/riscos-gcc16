#!/bin/bash
# Installs modkit (RISC OS relocatable modules without a C library) into a cross tool chain, so that  arm-riscos-gnueabihf-gcc -mmodule  works as it did in GCCSDK 4.7.4:
#   <tc>/lib/gcc/arm-riscos-gnueabihf/<ver>/include-modkit/   the few headers a module needs (kernel.h, string.h ...): the driver puts them before the compiler's own
#   <tc>/arm-riscos-gnueabihf/lib/libmodkit-core.a, libmodkit.a, module.ld   the mini C library, integer division, the SWI veneer (one object per source); libmodkit.a is a linker script that names the library and
#                                                               libgcc (the driver links it last); the linker script of the module (found by the driver, -mmodule links them)
#   <tc>/bin/cmunge, arm-riscos-gnueabihf-modreloc, -mkoslib    the three commands of a module's Makefile (programs of the host, made from modkit/src: C, no Python):
#                                                               cmunge (CMHG file -> header and veneers), mkoslib (the OSLib veneers, made from the objects: replaces -lOSLib32), modreloc (the flat module image)
#   <tc>/arm-riscos-gnueabihf/bin/modreloc                     the link that the driver runs after the link of a module (gcc -mmodule -o Module,ffa makes the flat module image)
#   <tc>/share/riscos-modkit/                                  module.mk, the library sources and the headers (what module.mk needs)
# usage: install-modkit.sh TOOLCHAIN_DIR [MODKIT_DIR]      (MODKIT_DIR: the modkit folder of this repository, found from the place of this script by default; HOSTCC: the host compiler, cc)
set -eu
TC=$(readlink -f "${1:?usage: install-modkit.sh TOOLCHAIN_DIR [MODKIT_DIR]}")
K=$(readlink -f "${2:-$(dirname "$(readlink -f "$0")")/../../../modkit}")
T=arm-riscos-gnueabihf
CC=$TC/bin/$T-gcc; AR=$TC/bin/$T-ar
HOSTCC=${HOSTCC:-cc}
[ -x "$CC" ] && [ -d "$K/lib" ] && [ -d "$K/src" ] || { echo "install-modkit: $TC is not a tool chain or $K is not modkit" >&2; exit 1; }
VER=$("$CC" -dumpversion)
INC=$TC/lib/gcc/$T/$VER/include-modkit
rm -rf "$INC" && mkdir -p "$INC" && cp "$K"/include/*.h "$INC/"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
# the library of the module: built with the compiler of the tool chain, -mmodule
# every lib/*.c (one object each: a module only gets the objects that it uses) and lib/*.S
OBJS=
for src in "$K"/lib/*.c; do f=$(basename "$src" .c); "$CC" -mmodule -O2 -std=gnu99 -Wall -c "$src" -o "$W/$f.o"; OBJS="$OBJS $W/$f.o"; done
for src in "$K"/lib/*.S; do f=$(basename "$src" .S); "$CC" -march=armv6 -c "$src" -o "$W/$f.o"; OBJS="$OBJS $W/$f.o"; done
rm -f "$TC/$T/lib/libmodkit.a" "$TC/$T/lib/libmodkit-core.a"
"$AR" rcs "$TC/$T/lib/libmodkit-core.a" $OBJS
# libmodkit.a is what the driver links last (ENDFILE_SPEC): a linker script, so that libgcc (soft float, 64 bit division) comes after the library without a change of the compiler
printf '%s\n%s\n' '/* libmodkit.a: the C library of modkit (libmodkit-core.a), then libgcc (the soft float and 64 bit division routines): gcc -mmodule links this file after the objects of the module. */' 'GROUP ( libmodkit-core.a -lgcc )' > "$TC/$T/lib/libmodkit.a"
cp "$K/lib/module.ld" "$TC/$T/lib/module.ld"
# the tools (programs of the host)
for t in cmunge modreloc mkoslib; do "$HOSTCC" -O2 -Wall -o "$W/$t" "$K/src/$t.c" "$K/src/modcommon.c"; done
install -m 755 "$W/cmunge" "$TC/bin/cmunge"
install -m 755 "$W/modreloc" "$TC/bin/$T-modreloc"
install -m 755 "$W/mkoslib" "$TC/bin/$T-mkoslib"
ln -sf ../../bin/$T-modreloc "$TC/$T/bin/modreloc"
# what module.mk needs
S=$TC/share/riscos-modkit
rm -rf "$S" && mkdir -p "$S/lib" "$S/include"
cp "$K"/lib/*.c "$K"/lib/*.S "$K"/lib/module.ld "$S/lib/"
cp "$K"/include/*.h "$S/include/"
cp "$K/module.mk" "$S/module.mk"
echo "modkit installed in $TC (gcc $VER): libmodkit-core.a $(stat -c %s "$TC/$T/lib/libmodkit-core.a") bytes (+ libmodkit.a: the script that adds libgcc), cmunge, $T-modreloc, $T-mkoslib"
