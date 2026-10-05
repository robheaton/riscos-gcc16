#!/bin/bash
# build-model.sh -- host model of UnixLib's vfscanf (libunixlib/stdio/scanf.c): the function text of the PRISTINE upstream file ("old") and of the file with patches/unixlib-scanf-long-long.patch ("new") is compiled on a Linux
# host (gcc, ASan + UBSan) with the 32-bit types of the target where that matters: `long' and strtol / strtoul are 32 bits, size_t and ptrdiff_t are 4 bytes (the sizeof's in the text are replaced by 4).  Nothing is copied
# by hand: the model is made from the two files.  Then:
#   scanf_test            47315 cases: the old and the new code agree on every modifier the old code had (l, h, none) and the new code agrees with glibc on the new ones (hh ll j q z t), %Lf, %n, canary cases
#   scanfcheck_new/_old   replay the table scantab (made by glibc with "scanfcheck_host gen") on the new / the old code, and scanfcheck_host replays it on glibc itself
# usage: build-model.sh [UPSTREAM_WORKING_COPY]    (default $GCCSDK or ~/gccsdk: a clean svn working copy of trunk r7800; needs  gcc4/recipe/files/gcc/libunixlib/stdio/scanf.c)
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); B=$(cd "$HERE/../.." && pwd)
G=${1:-${GCCSDK:-$HOME/gccsdk}}
U=gcc4/recipe/files/gcc/libunixlib
W=${W:-$HERE/work}
rm -rf "$W"; mkdir -p "$W/pristine" "$W/tree/$U/stdio"
cp "$G/$U/stdio/scanf.c" "$W/pristine/scanf.c"; cp "$G/$U/stdio/scanf.c" "$W/tree/$U/stdio/scanf.c"
(cd "$W/tree" && patch -p1 -s < "$B/patches/unixlib-scanf-long-long.patch")
extract() {   # $1 = scanf.c, $2 = output: from "#define FLOATING_POINT" to the line before "int vsscanf" (vfscanf and __sccl with their defines)
  awk '/^#define FLOATING_POINT/ {on=1} /^int vsscanf/ {on=0} on' "$1" > "$2"
  sed -i -e 's/va_arg(ap, long \*)/va_arg(ap, int32_t *)/g' -e 's/^#define u_long unsigned long/#define u_long uint32_t/' \
         -e 's/strtoll\b/model_strtoll/g; s/strtoull\b/model_strtoull/g; s/strtold\b/model_strtold/g' \
         -e 's/(u_long (\*)(const char \*, char \*\*, int))strtol;/(u_long (*)(const char *, char **, int))model_strtol32;/' \
         -e 's/ccfn = strtoul;/ccfn = (u_long (*)(const char *, char **, int))model_strtoul32;/' \
         -e 's/sizeof (size_t)/4/g; s/sizeof (ptrdiff_t)/4/g; s/sizeof (long)/4/g; s/sizeof (int)/4/g' "$2"
}
extract "$W/pristine/scanf.c" "$W/body_old.c"
extract "$W/tree/$U/stdio/scanf.c" "$W/body_new.c"
cp "$HERE/model_main.c" "$HERE/model_test.c" "$HERE/scanfcheck.c" "$W/"
CF="-O1 -g -Wall -Wno-unused -Wno-type-limits -Wno-tautological-compare -Wno-int-to-pointer-cast -fsanitize=address,undefined -fno-sanitize-recover=undefined"
cc $CF -c -o "$W/model_new.o" "$W/model_main.c" -DBODY_FILE='"body_new.c"' -I"$W"
cc $CF -c -o "$W/model_old.o" "$W/model_main.c" -DBODY_FILE='"body_old.c"' -Dul_vfscanf=ul_vfscanf_old -Dul_vsscanf=ul_vsscanf_old -Dul_sscanf=ul_sscanf_old -I"$W"
cc $CF -Wno-format-security -o "$W/scanf_test" "$W/model_test.c" "$W/model_new.o" "$W/model_old.o" -I"$W" -lm
cc $CF -Wno-format-security -Wno-format -DUSE_MODEL -DMODEL32 -DMAXFAIL=40 -o "$W/scanfcheck_new" "$W/scanfcheck.c" "$W/model_new.o"
cc $CF -Wno-format-security -Wno-format -DUSE_MODEL -DUSE_MODEL_OLD -DMODEL32 -DMAXFAIL=3 -o "$W/scanfcheck_old" "$W/scanfcheck.c" "$W/model_old.o"
gcc -O2 -Wno-format-security -DSCANFCHECK_GEN -o "$W/scanfcheck_host" "$W/scanfcheck.c"
echo "--- scanf_test (old == new for the old modifiers, new == glibc for the new ones):"; "$W/scanf_test"
echo "--- scanfcheck, the table on the NEW code:";  "$W/scanfcheck_new" "$HERE/scantab" | tail -3
echo "--- scanfcheck, the table on the OLD code (this is what is wrong with it):"; "$W/scanfcheck_old" "$HERE/scantab" | tail -4
echo "--- scanfcheck, the table on glibc itself (the table is right):"; "$W/scanfcheck_host" "$HERE/scantab" | tail -1
