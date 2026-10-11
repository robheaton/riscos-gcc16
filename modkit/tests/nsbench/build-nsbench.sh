#!/bin/bash
# build-nsbench.sh TOOLCHAIN ENV OUT [FLAGS]: build nsbench against the libraries that build-netsurf.py installed in ENV (its --work/env)
set -e
TC=$1; E=$2; OUT=$3; FLAGS=${4:--O2}
export PKG_CONFIG_PATH=$E/lib/pkgconfig PKG_CONFIG_LIBDIR=$E/lib/pkgconfig
$TC/bin/arm-riscos-gnueabihf-gcc -std=gnu99 $FLAGS -I$E/include -o $OUT $(dirname "$0")/nsbench.c -L$E/lib $(pkg-config --static --libs libdom libcss libhubbub libparserutils libwapcaplet expat) -ljpeg -lpng16 -lz -lm
