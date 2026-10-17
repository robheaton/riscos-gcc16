#!/bin/bash
# build.sh [OUTDIR]: builds libunixlib64.a, UnixLib 64, with the aarch64-riscos compiler that is on PATH (target/build-target.sh).   The library is OUTDIR/libunixlib64.a (default ./out).
# A program uses it with:   aarch64-riscos-gcc prog.c -Wl,-u,__ul64_init -lunixlib64 -o prog,ff8     (the -u pulls the library in: the SDK's start-up code refers to __ul64_init only weakly)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-$HERE/out}
mkdir -p "$OUT/o"
CC=${CC:-aarch64-riscos-gcc}; AR=${AR:-aarch64-riscos-ar}
for f in riscosify unixify control init dirent cwd statvfs; do
  $CC -O2 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-implicit-fallthrough -I"$HERE/include" -c "$HERE/src/$f.c" -o "$OUT/o/$f.o"
done
rm -f "$OUT/libunixlib64.a"
$AR rcs "$OUT/libunixlib64.a" "$OUT"/o/*.o
ls -l "$OUT/libunixlib64.a"
