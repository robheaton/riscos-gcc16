#!/bin/bash
# build-more.sh PREFIX WORKDIR: more programs and libraries for RISC OS 64 (UnixLib 64) with the aarch64-riscos tool chain in PREFIX.
#   programs (WORKDIR/more/NAME,ff8): zstd 1.5.6, lz4 1.10.0, tree 2.1.3, unzip 6.00, zip 3.0, jq 1.7.1, pcre2grep 10.44
#   less 668 (needs the termcap functions of the SDK: sdk/crt/posix/termcap.c, the "riscos" entry for the VDU drivers)
#   libraries (static, in WORKDIR/inst/{include,lib}): zlib 1.3.1, expat 2.6.4, pcre2 10.44 (8, 16, 32 bit and posix; no JIT), libpng 1.6.44, libjpeg-turbo 3.0.4 (no SIMD, no TurboJPEG); also WORKDIR/more/cjpeg djpeg pngfix xmlwf
# Tested on RISC OS Pyromaniac on 18 October 2026 (jobs port_arc, port_jq, port_lib of tools/run_on_service.py): zstd and lz4 read files made on Linux and give files that Linux can read, unzip reads a zip made
# by Linux and zip's files test OK, jq gives the output of the host jq on the test filters, cjpeg and djpeg give the bytes of PIL's libjpeg-turbo, pcre2grep gives the lines of grep -P.
set -e
P=$(cd "${1:?PREFIX}" && pwd); W=${2:?WORKDIR}
mkdir -p "$W/more"; W=$(cd "$W" && pwd); cd "$W"
T=$P/bin/aarch64-riscos
CC="$T-gcc -munixlib"
BUILD="$(uname -m)-pc-linux-gnu"
INST=$W/inst; mkdir -p "$INST"
fetch () { [ -f "$(basename "$1")" ] || curl -sSLfO "$1"; echo "$2  $(basename "$1")" | sha256sum -c -; }
fetch https://github.com/facebook/zstd/releases/download/v1.5.6/zstd-1.5.6.tar.gz 8c29e06cf42aacc1eafc4077ae2ec6c6fcb96a626157e0593d5e82a34fd403c1
fetch https://github.com/lz4/lz4/releases/download/v1.10.0/lz4-1.10.0.tar.gz 537512904744b35e232912055ccf8ec66d768639ff3abe5788d90d792ec5f48b
fetch https://gitlab.com/OldManProgrammer/unix-tree/-/archive/2.1.3/unix-tree-2.1.3.tar.gz f554a1b62233b96fa8eaa2d85e91bc62cad80ee441fd591380f16cdfbe3e8868
fetch https://downloads.sourceforge.net/infozip/unzip60.tar.gz 036d96991646d0449ed0aa952e4fbe21b476ce994abc276e49d30e686708bd37
fetch https://downloads.sourceforge.net/infozip/zip30.tar.gz f0e8bb1f9b7eb0b01285495a2699df3a4b766784c1765a8f1aeedf63c0806369
fetch https://github.com/jqlang/jq/releases/download/jq-1.7.1/jq-1.7.1.tar.gz 478c9ca129fd2e3443fe27314b455e211e0d8c60bc8ff7df703873deeee580c2
fetch https://www.greenwoodsoftware.com/less/less-668.tar.gz 2819f55564d86d542abbecafd82ff61e819a3eec967faa36cd3e68f1596a44b8
fetch https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.xz 38ef96b8dfe510d42707d9c781877914792541133e1870841463bfa73f883e32
fetch https://github.com/libexpat/libexpat/releases/download/R_2_6_4/expat-2.6.4.tar.xz a695629dae047055b37d50a0ff4776d1d45d0a4c842cf4ccee158441f55ff7ee
fetch https://github.com/PCRE2Project/pcre2/releases/download/pcre2-10.44/pcre2-10.44.tar.bz2 d34f02e113cf7193a1ebf2770d3ac527088d485d4e047ed10e5d217c6ef5de96
fetch https://download.sourceforge.net/libpng/libpng-1.6.44.tar.xz 60c4da1d5b7f0aa8d158da48e8f8afa9773c1c8baa5d21974df61f1886b8ce8e
fetch https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.0.4/libjpeg-turbo-3.0.4.tar.gz 99130559e7d62e8d695f2c0eaeef912c5828d5b84a0537dcb24c9678c9d5b76b
for t in zstd-1.5.6.tar.gz lz4-1.10.0.tar.gz unix-tree-2.1.3.tar.gz less-668.tar.gz unzip60.tar.gz zip30.tar.gz jq-1.7.1.tar.gz zlib-1.3.1.tar.xz expat-2.6.4.tar.xz pcre2-10.44.tar.bz2 libpng-1.6.44.tar.xz libjpeg-turbo-3.0.4.tar.gz; do d=${t%.tar.*}; [ -d "$d" ] || [ -d "${d%.*}" ] || tar xf "$t"; done
# zstd (util.h does not include stdio.h), lz4, tree
sed -i '0,/#include <libgen.h>/s//#include <libgen.h>\n#include <stdio.h>/' zstd-1.5.6/programs/util.h
(cd zstd-1.5.6/programs && make -j"$(nproc)" zstd CC="$CC" AR="$T-ar" RANLIB="$T-ranlib" HAVE_THREAD=0 HAVE_PTHREAD=0 ZSTD_LEGACY_SUPPORT=0 HAVE_ZLIB=0 HAVE_LZMA=0 HAVE_LZ4=0 CFLAGS="-O2 -std=gnu17" > "$W/zstd.log" 2>&1)
cp zstd-1.5.6/programs/zstd more/zstd,ff8
(cd lz4-1.10.0 && make -j"$(nproc)" -C programs lz4 CC="$CC" AR="$T-ar" CFLAGS="-O2 -std=gnu17" > "$W/lz4.log" 2>&1)
cp lz4-1.10.0/programs/lz4 more/lz4,ff8
(cd unix-tree-2.1.3 && make -j"$(nproc)" CC="$CC" CFLAGS="-O2 -std=gnu17 -DLINUX" > "$W/tree.log" 2>&1)
cp unix-tree-2.1.3/tree more/tree,ff8
# Info-ZIP: their configure runs test programs, so the Makefile targets are used directly (K&R C: -std=gnu89)
(cd unzip60 && make -f unix/Makefile clean > /dev/null 2>&1; make -f unix/Makefile unzips CC="$CC" CF="-O2 -std=gnu89 -I. -DUNIX -DNO_LCHMOD -DNO_UNICODE_SUPPORT -DHAVE_TERMIOS_H -DTERMIO" LF2="" > "$W/unzip.log" 2>&1)
cp unzip60/unzip more/unzip,ff8
(cd zip30 && make -f unix/Makefile clean > /dev/null 2>&1; echo "" > flags; make -f unix/Makefile zips CC="$CC" CFLAGS="-O2 -std=gnu89 -I. -DUNIX -DUNICODE_SUPPORT=0 -DHAVE_DIRENT_H -DNO_PROTO=0" > "$W/zip.log" 2>&1 || true)	# (zipnote, zipcloak and zipsplit do not link: crc32)
cp zip30/zip more/zip,ff8
mkdir -p less-build && (cd less-build && ../less-668/configure --host=aarch64-riscos --build=$BUILD CC="$CC" CFLAGS="-O2 -std=gnu17" --with-regex=posix > conf.log 2>&1 && make -j"$(nproc)" > make.log 2>&1)
cp less-build/less more/less,ff8; cp less-build/lessecho more/lessecho,ff8
# jq (with its own Oniguruma)
mkdir -p jq-build && (cd jq-build && ../jq-1.7.1/configure --host=aarch64-riscos --build=$BUILD CC="$CC" CFLAGS="-O2 -std=gnu17 -include stdbool.h" --disable-docs --disable-maintainer-mode --with-oniguruma=builtin --disable-shared --enable-all-static > conf.log 2>&1 && make -j"$(nproc)" > make.log 2>&1)
cp jq-build/jq more/jq,ff8
# libraries
(cd zlib-1.3.1 && CC="$CC" AR="$T-ar" RANLIB="$T-ranlib" CFLAGS="-O2" ./configure --static --prefix="$INST" > ../zlib.conf.log 2>&1 && make -j"$(nproc)" > ../zlib.make.log 2>&1 && make install > /dev/null 2>&1)
mkdir -p expat-build && (cd expat-build && ../expat-2.6.4/configure --host=aarch64-riscos --build=$BUILD --prefix="$INST" --disable-shared --without-docbook --without-tests --without-examples CC="$CC" CFLAGS="-O2 -std=gnu17" > conf.log 2>&1 && make -j"$(nproc)" > make.log 2>&1 && make install > /dev/null 2>&1)
mkdir -p pcre2-build && (cd pcre2-build && ../pcre2-10.44/configure --host=aarch64-riscos --build=$BUILD --prefix="$INST" --disable-shared --enable-pcre2-16 --enable-pcre2-32 --disable-jit CC="$CC" CFLAGS="-O2 -std=gnu17" > conf.log 2>&1 && make -j"$(nproc)" > make.log 2>&1 && make install > /dev/null 2>&1)
mkdir -p png-build && (cd png-build && ../libpng-1.6.44/configure --host=aarch64-riscos --build=$BUILD --prefix="$INST" --disable-shared CC="$CC" CPPFLAGS="-I$INST/include" LDFLAGS="-L$INST/lib" CFLAGS="-O2 -std=gnu17" > conf.log 2>&1 && make -j"$(nproc)" > make.log 2>&1 && make install > /dev/null 2>&1)
cat > riscos64.cmake <<EOF
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER $T-gcc)
set(CMAKE_C_FLAGS_INIT "-munixlib -std=gnu17")
set(CMAKE_AR $T-ar)
set(CMAKE_RANLIB $T-ranlib)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
EOF
mkdir -p jpeg-build && (cd jpeg-build && cmake ../libjpeg-turbo-3.0.4 -DCMAKE_TOOLCHAIN_FILE=../riscos64.cmake -DCMAKE_INSTALL_PREFIX="$INST" -DENABLE_SHARED=OFF -DWITH_SIMD=OFF -DWITH_TURBOJPEG=OFF -DWITH_JAVA=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_SKIP_RPATH=ON > conf.log 2>&1 && make -j"$(nproc)" > make.log 2>&1 && make install > /dev/null 2>&1)
for n in cjpeg djpeg jpegtran pngfix xmlwf pcre2grep pcre2test; do cp "$INST/bin/$n" "more/$n,ff8"; done
ls -l more/
