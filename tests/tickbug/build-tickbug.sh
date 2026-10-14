#!/bin/bash
# build-tickbug.sh OUTDIR [TOOLCHAIN] - builds the thread ticker test (tickerbug.c) in the variants of the README: tb-dyn,e1f (dynamic), tb-static,e1f (-static).
# TOOLCHAIN: the tool chain (default: arm-riscos-gnueabihf-gcc on the PATH).  To test a libunixlib that is not in the tool chain yet (the build of build-unixlib.sh), set
# ULBUILD=<its build directory, the one with .libs/libunixlib.a and crt0.o>: the static variant is then linked with that library and that crt0.o.
set -e
OUT=${1:?usage: build-tickbug.sh OUTDIR [TOOLCHAIN]}
TC=${2:-}
HERE=$(cd "$(dirname "$0")" && pwd)
[ -n "$TC" ] && export PATH="$TC/bin:$PATH"
mkdir -p "$OUT"
CC=arm-riscos-gnueabihf-gcc
$CC -O1 -Wall -o "$OUT/tb-dyn,e1f" "$HERE/tickerbug.c" -lpthread
if [ -n "${ULBUILD:-}" ]; then
  $CC -O1 -Wall -static -B"$ULBUILD/" -L"$ULBUILD/.libs" -o "$OUT/tb-static,e1f" "$HERE/tickerbug.c" -lpthread
else
  $CC -O1 -Wall -static -o "$OUT/tb-static,e1f" "$HERE/tickerbug.c" -lpthread
fi
ls -l "$OUT"/tb-*
