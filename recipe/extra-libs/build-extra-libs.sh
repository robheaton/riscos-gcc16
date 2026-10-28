#!/bin/bash
# build-extra-libs.sh TOOLCHAIN_DIR [WORKDIR]
#
# Builds libffi 3.8.0 and OpenSSL 1.1.1w (the series Python 3.8 needs) as static, hard-float libraries for
# arm-riscos-gnueabihf and installs them into the tool chain's own sysroot (TOOLCHAIN_DIR/arm-riscos-gnueabihf),
# next to libc.a, so that  -lffi  -lssl -lcrypto  work without any -I or -L.
#
# Needs: the cross tool chain (riscos-gcc16-cross-16.2.0-24 or later), curl, make, perl, tar.
# Nothing is installed outside TOOLCHAIN_DIR and WORKDIR (default: ./extra-libs-work).
#
# To get a package that you can unpack over a tool chain instead of installing, set STAGE_ONLY=1: the files are
# then collected in WORKDIR/stage/arm-riscos-gnueabihf/ and TOOLCHAIN_DIR is only used for the compiler.
set -eu

TC=$(readlink -f "${1:?usage: build-extra-libs.sh TOOLCHAIN_DIR [WORKDIR]}")
W=$(mkdir -p "${2:-extra-libs-work}" && readlink -f "${2:-extra-libs-work}")
T=arm-riscos-gnueabihf
J=${JOBS:-$(nproc)}
[ -x "$TC/bin/$T-gcc" ] || { echo "no $T-gcc in $TC/bin"; exit 1; }

FFI_VER=3.8.0
FFI_URL=https://github.com/libffi/libffi/releases/download/v$FFI_VER/libffi-$FFI_VER.tar.gz
FFI_SHA=7da3e2d9a171eb0a038f592ecad3ff2bb2550f3496d87b3b29ad0cf4430c0db4
SSL_VER=1.1.1w
SSL_URL=https://www.openssl.org/source/old/1.1.1/openssl-$SSL_VER.tar.gz
SSL_SHA=cf3098950cb4d853ad95c0841f1f9c6d3dc102dccfcacd521d93925208b76ac8

mkdir -p "$W/dl" "$W/src" "$W/build" "$W/stage"
fetch() {	# fetch URL SHA256 FILE
  [ -f "$W/dl/$3" ] || curl -fL -o "$W/dl/$3" "$1"
  echo "$2  $W/dl/$3" | sha256sum -c - >/dev/null || { echo "checksum of $3 does not match"; exit 1; }
}
fetch "$FFI_URL" "$FFI_SHA" libffi-$FFI_VER.tar.gz
fetch "$SSL_URL" "$SSL_SHA" openssl-$SSL_VER.tar.gz

if [ "${STAGE_ONLY:-0}" = 1 ]; then ROOT=$W/stage; else ROOT=$TC; fi
SYSROOT=$ROOT/$T
export PATH=$TC/bin:$PATH

# ---- libffi: VFP calling convention, closures through malloc (no mmap on RISC OS)
rm -rf "$W/src/libffi-$FFI_VER" "$W/build/ffi"; mkdir -p "$W/build/ffi"
tar -C "$W/src" -xf "$W/dl/libffi-$FFI_VER.tar.gz"
( cd "$W/build/ffi" &&
  CC=$TC/bin/$T-gcc AR=$TC/bin/$T-ar RANLIB=$TC/bin/$T-ranlib CFLAGS=-O2 \
  "$W/src/libffi-$FFI_VER/configure" --host=$T --prefix=/$T --disable-shared --enable-static --disable-docs --disable-multi-os-directory > configure.log 2>&1 &&
  make -j"$J" > make.log 2>&1 &&
  make install DESTDIR="$ROOT" > install.log 2>&1 )
rm -f "$SYSROOT/lib/libffi.la"

# ---- OpenSSL: generic 32-bit C code (no assembler, no engines that need /dev/crypto); thread safe (pthreads of UnixLib)
#   RAND_bytes reads /dev/urandom, which UnixLib serves from the CryptRandom module.
#   The compiled-in certificate directory is /OpenSSL: it does not exist; give programs their CA bundle explicitly
#   (SSL_CERT_FILE, or the cafile argument of Python's ssl module).
rm -rf "$W/src/openssl-$SSL_VER" "$W/build/ssl"; mkdir -p "$W/build/ssl"
tar -C "$W/src" -xf "$W/dl/openssl-$SSL_VER.tar.gz"
cp -r "$W/src/openssl-$SSL_VER/." "$W/build/ssl/"
#   OpenSSL's BSD-generic32 target adds -pthread (and -lpthread), which this tool chain does not have: threads are in
#   UnixLib.  The target below is BSD-generic32 without them (the thread flags -D_THREAD_SAFE -D_REENTRANT stay).
cat > "$W/riscos-gnueabihf.conf" <<'CONF'
my %targets = (
    "riscos-gnueabihf" => {
        inherit_from => [ "BSD-generic32" ],
        cflags       => "",
        cppflags     => threads("-D_THREAD_SAFE -D_REENTRANT"),
        ex_libs      => "",
    },
);
CONF
( cd "$W/build/ssl" &&
  ./Configure riscos-gnueabihf --config="$W/riscos-gnueabihf.conf" --cross-compile-prefix=$TC/bin/$T- --prefix=/$T --openssldir=/OpenSSL \
    no-shared no-pic no-asm no-dso no-async no-tests no-ui-console no-devcryptoeng -O2 > configure.log 2>&1 &&
  make -j"$J" CC=$TC/bin/$T-gcc build_libs > make.log 2>&1 &&
  make install_dev DESTDIR="$ROOT" CC=$TC/bin/$T-gcc > install.log 2>&1 )

# ---- the .pc files find their own folder, so the sysroot can be moved
for f in "$SYSROOT"/lib/pkgconfig/{libffi,libssl,libcrypto,openssl}.pc; do
  sed -i -e 's|^prefix=.*|prefix=${pcfiledir}/../..|' -e 's|^libdir=.*|libdir=${prefix}/lib|' -e 's|^includedir=.*|includedir=${prefix}/include|' -e 's|^toolexeclibdir=.*|toolexeclibdir=${libdir}|' "$f"
done

# ---- the licences go with the libraries
mkdir -p "$ROOT/share/licenses/riscos-extra-libs"
cp "$W/src/libffi-$FFI_VER/LICENSE" "$ROOT/share/licenses/riscos-extra-libs/libffi-LICENSE"
cp "$W/src/openssl-$SSL_VER/LICENSE" "$ROOT/share/licenses/riscos-extra-libs/openssl-LICENSE"
echo "done: $SYSROOT/lib/{libffi,libssl,libcrypto}.a  $SYSROOT/include/{ffi.h,ffitarget.h,openssl/}"
