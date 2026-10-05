#!/bin/bash
# Apply the host-side patches for a NATIVE (RISC OS-hosted) compiler to a tree that already has the port applied (apply-port.sh, apply-port-cxx.sh):
# patches-native/ (config.host for arm-riscos hosts, libcpp off_t vs __off_t) and new-files-native/ (the RISC OS driver hooks).
# Keep this tree SEPARATE from the one the cross compiler is built from: config.host is part of every configure.
# usage: apply-port-native.sh /path/to/ported/gcc-16.2.0
set -e
SRC=${1:?usage: apply-port-native.sh /path/to/ported/gcc-16.2.0}
HERE=$(cd "$(dirname "$0")/.." && pwd)
cd "$SRC"
for p in "$HERE"/patches-native/*.patch; do echo "applying $(basename $p)"; patch -p1 -l < "$p"; done
[ -d "$HERE/new-files-native" ] && cp -r "$HERE"/new-files-native/* .
echo "native (RISC OS host) support applied to $SRC"
