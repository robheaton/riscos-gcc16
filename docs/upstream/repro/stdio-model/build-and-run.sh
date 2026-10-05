#!/bin/bash
# Build and run the host model of the direct-transfer loops of fread () / fwrite () against a UnixLib source tree.
#   usage: build-and-run.sh SRCDIR LABEL [mutate]
#     SRCDIR  the libunixlib directory (with stdio/fread.c, stdio/fwrite.c, include/stdio.h): pristine upstream, or with patches/unixlib-stdio-short-transfers.patch applied
#     LABEL   original | patched  (only for the last line of the output)
#     mutate  (patched only) re-run with deliberate breakages of the new lines: every one must make the model fail
# The last line of the output is "LABEL: ... N failed check(s)".  Needs a host gcc only: the REAL fread.c and fwrite.c run on the host, with a mock read () and write ().
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$1; LABEL=${2:-model}; MODE=${3:-}
W=${KEEP:-$(mktemp -d)}; [ -n "${KEEP:-}" ] || trap 'rm -rf "$W"' EXIT
mkdir -p "$W/inc" "$W/src/stdio"
cp -r "$HERE/stubs/." "$W/inc/"
# the REAL struct __iobuf (the non-SCL one) and the mode union
awk '/^typedef union$/ {p=1} p {print} /^};$/ && p && seen++ {exit}' "$SRC/include/stdio.h" | sed -n '1,/^};$/p' | sed 's/^  FILE \*next;/  struct __iobuf *next;/' > "$W/inc/stdio_struct.h"
mkdir -p "$W/hinc"; cp "$W/inc/stdio_struct.h" "$W/hinc/"
cp "$SRC/stdio/fread.c" "$SRC/stdio/fwrite.c" "$W/src/stdio/"
build_run() { # dir label
  ( cd "$1" && gcc -std=gnu11 -O1 -g -w -D_GNU_SOURCE -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -include string.h -I"$W/inc" -Dread=model_read -Dwrite=model_write -Disatty=model_isatty -Dfread=ul_fread -Dfwrite=ul_fwrite -Dfread_unlocked=ul_fread_unlocked -Dfwrite_unlocked=ul_fwrite_unlocked -c src/stdio/fread.c -o fread.o \
      && gcc -std=gnu11 -O1 -g -w -D_GNU_SOURCE -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -include string.h -I"$W/inc" -Dread=model_read -Dwrite=model_write -Disatty=model_isatty -Dfread=ul_fread -Dfwrite=ul_fwrite -Dfread_unlocked=ul_fread_unlocked -Dfwrite_unlocked=ul_fwrite_unlocked -c src/stdio/fwrite.c -o fwrite.o \
      && gcc -std=gnu11 -O1 -g -w -I"$W/hinc" -c "$HERE/harness.c" -o harness.o \
      && gcc harness.o fread.o fwrite.o -o model ) || { echo "build failed"; return 99; }
  MODEL_LABEL="$2" "$1/model" ${CASES:-}
}
if [ "$MODE" != mutate ]; then build_run "$W" "$LABEL"; exit $?; fi
n=0; missed=0
mut() { # file description sed-expression
  n=$((n+1)); rm -rf "$W/m"; mkdir -p "$W/m/src/stdio"; cp "$W/src/stdio/fread.c" "$W/src/stdio/fwrite.c" "$W/m/src/stdio/"
  sed -i "$3" "$W/m/src/stdio/$1"
  if cmp -s "$W/src/stdio/$1" "$W/m/src/stdio/$1"; then echo "  mutation $n NOT APPLIED: $2"; missed=$((missed+1)); return; fi
  r=$(CASES=20000 build_run "$W/m" "mutant $n" 2>&1 | tail -1)
  case "$r" in *" 0 failed check(s)") echo "  MISSED  mutation $n: $2"; missed=$((missed+1));; *) echo "  caught  mutation $n: $2  ($r)";; esac
}
mut fread.c  "fread: the data pointer is not advanced"                  's/^\t      data = (void \*)((char \*)data + bytes);$/\t      \/\* mutated \*\//'
mut fread.c  "fread: advanced by one byte too many"                     's/data = (void \*)((char \*)data + bytes);$/data = (void *)((char *)data + bytes + 1);/;t;b'
mut fwrite.c "fwrite (buffered path): the data pointer is not advanced" '0,/^\t      data = (const void \*)((const char \*)data + bytes);$/s//\t      \/\* mutated \*\//'
mut fwrite.c "fwrite (unbuffered path): the data pointer is not advanced" 's/^\t  data = (const void \*)((const char \*)data + bytes);$/\t  \/\* mutated \*\//'
mut fwrite.c "fwrite: advanced by the whole request instead of bytes"   's/data = (const void \*)((const char \*)data + bytes);$/data = (const void *)((const char *)data + to_write + bytes);/'
echo "mutation checks: $n mutations, $missed not caught"
[ "$missed" = 0 ]
