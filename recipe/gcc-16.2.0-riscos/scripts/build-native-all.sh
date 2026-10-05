#!/bin/bash
# Build the NATIVE (RISC OS-hosted) GCC 16.2.0 compilers (cc1, cc1plus, gcc, g++, cpp, collect2) cross-built on Linux and stage them with `make install-gcc`.
# usage: build-native-all.sh O2|Os [SRC_TREE]
#   O2|Os      how the compiler itself is compiled (-Os makes cc1/cc1plus about 21% smaller)
#   SRC_TREE   default $HOME/gccsdk-next/src/native2/gcc-16.2.0 (prepare-native-src.sh)
# Result: BUILD=$HOME/gccsdk-next/build-native2-VARIANT (cc1, cc1plus, xgcc, ...), STAGE=$HOME/gccsdk-next/native-stage2-VARIANT/RISCOS/gccnative
set -eu
V=${1:?usage: build-native-all.sh O2|Os [SRC_TREE]}
G=${GCCNEXT:-$HOME/gccsdk-next}
R=$(cd "$(dirname "$0")" && pwd)
SRC=${2:-$G/src/native2/gcc-16.2.0}
B=$G/build-native2-$V
rm -rf "$B"
HOST_OPT=-$V "$R/configure-gcc16-native.sh" "$SRC" "$B" "$G/env-f" /RISCOS/gccnative > "$B.configure.log" 2>&1
"$R/make-gcc16-native.sh" "$B" "$G/env-f" -k all-gcc > "$B.make.log" 2>&1
"$R/make-gcc16-native.sh" "$B" "$G/env-f" install-gcc DESTDIR="$G/native-stage2-$V" > "$B.install.log" 2>&1
ls -la "$B/gcc/cc1" "$B/gcc/cc1plus" "$B/gcc/xgcc"
echo "build-native-all.sh $V done"
