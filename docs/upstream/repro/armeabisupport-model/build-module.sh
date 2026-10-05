#!/bin/bash
# Build the ARMEABISupport module from a source directory with the GCCSDK 4.7.4 module tool chain (the only one that builds RISC OS modules) and CMunge.
#   usage: build-module.sh SRCDIR VERSIONSTRING OUTFILE        e.g.  build-module.sh ~/gccsdk/gcc4/riscos/armeabisupport 1.05-orig ARMEABISupport-1.05-orig,ffa
# The numeric version stays 1.05 (RMEnsure); VERSIONSTRING is what the module's help string says.  The sources are copied; nothing in SRCDIR is touched.
set -eu
SRC=$(cd "$1" && pwd); VER=$2; OUT=$(readlink -f "$3")
G=${GCCSDK:-$HOME/gccsdk}
export PATH=$G/env/bin:$G/cross/bin:$PATH
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
cp "$SRC"/*.c "$SRC"/*.h "$SRC"/*.s "$SRC"/armeabisupport.cmhg "$W"/ 2>/dev/null || cp "$SRC"/*.c "$SRC"/*.h "$SRC"/*.s "$W"/
[ -f "$SRC/armeabisupport.cmhg" ] && cp "$SRC/armeabisupport.cmhg" "$W/"
cd "$W"
rm -f armeabisupport.h
cmunge -tgcc -32bit -p -DPACKAGE_VERSION=$VER -d armeabisupport.h armeabisupport.cmhg >/dev/null
cmunge -tgcc -32bit -p -DPACKAGE_VERSION=$VER -o armeabisupport.o armeabisupport.cmhg >/dev/null
for f in link_list memory swihandler init-fini main swi command abort stack mmap shm; do arm-unknown-riscos-gcc -O3 -mmodule -Wall -std=gnu99 -c $f.c -o $f.o; done
for f in swi-asm abort-asm stack-asm mmap-asm; do arm-unknown-riscos-gcc -O3 -mmodule -Wall -std=gnu99 -xassembler-with-cpp -c $f.s -o $f.o; done
arm-unknown-riscos-gcc -mmodule -o "$OUT" abort-asm.o abort.o armeabisupport.o command.o init-fini.o link_list.o main.o memory.o mmap-asm.o mmap.o shm.o stack-asm.o stack.o swi-asm.o swi.o swihandler.o
echo "built $OUT ($(stat -c %s "$OUT") bytes): $(strings -a "$OUT" | grep 'GCCSDK Dev' | head -1)"
