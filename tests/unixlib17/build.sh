#!/bin/bash
# Build the programs of the libunixlib 16.2.0-6 tests (stack size, heap fallback) with the GCC 16.2 cross compiler (env-f: -fstack-clash-protection is its default).
# usage: build.sh [OUTDIR]     default: ./out (made fresh; names end in ,e1f = RISC OS file type for the NAS/Samba convention)
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-$HERE/out}
E=${GCCNEXT:-$HOME/gccsdk-next}/env-f/bin
CC=$E/arm-riscos-gnueabihf-gcc
ST=$E/arm-riscos-gnueabihf-strip
mkdir -p "$OUT"
b() { # name source cfg-define...
  local n=$1 src=$2; shift 2
  "$CC" -std=gnu11 -O2 -g0 -Wall -Wextra -Wno-unused-parameter "-DROTEST_CFG=\"$n gcc16.2.0\"" "$@" "$HERE/$src" -static-libgcc -Wl,--allow-shlib-undefined -lm -o "$OUT/$n,e1f"
  "$ST" --strip-all "$OUT/$n,e1f"
}
b ulinfo   ../unixlib16/ulinfo.c
b stk-def  stkinfo.c
b stk-16m  stkinfo.c -DSTACK_MB=16
b stk-64m  stkinfo.c -DSTACK_MB=64
b stk-200m stkinfo.c -DSTACK_MB=200
b stk-1g   stkinfo.c -DSTACK_MB=1024
b heap-def  heapinfo.c
b heap-512m heapinfo.c -DHEAPMAX_MB=512
b heap-1g   heapinfo.c -DHEAPMAX_MB=1024
b heap-3g   heapinfo.c -DHEAPMAX_MB=3072
b daprobe  daprobe.c
b mmaptest mmaptest.c
b seqtest  seqtest.c
b modver   modver.c
b chain     chain.c -DHEAPMAX_MB=512
b chaindeep chain.c -DHEAPMAX_MB=512 -DDEEP=1
b chainstk  chain.c -DHEAPMAX_MB=16 -DSTACK_MB=48 -DAUTOLEVELS=1 -DDEEP=1
ls -la "$OUT"
