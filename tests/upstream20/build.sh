#!/bin/bash
# Build the programs of the upstream20 pack (the reproducer of the report "the heap of a vfork + exec child grows over the saved parent") with the GCC 16.2 cross compiler of the forward-port (env-f).
#   usage: build.sh [OUTDIR]     default ./out (names end in ,e1f = RISC OS file type)
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-$HERE/out}
E=${GCCNEXT:-$HOME/gccsdk-next}/env-f/bin
mkdir -p "$OUT"
"$E/arm-riscos-gnueabihf-gcc" -std=gnu11 -O2 -g0 -Wall -Wextra "$HERE/vforkheap.c" -static-libgcc -Wl,--allow-shlib-undefined -o "$OUT/vforkheap,e1f"
"$E/arm-riscos-gnueabihf-strip" --strip-all "$OUT/vforkheap,e1f"
cp -p "$OUT/vforkheap,e1f" "$OUT/vforkheapc,e1f"       # the same program under another name: vforkheapc\$Heap applies to the child only
ls -la "$OUT"
