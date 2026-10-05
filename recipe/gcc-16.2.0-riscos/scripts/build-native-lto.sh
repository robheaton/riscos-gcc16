#!/bin/bash
# build-native-lto.sh -- the NATIVE (RISC OS-hosted) GCC 16.2.0 with LTO: lto1 + lto-wrapper, and cc1 / cc1plus / f951 compiled with ENABLE_LTO.  Same recipe as build-native-all.sh (same source tree, same options) but
# --enable-lto, in its own build and stage directories: the proven native package is not touched.
# result: BUILD=$G/build-native3-lto  STAGE=$G/native-stage3-lto/RISCOS/gccnative   logs: $G/build-native3-lto.{configure,make,install}.log
set -eu
G=${GCCNEXT:-$HOME/gccsdk-next}
R=$G/recipe/gcc-16.2.0-riscos/scripts
SRC=${1:-$G/src/native2/gcc-16.2.0}
B=$G/build-native3-lto
ST=$G/native-stage3-lto
case "$B" in "$G"/build-native3-lto) ;; *) echo "refusing: $B"; exit 1;; esac
[ -d "$B" ] && mv "$B" "$B-prev-$(date +%Y%m%d-%H%M%S)"
[ -d "$ST" ] && mv "$ST" "$ST-prev-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$B"
date +%T > "$B.start"
NATIVE_LTO=yes HOST_OPT=-O2 EXTRA_CONFIGURE_ARGS="--with-plugin-ld=$R/../data/no-plugin-ld" "$R/configure-gcc16-native.sh" "$SRC" "$B" "$G/env-f" /RISCOS/gccnative > "$B.configure.log" 2>&1
"$R/make-gcc16-native.sh" "$B" "$G/env-f" -k all-gcc > "$B.make.log" 2>&1 || true
"$R/make-gcc16-native.sh" "$B" "$G/env-f" install-gcc DESTDIR="$ST" > "$B.install.log" 2>&1
ls -la "$B/gcc/cc1" "$B/gcc/cc1plus" "$B/gcc/f951" "$B/gcc/lto1" "$B/gcc/lto-wrapper" "$B/gcc/xgcc" 2>&1
date +%T > "$B.end"
echo "build-native-lto.sh done"
