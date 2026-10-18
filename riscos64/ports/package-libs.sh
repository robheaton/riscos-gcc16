#!/bin/bash
# package-libs.sh WORKDIR OUTDIR [VERSION]: packs the static libraries that build-more.sh put in WORKDIR/inst (zlib, expat, pcre2, libpng, libjpeg-turbo: headers and .a files) into
# OUTDIR/riscos64-libs-VERSION.tar.xz, to be unpacked into the tool chain:  tar -xf riscos64-libs-1.tar.xz -C PREFIX/aarch64-riscos --strip-components=1
set -e
W=$(cd "${1:?WORKDIR}" && pwd); O=${2:?OUTDIR}; V=${3:-1}
mkdir -p "$O"; O=$(cd "$O" && pwd)
N=riscos64-libs-$V
rm -rf "$O/$N"; mkdir -p "$O/$N/lib" "$O/$N/include" "$O/$N/share/riscos64/licenses"
cp -a "$W/inst/include/." "$O/$N/include/"
for f in "$W"/inst/lib/*.a; do cp "$f" "$O/$N/lib/"; done
cp "$W/zlib-1.3.1/README" "$O/$N/share/riscos64/licenses/zlib-README" 2>/dev/null || true
cp "$W/expat-2.6.4/COPYING" "$O/$N/share/riscos64/licenses/expat-COPYING" 2>/dev/null || true
cp "$W/pcre2-10.44/LICENCE" "$O/$N/share/riscos64/licenses/pcre2-LICENCE" 2>/dev/null || cp "$W/pcre2-10.44/LICENCE.md" "$O/$N/share/riscos64/licenses/" 2>/dev/null || true
cp "$W/libpng-1.6.44/LICENSE" "$O/$N/share/riscos64/licenses/libpng-LICENSE" 2>/dev/null || true
cp "$W/libjpeg-turbo-3.0.4/LICENSE.md" "$W/libjpeg-turbo-3.0.4/README.ijg" "$O/$N/share/riscos64/licenses/" 2>/dev/null || true
cat > "$O/$N/README.txt" <<EOF
Static libraries for the aarch64-riscos tool chain (GCC 16.2.0, UnixLib 64): zlib 1.3.1, expat 2.6.4, PCRE2 10.44 (8, 16 and 32 bit, POSIX wrapper, no JIT), libpng 1.6.44, libjpeg-turbo 3.0.4 (no SIMD).
Install:  tar -xf riscos64-libs-$V.tar.xz -C PREFIX/aarch64-riscos --strip-components=1     (PREFIX is where the tool chain is unpacked)
Use:      aarch64-riscos-gcc -munixlib prog.c -lpng16 -lz -lm     (or -ljpeg, -lexpat, -lpcre2-8)
Built by riscos64/ports/build-more.sh; the programs cjpeg djpeg jpegtran pngfix xmlwf pcre2grep pcre2test are in the !GNU application. Licences: share/riscos64/licenses/.
EOF
( cd "$O" && tar -cf - "$N" | xz -T0 -6 > "$N.tar.xz" && sha256sum "$N.tar.xz" > "$N.tar.xz.sha256" && cat "$N.tar.xz.sha256" )
