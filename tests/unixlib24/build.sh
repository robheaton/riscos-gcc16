#!/bin/bash
# Build the programs of the libunixlib 16.2.0-11 tests (scanf with ll / hh / j / z / t: fix level 13) with the GCC 16.2 cross compiler (env-f).  The other programs (modver, mmaptest, exittest, seqtest, daprobe,
# vforkheap, fixlevel) are the ones of the earlier packs (tests/unixlib19/out, tests/upstream20/out, pack throwback22).   usage: build.sh [OUTDIR]     default: ./out (names end in ,e1f = RISC OS file type)
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
G=${GCCNEXT:-$HOME/gccsdk-next}
OUT=${1:-$HERE/out}
E=$G/env-f/bin
CC=$E/arm-riscos-gnueabihf-gcc
ST=$E/arm-riscos-gnueabihf-strip
mkdir -p "$OUT"
b() { # name source cfg-define...
  local n=$1 src=$2; shift 2
  "$CC" -std=gnu11 -O2 -g0 -Wall -Wextra -Wno-unused-parameter "-DROTEST_CFG=\"$n gcc16.2.0\"" -I"$G/tests/unixlib17" -I"$G/tests/unixlib19" "$@" "$src" -static-libgcc -Wl,--allow-shlib-undefined -lm -o "$OUT/$n,e1f"
  "$ST" --strip-all "$OUT/$n,e1f"
}
b ulinfo     "$G/tests/unixlib16/ulinfo.c"
b scantest   "$G/docs/upstream/repro/scanf/scantest.c"
b scanfcheck "$G/docs/upstream/repro/scanf/scanfcheck.c" -Wno-format-security
cp -p "$G/docs/upstream/repro/scanf/scantab" "$OUT/scantab"
ls -la "$OUT"
