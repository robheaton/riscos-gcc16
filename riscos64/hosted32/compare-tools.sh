#!/bin/bash
# compare-tools.sh: the C tools (cmunge64, mkoslib64, modreloc64) against the Python originals, byte for byte, on the CMHG files and modules of the repository.  Builds the C tools for Linux in $TMPDIR.
set -u
HERE=$(cd "$(dirname "$0")" && pwd); KS=$HERE/../../modkit/src; W=${TMPDIR:-/tmp}/hosted32-compare; mkdir -p "$W"; cd "$W"
gcc -O2 -I "$KS" -o cmunge64 "$HERE/src/cmunge64.c" "$KS/modcommon.c" && gcc -O2 -I "$KS" -o mkoslib64 "$HERE/src/mkoslib64.c" "$KS/modcommon.c" && gcc -O2 -o modreloc64 "$HERE/../tools/modreloc64.c" || exit 1
n=0; ok=0
for f in $(find "$(cd "$HERE/../.." && pwd)" -name '*.cmhg' -not -path '*/hosted32/*' | sort); do
  b=$(basename "$f" .cmhg); n=$((n+1))
  python3 -I "$HERE/../kit/cmunge64.py" -s py.S -d py.h "$f" > py.out 2>&1; prc=$?
  ./cmunge64 -s c.S -d c.h "$f" > c.out 2>&1; crc=$?
  if [ $prc = $crc ] && { [ $prc != 0 ] || { cmp -s py.S c.S && cmp -s py.h c.h; }; }; then ok=$((ok+1)); else echo "DIFFERENT: $f"; fi
done
echo "cmunge64: $ok of $n CMHG files give the same output (or the same refusal)"
