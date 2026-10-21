#!/bin/bash
# build-oracle.sh [OUTDIR]: compiles UnixLib's own test programs (the 32-bit sources, libunixlib/test/*/*.c, run with --direct: no fork) for AArch64 with
# the aarch64-riscos tool chain and -munixlib.  Skips scl (the Shared C Library is not supported).  Writes OUTDIR/NAME,ff8 for every test that builds, OUTDIR/build.log and
# OUTDIR/build-summary.txt (NAME ok | NAME FAIL first error line).  NAME = directory_file.
set -u
T=${TESTDIR:-/home/robh/gccsdk-next/unixlib/root/libunixlib/test}
P=${PREFIX:-/var/tmp/a64/target7-inst}
O=${1:-/var/tmp/a64/oracle}
mkdir -p "$O/obj" "$O/log"
: > "$O/build-summary.txt"
export T P O
one() {
  f=$1; d=$(basename "$(dirname "$f")"); b=$(basename "$f" .c); n=${d}_$b
  if "$P/bin/aarch64-riscos-gcc" -munixlib -O1 -w -D_GNU_SOURCE -include string.h -include stdlib.h -include unistd.h -include sys/wait.h -I"$T" -I"$(dirname "$f")" -o "$O/obj/$n,ff8" "$f" -lm > "$O/log/$n.txt" 2>&1; then
    echo "$n ok" >> "$O/build-summary.txt"
  else
    echo "$n FAIL $(grep -a -m1 'error\|undefined' "$O/log/$n.txt" | cut -c1-150)" >> "$O/build-summary.txt"
  fi
}
export -f one
find "$T" -mindepth 2 -name '*.c' -not -path '*/scl/*' | sort | xargs -P 8 -I{} bash -c 'one {}'
sort -o "$O/build-summary.txt" "$O/build-summary.txt"
echo "built: $(grep -c ' ok$' "$O/build-summary.txt")  failed: $(grep -c ' FAIL ' "$O/build-summary.txt")  of $(wc -l < "$O/build-summary.txt")"
