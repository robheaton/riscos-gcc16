#!/bin/bash
# Apply the RISC OS (arm-riscos-gnueabihf) port to a pristine, extracted gcc-16.2.0 tree.
set -e
SRC=${1:?usage: apply-port.sh /path/to/gcc-16.2.0}
HERE=$(cd "$(dirname "$0")/.." && pwd)
cd "$SRC"
for p in "$HERE"/patches/*.patch; do echo "applying $(basename $p)"; patch -p1 -l < "$p"; done
cp -r "$HERE"/new-files/* .
echo "RISC OS port applied to $SRC"
