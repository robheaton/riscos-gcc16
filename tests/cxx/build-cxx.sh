#!/bin/bash
# Build the C++ regression matrix for RISC OS with GCC 10.2.0 (cx10-*) and GCC 16.2.0 (cx16-*).
# libstdc++ and libgcc are linked statically so the target needs nothing beyond the EABI SharedLibs
# (libunixlib.so.5, libm.so.1).  Output: out/<name>,e1f
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=$HERE/out; mkdir -p "$OUT"
OLDX=${OLDX:-$HOME/gccsdk/env/bin/arm-riscos-gnueabihf-g++}
NEWX=${NEWX:-$HOME/gccsdk-next/env-f/bin/arm-riscos-gnueabihf-g++}
LOG=$OUT/build-cxx.log; : > "$LOG"
COMMON="-std=gnu++17 -g0 -Wall -Wno-unused-parameter -Wno-unused-variable"
FAILED=0
b() { local name=$1 cxx=$2 opt=$3 cfg=$4 src=${5:-cxxtest.cc}; local ver; ver=$($cxx -dumpversion)
  echo "+ $cxx $COMMON $opt cxxtest.cc -> $name" >> "$LOG"
  if $cxx $COMMON $opt "-DROTEST_CFG=\"$cfg gcc$ver\"" "$HERE/$src" -static-libstdc++ -static-libgcc -Wl,--allow-shlib-undefined -lm -o "$OUT/$name,e1f" >> "$LOG" 2>&1; then
    printf "  built  %-10s %s\n" "$name" "$(stat -c %s "$OUT/$name,e1f") bytes"
  else printf "  FAILED %-10s (see out/build-cxx.log)\n" "$name"; FAILED=$((FAILED+1)); fi; }
echo "compilers: OLD=$($OLDX -dumpversion)  NEW=$($NEWX -dumpversion)"
b cx10-O0  $OLDX "-O0"        "cx10 -O0"
b cx10-O2  $OLDX "-O2"        "cx10 -O2"
b cx16-O0  $NEWX "-O0"        "cx16 -O0"
b cx16-O2  $NEWX "-O2"        "cx16 -O2"
b cx16-O3  $NEWX "-O3"        "cx16 -O3"
b cx16-O2p $NEWX "-O2 -fPIC"  "cx16 -O2 -fPIC"
b cx16-20  $NEWX "-O2 -std=gnu++20"  "cx16 -O2 c++20"
b cx16-23  $NEWX "-O2 -std=gnu++23"  "cx16 -O2 c++23"
b cx23     $NEWX "-O2 -std=gnu++23"  "cx16 -O2 c++23 features" cxx23.cc
echo "$FAILED build(s) failed"
# keep unstripped copies (for crash analysis), ship stripped ones
mkdir -p "$OUT/unstripped"
for f in "$OUT"/cx*,e1f; do cp "$f" "$OUT/unstripped/"; ${NEWX%g++}strip --strip-all "$f"; done
ls -la "$OUT"/cx*,e1f | awk '{print $5, $9}' | sed "s|$OUT/||" 
