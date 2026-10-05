#!/bin/bash
# build.sh <gcc> <g++> <outdir> [extra link flags]  : build the stress test with the given compilers
S=$(dirname $(readlink -f $0))
GCC=$1; GXX=$2; OUT=$(readlink -f $3); shift 3; XL="$*"
mkdir -p $OUT && cd $OUT || exit 1
$GCC -O2 -fPIC -shared $S/ext.c -o libext.so,e1f || echo "FAIL libext"
cp libext.so,e1f libext.so
$GCC -O2 -fPIC -shared $S/mid.c -L. -lext $XL -o libmid.so,e1f || echo "FAIL libmid"
cp libmid.so,e1f libmid.so
$GCC -O2 $S/main.c -L. -lmid -lext $XL -o stress,e1f || echo "FAIL stress"
$GCC -O2 -fPIC -shared -ffunction-sections -Wl,--gc-sections $S/mid.c -L. -lext $XL -o libmid-gc.so,e1f || echo "FAIL libmid-gc"
$GXX -O2 -fPIC -shared $S/cxxlib.cc $S/cxxlib2.cc $XL -o libcxx.so,e1f || echo "FAIL libcxx"
$GXX -O2 -fPIC -shared -ffunction-sections -Wl,--gc-sections $S/cxxlib.cc $S/cxxlib2.cc $XL -o libcxx-gc.so,e1f || echo "FAIL libcxx-gc"
cp libcxx.so,e1f libcxx.so
$GXX -O2 $S/cxxmain.cc -L. -lcxx $XL -o cxxmain,e1f || echo "FAIL cxxmain"
ls -la
