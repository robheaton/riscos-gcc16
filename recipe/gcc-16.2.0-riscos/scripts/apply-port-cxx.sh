#!/bin/bash
# Apply the libstdc++ (C++) part of the RISC OS port to a gcc-16.2.0 tree and regenerate libstdc++-v3/configure.
#   - libtool.m4 patch: NOT needed (libtool uses its generic GNU settings for host_os=gnueabihf, as in the 10.2.0 build)
#   - acinclude.m4 patch: NOT needed (long-double guards replaced by the cache overrides in make-gcc16-cxx.sh)
# Needs autoconf 2.69 (Ubuntu: package autoconf2.69, binary autoconf2.69).
set -e
SRC=${1:?usage: apply-port-cxx.sh /path/to/gcc-16.2.0}
HERE=$(cd "$(dirname "$0")/.." && pwd)
cd "$SRC"
for p in "$HERE"/patches-cxx/*.patch; do echo "applying $(basename $p)"; patch -p1 -l < "$p"; done
cp -r "$HERE"/new-files-cxx/* .
( cd libstdc++-v3 && PATH=/usr/bin:/bin autoconf2.69 -f )
grep -c 'riscos' libstdc++-v3/configure | sed 's/^/riscos mentions in regenerated configure: /'
