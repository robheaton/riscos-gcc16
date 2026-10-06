#!/bin/bash
# Build the programs of the libunixlib 16.2.0-12 tests (fix level 14: the .fini_array, getrlimit (RLIMIT_STACK), POSIX semaphores, gcov and profile data) with the GCC 16.2 cross compiler (env-f).
# usage: build.sh [OUTDIR]     default: ./out  (the names end in ,e1f = RISC OS file type ELF when the directory is copied to a RISC OS share)
# The two coverage programs write their .gcda file to the absolute path of the object file they were compiled from (a Linux path); on RISC OS set GCOV_PREFIX (the directory to write in, for
# example .) and GCOV_PREFIX_STRIP (the number of leading directories of that Linux path to cut off: the script prints the number for the OUTDIR it was given).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
G=${GCCNEXT:-$HOME/gccsdk-next}
OUT=$(mkdir -p "${1:-$HERE/out}" && cd "${1:-$HERE/out}" && pwd)
E=$G/env-f/bin
CC=$E/arm-riscos-gnueabihf-gcc
ST=$E/arm-riscos-gnueabihf-strip
F="-std=gnu11 -O2 -g0 -Wall -Wextra -Wno-unused-parameter"
cd "$OUT"
FIX=$HERE/../fixlevel/fixlevel.c                      # the published layout; in the author's work area the source is under recipe/
[ -f "$FIX" ] || FIX=$G/recipe/gcc-16.2.0-riscos/tests/throwback/hw/fixlevel.c
$CC $F -o fixlevel,e1f "$FIX"
$CC $F -o finitest,e1f  "$HERE/finitest.c"
$CC $F -o rlimtest,e1f  "$HERE/rlimtest.c"
$CC $F -DBIGSTACK -o rlimtest64,e1f "$HERE/rlimtest.c"
$CC $F -o semtest,e1f   "$HERE/semtest.c"
# gcov: compile with --coverage (writes covtest.gcno next to the object), link, run on RISC OS (writes covtest.gcda), read with the cross gcov on Linux
$CC $F -O0 --coverage -c -o covtest.o "$HERE/covtest.c"
$CC --coverage -o covtest,e1f covtest.o
# profile-guided optimisation: -fprofile-generate, run, then -fprofile-use
$CC $F -fprofile-generate -c -o pgotest.o "$HERE/covtest.c"
$CC -fprofile-generate -o pgotest,e1f pgotest.o
for p in fixlevel finitest rlimtest rlimtest64 semtest covtest pgotest; do $ST --strip-all "$p,e1f"; done
ls -la "$OUT"
echo "on RISC OS:   *Set GCOV_PREFIX .    *Set GCOV_PREFIX_STRIP $(echo "$OUT" | awk -F/ '{print NF-1}')    before running covtest and pgotest"
