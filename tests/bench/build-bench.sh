#!/bin/bash
# Build the benchmark + ARMv8 (Cortex-A72) correctness variants for RISC OS with GCC 10.2.0 and 16.2.0.
#   out/b10-O2 b16-O2 b16-O3            generic ARMv7-A / VFPv3 (the toolchain default)
#   out/b10-A72-O2 b16-A72-O2 b16-A72-O3  -mcpu=cortex-a72 -mfpu=fp-armv8 (Pi 3/4/CM4: hardware divide, vrint*, vmaxnm, FMA)
#   out/r10-A72-O2 r16-A72-O2 r16-A72-O3  the C regression suite (rotest, strict unwind) built the same way
#   out/cx16-A72-O2                       the C++ regression suite built the same way
# -ffp-contract=off keeps every kernel's checksum identical across builds.
set -u
HERE=$(cd "$(dirname "$0")" && pwd); R=$HERE/../rotest; X=$HERE/../cxx
OUT=$HERE/out; mkdir -p "$OUT" "$HERE/obj"
OLD=${OLD:-$HOME/gccsdk/env/bin/arm-riscos-gnueabihf-gcc}; NEW=${NEW:-$HOME/gccsdk-next/env-f/bin/arm-riscos-gnueabihf-gcc}
OLDX=${OLD%gcc}g++; NEWX=${NEW%gcc}g++
A72="-mcpu=cortex-a72 -mfpu=fp-armv8"
bench() { local name=$1 cc=$2 flags=$3; local ver; ver=$($cc -dumpversion)
  if $cc -std=gnu11 -g0 -Wall -ffp-contract=off $flags -include "$HERE/expected.h" "-DROTEST_CFG=\"$name gcc$ver\"" "$HERE/bench.c" -lm -o "$OUT/$name,e1f" 2> "$HERE/obj/$name.err"; then
    printf "  built  %-12s %s bytes\n" $name "$(stat -c %s "$OUT/$name,e1f")"; else printf "  FAILED %-12s %s\n" $name "$(head -c 200 "$HERE/obj/$name.err")"; fi; }
rot() { local name=$1 cc=$2 flags=$3; local ver d; ver=$($cc -dumpversion); d=$HERE/obj/$name; mkdir -p "$d"
  local cf="-std=gnu11 -g0 -Wall -Wno-unused-parameter -Wno-sign-compare -Wno-infinite-recursion -funwind-tables -DUNWIND_STRICT -ffp-contract=off $flags"
  local extra=""; [ "$cc" = "$NEW" ] && extra="-DHAVE_C23"
  $cc $cf "-DROTEST_CFG=\"$name unwind-tables gcc$ver\"" $extra -c "$R/rotest.c" -o "$d/rotest.o" && $cc $cf -c "$R/callee.c" -o "$d/callee.o" && $cc $cf -c "$R/picdata.c" -o "$d/picdata.o" || { echo "  FAILED $name"; return; }
  local objs="$d/rotest.o $d/callee.o $d/picdata.o"
  if [ -n "$extra" ]; then $cc $cf -std=gnu23 -c "$R/c23.c" -o "$d/c23.o" && objs="$objs $d/c23.o"; fi
  $cc $flags $objs -static-libgcc -Wl,--allow-shlib-undefined -lm -o "$OUT/$name,e1f" && printf "  built  %-12s %s bytes\n" $name "$(stat -c %s "$OUT/$name,e1f")"; }
echo "compilers: OLD=$($OLD -dumpversion) NEW=$($NEW -dumpversion)"
bench b10-O2      $OLD "-O2"; bench b16-O2      $NEW "-O2"; bench b16-O3      $NEW "-O3"
bench b10-A72-O2  $OLD "-O2 $A72"; bench b16-A72-O2  $NEW "-O2 $A72"; bench b16-A72-O3  $NEW "-O3 $A72"
rot r10-A72-O2 $OLD "-O2 $A72"; rot r16-A72-O2 $NEW "-O2 $A72"; rot r16-A72-O3 $NEW "-O3 $A72"
# C++ suite with ARMv8 flags (static libstdc++/libgcc, as in tests/cxx)
if $NEWX -std=gnu++17 -g0 -Wall -Wno-unused-parameter -Wno-unused-variable -O2 $A72 -ffp-contract=off "-DROTEST_CFG=\"cx16-A72-O2 gcc$($NEWX -dumpversion)\"" "$X/cxxtest.cc" -static-libstdc++ -static-libgcc -Wl,--allow-shlib-undefined -lm -o "$OUT/cx16-A72-O2,e1f" 2> "$HERE/obj/cxa72.err"; then
  ${NEW%gcc}strip --strip-all "$OUT/cx16-A72-O2,e1f"; printf "  built  %-12s %s bytes\n" cx16-A72-O2 "$(stat -c %s "$OUT/cx16-A72-O2,e1f")"; else echo "  FAILED cx16-A72-O2: $(head -c 200 "$HERE/obj/cxa72.err")"; fi
# LTO variants (GCC 16.2 only: the 10.2.0 recipe built without LTO).  Needs the LTO-enabled build of the compiler.
bench b16-O3-lto      $NEW "-O3 -flto"
bench b16-A72-O3-lto  $NEW "-O3 -flto $A72"
rot   r16-LTO-O2      $NEW "-O2 -flto"
if $NEWX -std=gnu++17 -g0 -Wall -Wno-unused-parameter -Wno-unused-variable -O2 -flto -ffp-contract=off "-DROTEST_CFG=\"cx16-LTO-O2 gcc$($NEWX -dumpversion)\"" "$X/cxxtest.cc" -static-libstdc++ -static-libgcc -Wl,--allow-shlib-undefined -lm -o "$OUT/cx16-LTO-O2,e1f" 2> "$HERE/obj/cxlto.err"; then
  ${NEW%gcc}strip --strip-all "$OUT/cx16-LTO-O2,e1f"; printf "  built  %-12s %s bytes\n" cx16-LTO-O2 "$(stat -c %s "$OUT/cx16-LTO-O2,e1f")"; else echo "  FAILED cx16-LTO-O2: $(head -c 300 "$HERE/obj/cxlto.err")"; fi
# Obey script: correctness first (a failing A72 build would show up before the timings), then the benchmarks
{ echo '| RunAll - ARMv8 (Cortex-A72) correctness checks, then the benchmarks.  Output: <RoTest$Dir>.Results'
  echo '| (the first EABI program run changes <Obey$Dir>, so remember our directory first)'
  echo 'Set RoTest$Dir <Obey$Dir>'; echo 'Spool <RoTest$Dir>.Results'
  for n in r10-A72-O2 r16-A72-O2 r16-A72-O3 cx16-A72-O2 r16-LTO-O2 cx16-LTO-O2 b10-O2 b16-O2 b16-O3 b16-O3-lto b10-A72-O2 b16-A72-O2 b16-A72-O3 b16-A72-O3-lto; do echo "Echo ===================== $n"; echo "Run <RoTest\$Dir>.$n"; done
  echo 'Spool'; echo 'Echo Finished. Output is in <RoTest$Dir>.Results'; } > "$OUT/RunAll,feb"
