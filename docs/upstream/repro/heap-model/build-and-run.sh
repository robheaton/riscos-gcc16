#!/bin/bash
# Build and run the host model of the heap growth of a vfork + exec child against a UnixLib source tree.
#   usage: build-and-run.sh SRCDIR LABEL [mutate]
#     SRCDIR  the libunixlib directory (with sys/brk.c, sys/stackalloc.c, incl-local/internal/unix.h): pristine upstream, or with
#             patches/unixlib-vfork-exec-heap-limit.patch applied
#     LABEL   original | patched  (only for the last line of the output)
#     mutate  (patched only) re-run with each of a few deliberate breakages of the patched check: every one must make the model fail
# The last line of the output is "LABEL: N check(s) failed".  Needs: a host gcc.  Nothing of RISC OS is used: the real brk.c and stackalloc.c run on the host
# against a mock of SharedUnixLibrary's sul_wimpslot and the numbers measured on the machine.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$1; LABEL=${2:-model}; MODE=${3:-}
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT
mkdir -p "$W/inc" "$W/src/sys"
cp -r "$HERE/stubs/." "$W/inc/"
# the REAL struct ul_memory
sed -n '/^struct ul_memory$/,/^};/p' "$SRC/incl-local/internal/unix.h" > "$W/inc/ul_memory_struct.h"
cp "$SRC/sys/brk.c" "$SRC/sys/stackalloc.c" "$W/src/sys/"
DEFS=""
grep -q appspace_himem_max "$W/inc/ul_memory_struct.h" && DEFS="-DHAVE_HIMEM_MAX"
build_run() { # dir label
  ( cd "$1" && gcc -std=gnu11 -O1 -g -w $DEFS -I"$W/inc" -Dbrk=ul_brk -Dsbrk=ul_sbrk -c src/sys/brk.c -o brk.o \
      && gcc -std=gnu11 -O1 -g -w $DEFS -I"$W/inc" -c src/sys/stackalloc.c -o stackalloc.o \
      && gcc -std=gnu11 -O1 -g -w $DEFS -I"$W/inc" -c "$HERE/harness.c" -o harness.o \
      && gcc harness.o brk.o stackalloc.o -o model ) || { echo "build failed"; return 99; }
  MODEL_LABEL="$2" "$1/model"
}
if [ "$MODE" != mutate ]; then
  build_run "$W" "$LABEL"; exit $?
fi
# mutation checks: break the patched condition in several ways; the model must notice every one
n=0; missed=0
mut() { # description sed-expression
  n=$((n+1)); rm -rf "$W/m"; mkdir -p "$W/m/src/sys"; cp "$W/src/sys/brk.c" "$W/src/sys/stackalloc.c" "$W/m/src/sys/"
  sed -i "$2" "$W/m/src/sys/stackalloc.c"
  if cmp -s "$W/src/sys/stackalloc.c" "$W/m/src/sys/stackalloc.c"; then echo "  mutation $n NOT APPLIED: $1"; missed=$((missed+1)); return; fi
  r=$(build_run "$W/m" "mutant $n" 2>&1 | tail -1)
  case "$r" in *": 0 check(s) failed"*) echo "  MISSED  mutation $n: $1"; missed=$((missed+1));; *) echo "  caught  mutation $n: $1  ($r)";; esac
}
mut "refusal never happens (condition made false)"            's/if (incr > mem->appspace_himem_max - mem->appspace_himem)/if (0 \&\& incr > mem->appspace_himem_max - mem->appspace_himem)/'
mut "refuse only when the request is more than 4 KB over"     's/if (incr > mem->appspace_himem_max - mem->appspace_himem)/if (incr > mem->appspace_himem_max - mem->appspace_himem + 4096)/'
mut "refuse only when the request is more than 4 bytes over"  's/if (incr > mem->appspace_himem_max - mem->appspace_himem)/if (incr > mem->appspace_himem_max - mem->appspace_himem + 4)/'
mut "signed comparison"                                       's/if (incr > mem->appspace_himem_max - mem->appspace_himem)/if ((int) incr > (int) (mem->appspace_himem_max - mem->appspace_himem))/'
mut "refuse everything"                                       's/if (incr > mem->appspace_himem_max - mem->appspace_himem)/if (1)/'
mut "limit taken from the application space limit"            's/if (incr > mem->appspace_himem_max - mem->appspace_himem)/if (incr > mem->appspace_limit - mem->appspace_himem)/'
mut "limit compared with himem the wrong way round"           's/if (incr > mem->appspace_himem_max - mem->appspace_himem)/if (incr > mem->appspace_himem - mem->appspace_himem_max)/'
# (equivalent, not run: "incr >= ..." - incr is never 0 here; "+ 1 / + 2 / + 3" - brk_rw rounds every request up to a multiple of 4)
echo "mutation checks: $n mutations, $missed not caught"
[ "$missed" = 0 ]
