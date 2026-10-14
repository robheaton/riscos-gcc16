#!/bin/bash
# build-pthticker.sh [OUTDIR] - the PThreadTicker module, from libunixlib/module/pthticker.s of the patched UnixLib (patches-unixlib/unixlib-thread-ticker.patch), after Andrew Youll's
# riscos-unixlib (https://github.com/adyoull/riscos-unixlib, PThreadTicker 0.03).  Run it after build-unixlib.sh.  The module is optional: UnixLib runs the ticker routines from a copy in the
# RMA block when it is not loaded, and from the module when it is (RMLoad it before the program starts: the !Run of the program can do "RMEnsure PThreadTicker 0.03 RMLoad ...").
# It is a flat binary module (no relocation table: position independent).  Result: OUTDIR/PThreadTicker (default $W/PThreadTicker, file type &FFA).
set -e
W=${W:-$HOME/gccsdk-next/unixlib}
N=${N:-$HOME/gccsdk-next/env-f}
OUT=${1:-$W}
R=$W/root/libunixlib
[ -f "$R/module/pthticker.s" ] || { echo "build-pthticker: $R/module/pthticker.s is not there: run build-unixlib.sh first (the thread ticker patch)" >&2; exit 1; }
mkdir -p "$OUT"
"$N/bin/arm-riscos-gnueabihf-gcc" -xassembler-with-cpp -isystem "$R/include" -I "$R/incl-local" -D__UNIXLIB_CHUNKED_STACK=0 -g -O2 -c "$R/module/pthticker.s" -o "$OUT/pthticker.o"
"$N/bin/arm-riscos-gnueabihf-strip" -O binary -o "$OUT/PThreadTicker" "$OUT/pthticker.o"
rm -f "$OUT/pthticker.o"
python3 - "$OUT/PThreadTicker" <<'PY'
import struct, sys
d = open(sys.argv[1], "rb").read()
w = struct.unpack("<9I", d[:36])
t = d[w[4]:d.index(b"\0", w[4])]; h = d[w[5]:d.index(b"\0", w[5])]
if t != b"PThreadTicker": sys.exit("build-pthticker: unexpected module title %r" % t)
print("PThreadTicker: %d bytes, title %s, help %r" % (len(d), t.decode(), h.decode("latin-1")))
PY
