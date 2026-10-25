#!/bin/bash
# stage-tree.sh WORK: stages the AArch64 cross compiler that runs on 32 bit RISC OS (host arm-riscos-gnueabihf, target aarch64-riscos) as a tree in the RISC OS layout.
#   WORK has  build-gcc2 (all-gcc built), build-bu (binutils built), gcc-src, and the Linux-hosted AArch64 tool chain is T64 (default /var/tmp/a64/target7-inst: its libgcc, UnixLib 64, headers and libraries are copied).
#   Output: WORK/tree (Unix layout, executables named ,e1f) and WORK/tree-ro/gcc16a64r (RISC OS layout: foo.h -> h/foo ...).
set -eu
W=${1:?WORK directory}
T64=${T64:-/var/tmp/a64/target7-inst}
E=${ENVF:-$HOME/gccsdk-next/env-f}
HERE=$(cd "$(dirname "$0")" && pwd)
ST=$E/bin/arm-riscos-gnueabihf-strip
export PATH=$E/bin:/usr/bin:/bin:/usr/local/bin
cd "$W"
# the specs file of the driver is made by running the driver, which cannot run here: the Linux-hosted compiler's is the same text
$T64/bin/aarch64-riscos-gcc -dumpspecs > build-gcc2/gcc/specs; touch build-gcc2/gcc/specs
rm -rf inst; mkdir inst
make -C build-gcc2 MAKEINFO=true -k install-gcc DESTDIR="$W/inst" > inst-gcc.log 2>&1 || true
make -C build-bu MAKEINFO=true install-binutils install-gas install-ld DESTDIR="$W/inst" > inst-bu.log 2>&1
D=inst/GCC16a64
rsync -a --exclude='*.py' --exclude=bin --exclude=ldscripts --exclude='*.la' --exclude='*-gdb.py' $T64/aarch64-riscos/ $D/aarch64-riscos/
rsync -a $T64/lib/gcc/aarch64-riscos/ $D/lib/gcc/aarch64-riscos/
rm -rf tree; mkdir tree; cp -a $D tree/gcc16a64
cd tree/gcc16a64
rm -rf share lib/gcc/aarch64-riscos/16.2.0/plugin lib/gcc/aarch64-riscos/16.2.0/install-tools libexec/gcc/aarch64-riscos/16.2.0/install-tools lib/bfd-plugins
for f in $(find . -type f); do
  if file "$f" | grep -q "ELF 32-bit LSB executable, ARM"; then $ST --strip-all -o "$f,e1f" "$f"; rm "$f"; fi
done
for f in $(find aarch64-riscos/lib lib/gcc -name '*.a' -o -name '*.o'); do $T64/bin/aarch64-riscos-strip -g "$f" 2>/dev/null || true; done
# modreloc64 (C, no Python) for the modules
$E/bin/arm-riscos-gnueabihf-gcc -O2 -o "$W/modreloc64.elf" "$HERE/../tools/modreloc64.c"
$ST --strip-all -o aarch64-riscos/bin/modreloc64,e1f "$W/modreloc64.elf"
# cmunge64 and mkoslib (C, no Python): the module tools of the kit
KS=$HERE/../../modkit/src
$E/bin/arm-riscos-gnueabihf-gcc -O2 -I "$KS" -o "$W/cmunge64.elf" "$HERE/src/cmunge64.c" "$KS/modcommon.c"
$E/bin/arm-riscos-gnueabihf-gcc -O2 -I "$KS" -o "$W/mkoslib64.elf" "$HERE/src/mkoslib64.c" "$KS/modcommon.c"
$ST --strip-all -o bin/cmunge64,e1f "$W/cmunge64.elf"
$ST --strip-all -o bin/mkoslib,e1f "$W/mkoslib64.elf"
cp "$HERE/Setup,feb" .
mkdir -p tests/c; cp "$HERE/tests/T1,feb" tests/; cp "$HERE/tests/hello.c" tests/c/hello
cd "$W"
rm -rf tree-ro; python3 "$HERE/make-ro-layout.py" tree/gcc16a64 tree-ro/gcc16a64r
echo "staged: $W/tree-ro/gcc16a64r"
