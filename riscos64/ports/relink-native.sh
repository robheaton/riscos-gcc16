#!/bin/bash
# relink-native.sh PREFIX --gcc BUILD/gcc [--gcc ...] --binutils BUILD [--binutils ...]
# Links the native compilers and binutils (programs for RISC OS 64, built by build-native-gcc.sh, build-native-binutils.sh and hosted64/build-hosted64.sh) again against the SDK libraries of
# the installed aarch64-riscos tool chain in PREFIX, without compiling anything: the programs link libriscosstub.a and libunixlib64.a statically, so after the SDK has changed (refresh-sdk.sh)
# they must be linked again to carry the change.  The driver, collect2 (and xg++) get a small heap, because they only wait for the compiler and the linker; cc1, cc1plus, as and ld keep the big one.
# --gcc is the gcc/ folder of a GCC build (xgcc xg++ collect2 cc1 cc1plus), --binutils the top folder of a binutils build (binutils/ar objcopy ..., gas/as-new, ld/ld-new).
set -e
PREFIX=$(cd "${1:?PREFIX of the installed aarch64-riscos tool chain}" && pwd); shift
G=(); B=()
while [ $# -gt 0 ]; do
  case $1 in
    --gcc) G+=("${2:?--gcc DIR}"); shift 2 ;;
    --binutils) B+=("${2:?--binutils DIR}"); shift 2 ;;
    *) echo "unknown argument $1"; exit 2 ;;
  esac
done
export PATH=$PREFIX/bin:$PATH
for g in "${G[@]}"; do
  echo "$(date +%T) relinking the compiler programs in $g"
  ( cd "$g" && rm -f xgcc xg++ collect2 cc1 cc1plus \
    && make xgcc xg++ collect2 LDFLAGS="-static-libstdc++ -static-libgcc -Wl,--defsym=__ul64_heap_max=0x1000000" \
    && make cc1 cc1plus )
done
for b in "${B[@]}"; do
  echo "$(date +%T) relinking the binutils in $b"
  ( cd "$b" && rm -f binutils/objcopy binutils/objdump binutils/readelf binutils/ar gas/as-new ld/ld-new \
    && make all-binutils all-gas all-ld )
done
echo "$(date +%T) done"
