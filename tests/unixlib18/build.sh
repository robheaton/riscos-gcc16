#!/bin/bash
# Build the programs of the libunixlib 16.2.0-7 tests (guard against requests that cannot be served, signal stack freed at exit) with the GCC 16.2 cross compiler (env-f: -fstack-clash-protection is
# its default).  Sources are shared with tests/unixlib17 and tests/unixlib16.   usage: build.sh [OUTDIR]     default: ./out (names end in ,e1f = RISC OS file type for the NAS/Samba convention)
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
U17=$HERE/../unixlib17
OUT=${1:-$HERE/out}
E=${GCCNEXT:-$HOME/gccsdk-next}/env-f/bin
CC=$E/arm-riscos-gnueabihf-gcc
ST=$E/arm-riscos-gnueabihf-strip
mkdir -p "$OUT"
b() { # name source cfg-define...
  local n=$1 src=$2; shift 2
  "$CC" -std=gnu11 -O2 -g0 -Wall -Wextra -Wno-unused-parameter "-DROTEST_CFG=\"$n gcc16.2.0\"" -I"$U17" "$@" "$src" -static-libgcc -Wl,--allow-shlib-undefined -lm -o "$OUT/$n,e1f"
  "$ST" --strip-all "$OUT/$n,e1f"
}
b ulinfo   "$HERE/../unixlib16/ulinfo.c"
b daprobe  "$U17/daprobe.c"
b mmaptest "$U17/mmaptest.c"
b seqtest  "$U17/seqtest.c"
b modver   "$U17/modver.c"
b exittest "$HERE/exittest.c"
b vforkbare "$HERE/vforkbare.c"
b moddump "$HERE/moddump.c"
ls -la "$OUT"
