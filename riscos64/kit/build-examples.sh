#!/bin/bash
# build-examples.sh PREFIX OUTDIR: builds the example modules of examples/ (Hello64 Svc64 Swi64 Dec64 Cxx64 Fpmod64 Libmod64 Slot64) with the aarch64-riscos tool chain in PREFIX, and their test programs, into OUTDIR as
# NAME,ffa and NAMEtest,ff8 (the files the module tests of riscos64/obey use).
set -e
KIT=$(cd "$(dirname "$0")" && pwd)
P=$(cd "${1:?PREFIX}" && pwd); O=${2:?OUTDIR}
mkdir -p "$O"; O=$(cd "$O" && pwd)
CC=$P/bin/aarch64-riscos-gcc; CMUNGE=$P/bin/cmunge64
E=$KIT/examples
T=$(mktemp -d "${TMPDIR:-/tmp}/modex.XXXXXX")
# module NAME  DIRECTORY  SOURCES  FLAGS      (a CMHG file DIRECTORY/NAME.cmhg is used unless it is Hello64, which has a header written by hand)
build () {
  local name=$1 dir=$2 flags=$3; shift 3
  local base; base=$(echo "$name" | tr 'A-Z' 'a-z')
  local srcs=(); local s; for s in "$@"; do srcs+=("$E/$dir/$s"); done
  if [ "$name" = Hello64 ]; then
    "$CC" $flags -O2 "${srcs[@]}" "$E/$dir/hello64_hdr.S" -o "$O/$name,ffa"
  else
    "$CMUNGE" -d "$T/${base}h.h" -o "$T/${base}_hdr.o" "$E/$dir/$base.cmhg"
    "$CC" $flags -O2 -I"$T" "${srcs[@]}" "$T/${base}_hdr.o" -o "$O/$name,ffa"
  fi
  "$CC" -O1 -o "$O/${base}test,ff8" "$E/$dir/${base}test.c"
}
build Hello64  hello64  -mmodule     hello64.c
build Svc64    svc64    -mmodule     svc64.c
build Swi64    swi64    -mmodule     swi64.c
build Dec64    dec64    -mmodule     dec64.c
build Cxx64    cxx64    -mmodule     cxx64.cc
build Fpmod64  fpmod64  -mmodule-fp  fpmod64.c
build Libmod64 libmod64 -mmodule-fp  libmod64.c
build Slot64   slot64   -mmodule     slot64.c
build Kv64     kv64     -mmodule     kv64.c
build Vec64    vec64    -mmodule     vec64.c vec64_veneer.S
rm -rf "$T"
ls -l "$O"/*,ffa
