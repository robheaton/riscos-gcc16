#!/bin/bash
# A fresh source tree for the NATIVE (RISC OS-hosted) GCC 16.2.0, made from the pristine tarball by replaying the recipe:
#   apply-port.sh (patches/ + new-files/), apply-port-cxx.sh (patches-cxx/ + new-files-cxx/), apply-port-native.sh (patches-native/ + new-files-native/).
# Keep it SEPARATE from the tree the cross compiler is built from (config.host is part of every configure).
# usage: prepare-native-src.sh [DIR]      default DIR=$HOME/gccsdk-next/src/native2 (an existing gcc-16.2.0 inside it is renamed gcc-16.2.0-prev-DATE, never deleted)
set -eu
G=${GCCNEXT:-$HOME/gccsdk-next}
D=${1:-$G/src/native2}
R=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$D"; cd "$D"
[ -d gcc-16.2.0 ] && mv gcc-16.2.0 "gcc-16.2.0-prev-$(date +%Y%m%d-%H%M%S)"
tar -xf "${GCC_TARBALL:-$G/src/gcc-16.2.0.tar.xz}"
"$R/apply-port.sh" gcc-16.2.0
"$R/apply-port-cxx.sh" gcc-16.2.0
"$R/apply-port-native.sh" gcc-16.2.0
echo "native source tree ready: $D/gcc-16.2.0"
