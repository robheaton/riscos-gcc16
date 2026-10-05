#!/bin/bash
# Build the rotest matrix for RISC OS with the installed GCC 10.2.0 and the new GCC 16.2.0.
#   ./build.sh            -> out/<name>,e1f  (+ libfooNN.so,e1f, RunAll,feb, README,fff)
# Executables use RISC OS' ,e1f (ELF) filetype suffix; Obey script ,feb; text ,fff.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=$HERE/out
OBJ=$HERE/obj
OLD=${OLD:-$HOME/gccsdk/env/bin/arm-riscos-gnueabihf-gcc}
NEW=${NEW:-$HOME/gccsdk-next/env-f/bin/arm-riscos-gnueabihf-gcc}
rm -rf "${OUT:?}" "${OBJ:?}"; mkdir -p "$OUT" "$OBJ"
python3 "$HERE/gen_cases.py" "$HERE/cases.h" || exit 1
LOG=$OUT/build.log; : > "$LOG"
COMMON="-std=gnu11 -g0 -Wall -Wno-unused-parameter -Wno-sign-compare -Wno-infinite-recursion"
FAILED=0

# cc_one <cc> <flags> <src> <obj> [extra]
cc_one() { local cc=$1 fl=$2 src=$3 obj=$4; shift 4
  echo "+ $cc $fl $* -c $src" >> "$LOG"
  $cc $COMMON $fl "$@" -c "$HERE/$src" -o "$obj" >> "$LOG" 2>&1; }

# build <name> <cc-for-driver> <cc-for-callee> <optflags> <ldextra> <cfgtag> [defs]
build() { local name=$1 cc=$2 cc2=$3 opt=$4 ld=$5 cfg=$6 defs=${7:-}
  local d=$OBJ/$name; mkdir -p "$d"
  local ver; ver=$($cc -dumpversion)
  local c23=""; if [ "$cc" = "$NEW" ]; then c23="-DHAVE_C23"; fi
  cc_one "$cc"  "$opt" rotest.c "$d/rotest.o" "-DROTEST_CFG=\"$cfg gcc$ver\"" $defs $c23 || true
  cc_one "$cc2" "$opt" callee.c "$d/callee.o" || true
  local objs="$d/rotest.o $d/callee.o"
  case "$defs" in *WITH_SHLIB*) ;; *) cc_one "$cc" "$opt" picdata.c "$d/picdata.o" || true; objs="$objs $d/picdata.o";; esac
  if [ -n "$c23" ]; then cc_one "$cc" "$opt -std=gnu23" c23.c "$d/c23.o" || true; objs="$objs $d/c23.o"; fi
  echo "+ link $name" >> "$LOG"
  if $cc $opt $objs $ld -lm -o "$OUT/$name,e1f" >> "$LOG" 2>&1; then
    printf "  built  %-10s %s\n" "$name" "$(stat -c %s "$OUT/$name,e1f") bytes"
  else
    printf "  FAILED %-10s (see out/build.log)\n" "$name"; FAILED=$((FAILED+1))
  fi
}

echo "compilers: OLD=$($OLD -dumpversion)  NEW=$($NEW -dumpversion)"
SL="-static-libgcc -Wl,--allow-shlib-undefined"      # the UnixLib of the sysroot refers to libgcc functions: with a static libgcc ld must not insist that they are defined there
for tag in 10 16; do
  if [ $tag = 10 ]; then CC=$OLD; else CC=$NEW; fi
  build "g$tag-O0"  $CC $CC "-O0"        "$SL" "g$tag -O0"
  build "g$tag-O2"  $CC $CC "-O2"        "$SL" "g$tag -O2"
  build "g$tag-O3"  $CC $CC "-O3"        "$SL" "g$tag -O3"
  build "g$tag-O2p" $CC $CC "-O2 -fPIC"  "$SL" "g$tag -O2 -fPIC"
  # shared-library variant: picdata.c lives in libfooNN.so (needs libfooNN/so on the target's library path)
  cc_one $CC "-O2 -fPIC" picdata.c "$OBJ/libfoo$tag.o"
  if $CC -shared -O2 -fPIC "$OBJ/libfoo$tag.o" -o "$OUT/libfoo$tag.so,e1f" >> "$LOG" 2>&1; then
    # link against a plain-named copy so -lfooNN works
    cp "$OUT/libfoo$tag.so,e1f" "$OBJ/libfoo$tag.so"
    build "g$tag-sl" $CC $CC "-O2 -fPIC" "$SL -L$OBJ -lfoo$tag -L$HOME/gccsdk/env/lib -ldl" "g$tag -O2 -fPIC shlib" "-DWITH_SHLIB -DWITH_DLOPEN -DFOO_SONAME=\"libfoo$tag.so\""
  else echo "  FAILED libfoo$tag.so"; FAILED=$((FAILED+1)); fi
done
build "g16-dyn" $NEW $NEW "-O2" "" "g16 -O2 dynamic-libgcc"
build "mixA"    $NEW $OLD "-O2" "$SL" "mixA driver16+callee10 -O2"
build "mixB"    $OLD $NEW "-O2" "$SL" "mixB driver10+callee16 -O2"
echo "$FAILED build(s) failed"
