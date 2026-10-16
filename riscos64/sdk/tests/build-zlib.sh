#!/bin/bash
# build-zlib.sh ZLIBDIR [OUTDIR]: zlib (1.2.12 or later: the folder with zlib.h and the sources) built with aarch64-riscos-gcc, with the zlib's own test program (example) and sdk/tests/zlibtest.c.  A RISC OS file name with a dot is
# a directory separator: run  example foogz  (the default name foo.gz is a file gz in a folder foo).
set -e
HERE=$(cd "$(dirname "$0")" && pwd); Z=$(cd "${1:?usage: build-zlib.sh ZLIBDIR [OUTDIR]}" && pwd); OUT=${2:-$HERE/../../out/zlib}
mkdir -p "$OUT"; cd "$OUT"
LIB="adler32 compress crc32 deflate gzclose gzlib gzread gzwrite infback inffast inflate inftrees trees uncompr zutil"
SRC=$(for f in $LIB; do echo "$Z/$f.c"; done)
CC="$HERE/../bin/aarch64-riscos-gcc -O2 -w -I$Z -DHAVE_UNISTD_H -DHAVE_STDARG_H"
$CC "$Z/example.c" $SRC -o example
$CC "$HERE/zlibtest.c" $SRC -o zlibtest
