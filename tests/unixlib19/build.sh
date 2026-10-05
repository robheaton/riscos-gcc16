#!/bin/bash
# Build the programs of the libunixlib 16.2.0-8 tests (a vfork child that ends without exec: libunixlib 16.2.0-8 + SharedUnixLibrary 1.16-vforkfix2) with the GCC 16.2 cross compiler (env-f:
# -fstack-clash-protection is its default).  Sources are shared with tests/unixlib16, 17, 18 and sulfix.   usage: build.sh [OUTDIR]     default: ./out (names end in ,e1f = RISC OS file type)
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
U17=$HERE/../unixlib17; U18=$HERE/../unixlib18; SF=$HERE/../sulfix
OUT=${1:-$HERE/out}
E=${GCCNEXT:-$HOME/gccsdk-next}/env-f/bin
CC=$E/arm-riscos-gnueabihf-gcc
ST=$E/arm-riscos-gnueabihf-strip
mkdir -p "$OUT"
b() { # name source cfg-define...
  local n=$1 src=$2; shift 2
  "$CC" -std=gnu11 -O2 -g0 -Wall -Wextra -Wno-unused-parameter "-DROTEST_CFG=\"$n gcc16.2.0\"" -I"$U17" -I"$HERE" "$@" "$src" -static-libgcc -Wl,--allow-shlib-undefined -lm -o "$OUT/$n,e1f"
  "$ST" --strip-all "$OUT/$n,e1f"
}
b ulinfo    "$HERE/../unixlib16/ulinfo.c"
b daprobe   "$U17/daprobe.c"
b mmaptest  "$U17/mmaptest.c"
b seqtest   "$U17/seqtest.c"
b modver    "$U17/modver.c"
b exittest  "$U18/exittest.c"
b vforkbare "$U18/vforkbare.c"
b vforkfail "$SF/vforkfail.c"
b vforkloop "$SF/vforkloop.c"
b vforkrma  "$SF/vforkrma.c"
b vforktrace "$HERE/vforktrace.c"
b sulmark    "$HERE/sulmark.c"
b vloopt     "$HERE/vloopt.c"
b daprobet   "$HERE/daprobet.c"
b sulfile    "$HERE/sulfile.c"
ls -la "$OUT"
