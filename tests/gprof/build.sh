#!/bin/bash
# build.sh [TOOLCHAIN_DIR]  -- cross-builds the gprof test program twice: pgtest,e1f with -pg and pgtest-plain,e1f without (the control: it must not write gmon.out).
# TOOLCHAIN_DIR = an unpacked cross toolchain (default: the arm-riscos-gnueabihf-gcc on PATH).  The programs need the runtime 16.2.0-13 or later on RISC OS.
set -eu
TC=${1:+$(readlink -f "$1")/bin/}
T=${TC:-}arm-riscos-gnueabihf
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${OUT:-$HERE/out}
mkdir -p "$OUT"
$T-gcc -O1 -pg -o "$OUT/pgtest,e1f" "$HERE/pgtest.c"
$T-gcc -O1 -o "$OUT/pgtest-plain,e1f" "$HERE/pgtest.c"
echo "built $OUT/pgtest,e1f and $OUT/pgtest-plain,e1f"
echo "on RISC OS:   pgtest 600     (6 seconds; writes gmon.out = the file gmon/out)      then on Linux:   $T-gprof pgtest gmon.out"
