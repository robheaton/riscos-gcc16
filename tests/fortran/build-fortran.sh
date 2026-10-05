#!/bin/bash
# Build the Fortran regression programs for RISC OS with the GCC 16.2 cross compiler (and, as a reference, GCC 10.2.0's gfortran).
#   build-fortran.sh [PREFIX=$HOME/gccsdk-next/env-f] [OUTDIR=./out]
# Output: OUTDIR/{fcore,fmath,fio,fmisc,fcinterop,ferr}<16|10>-<variant>,e1f  (libgfortran and libgcc linked statically)
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
P=${1:-$HOME/gccsdk-next/env-f}
OUT=${2:-$HERE/out}
GF16=$P/bin/arm-riscos-gnueabihf-gfortran
CC16=$P/bin/arm-riscos-gnueabihf-gcc
ST16=$P/bin/arm-riscos-gnueabihf-strip
GF10=$HOME/gccsdk/env/bin/arm-riscos-gnueabihf-gfortran
CC10=$HOME/gccsdk/env/bin/arm-riscos-gnueabihf-gcc
ST10=$HOME/gccsdk/env/bin/arm-riscos-gnueabihf-strip
W=$OUT/obj
mkdir -p "$OUT" "$W"
LOG=$OUT/build.log; : > "$LOG"
SL16="-static-libgfortran -static-libgcc -Wl,--allow-shlib-undefined"
SL10="-static-libgfortran -static-libgcc"
ok() { printf "  built  %-22s %s bytes\n" "$1" "$(stat -c %s "$2")"; }
# one program: name src... ; compiler/flags from the environment of the caller (GF, FL, SL, ST, TAG)
fbuild() { # outname opt-flags extra-link-objs src...
  local out=$1 fl=$2 extra=$3; shift 3
  if $GF $FL $fl "$@" $extra $SL -o "$OUT/$out,e1f" >> "$LOG" 2>&1; then $ST --strip-all "$OUT/$out,e1f"; ok "$out" "$OUT/$out,e1f"; else echo "  FAILED $out (see build.log)"; fi
}
for v in 16 10; do
  if [ $v = 16 ]; then GF=$GF16; CC=$CC16; SL="$SL16"; ST=$ST16; else GF=$GF10; CC=$CC10; SL="$SL10"; ST=$ST10; fi
  [ -x "$GF" ] || { echo "no $GF"; continue; }
  FL="-std=gnu -g0 -ffree-line-length-none -Wno-integer-division -Wno-unused-variable"
  echo "== gfortran $($GF -dumpversion)"
  $CC -std=gnu11 -O2 -g0 -c "$HERE/cside.c" -o "$W/cside$v.o" >> "$LOG" 2>&1
  for o in O0 O2 O3; do fbuild "fcore$v-$o" "-$o" "" "$HERE/chk.f90" "$HERE/fcore.f90"; done
  fbuild "fcore$v-O2chk" "-O2 -fcheck=all" "" "$HERE/chk.f90" "$HERE/fcore.f90"
  fbuild "fmath$v-O2"      "-O2" "" "$HERE/chk.f90" "$HERE/fmath.f90"
  fbuild "fio$v-O2"        "-O2" "" "$HERE/chk.f90" "$HERE/fio.f90"
  fbuild "fmisc$v-O2"      "-O2" "" "$HERE/chk.f90" "$HERE/fmisc.f90"
  fbuild "fcinterop$v-O2"  "-O2" "$W/cside$v.o" "$HERE/chk.f90" "$HERE/fcinterop.f90"
  fbuild "ferr$v"          "-O0 -fcheck=bounds" "" "$HERE/ferr.f90"
  if [ $v = 16 ]; then
    fbuild "fcore16-A72-O2" "-O2 -mcpu=cortex-a72 -mfpu=fp-armv8" "" "$HERE/chk.f90" "$HERE/fcore.f90"
    fbuild "fmath16-A72-O2" "-O2 -mcpu=cortex-a72 -mfpu=fp-armv8" "" "$HERE/chk.f90" "$HERE/fmath.f90"
  fi
done
ls -la "$OUT" | grep ",e1f" | wc -l | sed 's/^/programs built: /'
