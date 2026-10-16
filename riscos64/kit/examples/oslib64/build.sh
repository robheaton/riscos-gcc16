#!/bin/bash
# build.sh [OUTDIR]: the OSLib test module of the 32-bit kit (modkit/tests/oslib/oslibtest.c) as a 64-bit module: OSLib's X and non-X functions through the veneers that modkit/bin/mkoslib.py writes from OSLib's headers
# (OSLIB=<the folder that holds oslib/*.h>, default ~/gccsdk/env/include).  Run:  Set OlT$Var hello there / RMLoad OlTest / OlTest / RMKill OlTest.
set -e
HERE=$(cd "$(dirname "$0")" && pwd); R64=$HERE/../../..; K=$R64/../modkit
OUT=${1:-$R64/out/oslib64}; INC=${OSLIB:-$HOME/gccsdk/env/include}
A64=${A64:-/var/tmp/a64/prefix/bin}
CC="$R64/sdk/bin/aarch64-riscos-gcc -mmodule-fp -O2 -w -isystem $INC"
mkdir -p "$OUT" && cp "$K/tests/oslib/oslibtest.cmhg" "$OUT/"
python3 "$R64/kit/cmunge64.py" -d "$OUT/header.h" -o "$OUT/hdr.o" "$OUT/oslibtest.cmhg"
$CC -I "$OUT" -c "$K/tests/oslib/oslibtest.c" -o "$OUT/oslibtest.o"
MKOSLIB_NM=$A64/aarch64-none-elf-nm python3 "$K/bin/mkoslib.py" -I "$INC/oslib" -o "$OUT/veneers.c" --from-objects "$OUT/oslibtest.o"
$CC -I "$OUT" -c "$OUT/veneers.c" -o "$OUT/veneers.o"
$CC "$OUT/oslibtest.o" "$OUT/veneers.o" "$OUT/hdr.o" -o "$OUT/OlTest"
