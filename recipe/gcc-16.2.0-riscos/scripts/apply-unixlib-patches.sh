#!/bin/bash
# apply-unixlib-patches.sh DIR
# Applies the UnixLib patch series of this port (patches-unixlib/, in the order build-unixlib.sh applies it) to the unmodified UnixLib sources, to see the
# patched library source without building anything.  DIR is the directory that CONTAINS libunixlib/: unpack the snapshot attached to the release into an
# empty directory first:
#     mkdir ul && tar -xf gccsdk-unixlib-r7800.tar.xz -C ul && apply-unixlib-patches.sh ul
# (or use a copy of GCCSDK svn trunk r7800, gcc4/recipe/files/gcc, which has libunixlib/ in it).  A patch that does not apply is an error.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
D=${1:?usage: apply-unixlib-patches.sh DIR   (DIR contains libunixlib/)}
[ -d "$D/libunixlib" ] || { echo "no $D/libunixlib" >&2; exit 1; }
cd "$D"
n=0
for p in $(grep -o '^apply_patch [^ ]*' "$HERE/build-unixlib.sh" | awk '{print $2}'); do
  if patch -p1 -N -s < "$HERE/../patches-unixlib/$p" > /dev/null; then n=$((n+1)); else echo "ERROR: $p does not apply" >&2; exit 1; fi
done
echo "applied $n patches to $D/libunixlib"
