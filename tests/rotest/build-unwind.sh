#!/bin/bash
# Strict unwind probe: rotest built with -funwind-tables, unwind group promoted from INFO to CHECK.
# Output: out/u10-O0 u10-O2 u16-O0 u16-O2 (,e1f) + RunUnwind,feb   (does not touch the other outputs)
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=$HERE/out; OBJ=$HERE/obj-unwind; mkdir -p "$OUT" "$OBJ"
OLD=${OLD:-$HOME/gccsdk/env/bin/arm-riscos-gnueabihf-gcc}
NEW=${NEW:-$HOME/gccsdk-next/env-f/bin/arm-riscos-gnueabihf-gcc}
COMMON="-std=gnu11 -g0 -Wall -Wno-unused-parameter -Wno-sign-compare -Wno-infinite-recursion -funwind-tables -DUNWIND_STRICT"
for tag in 10 16; do
  if [ $tag = 10 ]; then CC=$OLD; else CC=$NEW; fi
  ver=$($CC -dumpversion); extra=""; [ $tag = 16 ] && extra="-DHAVE_C23"
  for opt in -O0 -O2; do
    n=u$tag$opt; d=$OBJ/$n; mkdir -p "$d"
    $CC $COMMON $opt "-DROTEST_CFG=\"$n unwind-tables gcc$ver\"" $extra -c "$HERE/rotest.c" -o "$d/rotest.o" || continue
    $CC $COMMON $opt -c "$HERE/callee.c" -o "$d/callee.o" && $CC $COMMON $opt -c "$HERE/picdata.c" -o "$d/picdata.o" || continue
    objs="$d/rotest.o $d/callee.o $d/picdata.o"
    if [ -n "$extra" ]; then $CC $COMMON $opt -std=gnu23 -c "$HERE/c23.c" -o "$d/c23.o" && objs="$objs $d/c23.o"; fi
    $CC $opt $objs -static-libgcc -Wl,--allow-shlib-undefined -lm -o "$OUT/$n,e1f" && printf "  built  %-8s %s bytes\n" $n "$(stat -c %s "$OUT/$n,e1f")"
  done
done
printf '%s\n' '| RunUnwind - strict unwinder probe (C code built with -funwind-tables)' \
  'Set RoTest$Dir <Obey$Dir>' 'Spool <RoTest$Dir>.ResultsUnwind' \
  'Echo ===================== u10-O0' 'Run <RoTest$Dir>.u10-O0' 'Echo ===================== u10-O2' 'Run <RoTest$Dir>.u10-O2' \
  'Echo ===================== u16-O0' 'Run <RoTest$Dir>.u16-O0' 'Echo ===================== u16-O2' 'Run <RoTest$Dir>.u16-O2' \
  'Spool' 'Echo Finished. Output is in <RoTest$Dir>.ResultsUnwind' > "$OUT/RunUnwind,feb"
