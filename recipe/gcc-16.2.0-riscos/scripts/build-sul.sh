#!/bin/bash
# Build the SharedUnixLibrary module (module/sul.s of the UnixLib sources) with the GCCSDK 10.2.0 tool chain: its binutils still know the FPA instructions sul.s uses (the 2.45.1 port does not,
# which is why the module is not part of build-unixlib.sh).  Same commands as the GCC 10.2.0 build tree (gcc-10.2.0/cross-build/arm-riscos-gnueabihf/libunixlib: -xassembler-with-cpp ... -g -O2, then
# strip -O binary).  Makes in $W:
#   sul-ref.bin            sul.s as it is                        (must be byte-identical to the module of the GCC 10.2.0 build tree: checked when that file exists)
#   sul-fixed.bin          with patches-unixlib/unixlib-sul-vfork-child-stack.patch applied (a vfork child that ends without exec no longer frees its parent's stack)
#   SharedULib-116orig,ffa the unpatched module with the help string marked "1.16-orig (3 Oct 2026)"   (the control of the hardware test)
#   SharedULib-116fix1,ffa the patched module with the help string marked "1.16-vforkfix1 (3 Oct 2026)"
#   sul-fixed2.bin         fix1 plus patches-unixlib/unixlib-sul-vfork-child-slot.patch (a vfork child that ends without exec no longer grows its parent's Wimp slot to the maximum: restore_wimpslot
#                          with nothing recorded did Wimp_SlotSize (0xFFFF8000))
#   SharedULib-116fix2,ffa that module with the help string marked "1.16-vforkfix2 (3 Oct 2026)"
#   sul-fixed3.bin        fix2 plus patches-unixlib/unixlib-sul-vfork-child-execed.patch (the child of an exec'd program no longer inherits IS_EXECED: its sul_exit no longer calls SOM_DeregisterClient for the
#                          client that it shares with its parent: that freed the parent's Shared Object Manager tables and froze the machine, RunTraceUL1 2026-10-04)
#   SharedULib-116fix3,ffa that module with the help string marked "1.16-vforkfix3 (4 Oct 2026)"
#   SharedULib-116fix3t,ffa fix3 plus patches-unixlib/unixlib-sul-trace.patch assembled with -DSULTRACE, help string "1.16-vforkfix3t (4 Oct 2026)": a DEBUGGING AID, not a fix.  Every step of sul_fork,
#                          sul_exec and sul_exit appends a line to the log file named in the source, with raw SWIs, so that the last line survives a freeze.  The same patched source assembled WITHOUT
#                          -DSULTRACE must be byte-identical to SharedULib-116fix3,ffa: checked here.
# usage: build-sul.sh [ROOT] [W]      ROOT = the patched UnixLib tree (default ~/gccsdk-next/unixlib/root, the one build-unixlib.sh makes: its module/sul.s is still the original), W = output (default ~/gccsdk-next/sul-build)
set -eu
HERE=$(cd "$(dirname "$0")/.." && pwd)
ROOT=${1:-$HOME/gccsdk-next/unixlib/root}
W=${2:-$HOME/gccsdk-next/sul-build}
G=${GCCSDK:-$HOME/gccsdk}
CC=$G/env/bin/arm-riscos-gnueabihf-gcc
STRIP=$G/env/arm-riscos-gnueabihf/bin/strip
REF=$G/build/gcc/gcc-10.2.0/cross-build/arm-riscos-gnueabihf/libunixlib/sul
U=$ROOT/libunixlib
mkdir -p "$W"
asm() { # source out-basename [extra compiler flag]
  "$CC" -xassembler-with-cpp -isystem "$U/include" -I "$U/incl-local" -D__UNIXLIB_CHUNKED_STACK=0 ${3:-} -g -O2 -o "$W/$2.o" -c "$1"
  "$STRIP" -O binary -o "$W/$2.bin" "$W/$2.o"
}
# the sources: the pristine one, a patched one, and the two with the help string marked (the marker is for the test builds only: it is not part of the patch)
cp "$U/module/sul.s" "$W/sul-ref.s"
cp "$U/module/sul.s" "$W/sul-fixed.s"
mkdir -p "$W/p/libunixlib/module" && cp "$W/sul-fixed.s" "$W/p/libunixlib/module/sul.s"
( cd "$W/p" && patch -p1 --silent < "$HERE/patches-unixlib/unixlib-sul-vfork-child-stack.patch" ) || { echo "ERROR: the SUL patch does not apply"; exit 1; }
cp "$W/p/libunixlib/module/sul.s" "$W/sul-fixed.s"
# fix2 = fix1 + the slot patch
mkdir -p "$W/p2/libunixlib/module" && cp "$W/sul-fixed.s" "$W/p2/libunixlib/module/sul.s"
( cd "$W/p2" && patch -p1 --silent < "$HERE/patches-unixlib/unixlib-sul-vfork-child-slot.patch" ) || { echo "ERROR: the SUL slot patch does not apply"; exit 1; }
cp "$W/p2/libunixlib/module/sul.s" "$W/sul-fixed2.s"
mark() { sed 's/\.ascii	"1\.16 (3 Apr 2020) "/.ascii	"'"$2"' "/' "$1" > "$3"; grep -q "$2" "$3" || { echo "ERROR: help string not found in $1"; exit 1; }; }
mark "$W/sul-ref.s"   "1.16-orig (3 Oct 2026)"      "$W/sul-orig-marked.s"
mark "$W/sul-fixed.s" "1.16-vforkfix1 (3 Oct 2026)" "$W/sul-fixed-marked.s"
mark "$W/sul-fixed2.s" "1.16-vforkfix2 (3 Oct 2026)" "$W/sul-fixed2-marked.s"
asm "$W/sul-ref.s" sul-ref
asm "$W/sul-fixed.s" sul-fixed
asm "$W/sul-orig-marked.s" sul-orig-marked
asm "$W/sul-fixed-marked.s" sul-fixed-marked
cp "$W/sul-orig-marked.bin" "$W/SharedULib-116orig,ffa"
cp "$W/sul-fixed-marked.bin" "$W/SharedULib-116fix1,ffa"
asm "$W/sul-fixed2.s" sul-fixed2
asm "$W/sul-fixed2-marked.s" sul-fixed2-marked
cp "$W/sul-fixed2-marked.bin" "$W/SharedULib-116fix2,ffa"
# fix3 = fix2 + the IS_EXECED patch
mkdir -p "$W/p4/libunixlib/module" && cp "$W/sul-fixed2.s" "$W/p4/libunixlib/module/sul.s"
( cd "$W/p4" && patch -p1 --silent < "$HERE/patches-unixlib/unixlib-sul-vfork-child-execed.patch" ) || { echo "ERROR: the SUL execed patch does not apply"; exit 1; }
cp "$W/p4/libunixlib/module/sul.s" "$W/sul-fixed3.s"
mark "$W/sul-fixed3.s" "1.16-vforkfix3 (4 Oct 2026)" "$W/sul-fixed3-marked.s"
asm "$W/sul-fixed3.s" sul-fixed3
asm "$W/sul-fixed3-marked.s" sul-fixed3-marked
cp "$W/sul-fixed3-marked.bin" "$W/SharedULib-116fix3,ffa"
# the trace variant = fix3 + the trace patch
mkdir -p "$W/p3/libunixlib/module" && cp "$W/sul-fixed3.s" "$W/p3/libunixlib/module/sul.s"
( cd "$W/p3" && patch -p1 --silent < "$HERE/patches-unixlib/unixlib-sul-trace.patch" ) || { echo "ERROR: the SUL trace patch does not apply"; exit 1; }
cp "$W/p3/libunixlib/module/sul.s" "$W/sul-trace.s"
mark "$W/sul-trace.s" "1.16-vforkfix3 (4 Oct 2026)"  "$W/sul-trace-off-marked.s"
mark "$W/sul-trace.s" "1.16-vforkfix3t (4 Oct 2026)" "$W/sul-trace-marked.s"
asm "$W/sul-trace-off-marked.s" sul-trace-off-marked
asm "$W/sul-trace-marked.s" sul-trace-marked -DSULTRACE
if cmp -s "$W/sul-trace-off-marked.bin" "$W/SharedULib-116fix3,ffa"; then echo "ok: the trace-patched source without -DSULTRACE is byte-identical to SharedULib-116fix3,ffa"
else echo "ERROR: the trace patch changes the normal module"; exit 1; fi
cp "$W/sul-trace-marked.bin" "$W/SharedULib-116fix3t,ffa"
if [ -f "$REF" ]; then
  if cmp -s "$REF" "$W/sul-ref.bin"; then echo "ok: the unpatched module is byte-identical to the reference build ($(stat -c %s "$REF") bytes)"
  else echo "ERROR: the unpatched module differs from the reference build: this tool chain does not reproduce it"; exit 1; fi
fi
ls -l "$W"/*.bin "$W"/SharedULib-*
