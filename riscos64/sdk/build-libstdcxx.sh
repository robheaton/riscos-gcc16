#!/bin/bash
# Build libstdc++ (and libsupc++) of GCC 16.2 for the SDK's AArch64 compiler, so that the C++ library (iostream, string, vector, map, exceptions ...) can be linked into programs by aarch64-riscos-g++.
# Needs: the cross compiler aarch64-none-elf-{gcc,g++} with C++ (riscos64/tools/build-a64-toolchain.sh), picolibc installed in the SDK's place, the GCC 16.2 sources.
#   usage: build-libstdcxx.sh GCC-SOURCE-DIR TOOLCHAIN-PREFIX BUILD-DIR     (BUILD-DIR is created; nothing in it is deleted)
set -e
SRC=${1:?GCC source directory (gcc-16.2.0)}; PREFIX=${2:?toolchain prefix}; B=${3:?new build directory}
SDK=$(cd "$(dirname "$0")" && pwd)
[ -e "$B" ] && { echo "$B exists: give a new directory"; exit 1; }
mkdir -p "$B" && cd "$B"
# the SDK driver as the compiler, in the form the configure script wants: the file name as asked for (RISCOS64_RAW), and for C code the dialect of 2016 (GCC 16 defaults to C23)
cat > cc64 <<EOT
#!/bin/bash
export RISCOS64_RAW=1
exec $SDK/bin/aarch64-riscos-gcc -std=gnu99 -fpermissive -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types -Wno-implicit-int "\$@"
EOT
cat > cxx64 <<EOT
#!/bin/bash
export RISCOS64_RAW=1 RISCOS64_CXX=1
exec $SDK/bin/aarch64-riscos-gcc "\$@"
EOT
chmod +x cc64 cxx64
export PATH=$PREFIX/bin:$PATH
# the dual ABI stays on: with --disable-libstdcxx-dual-abi the C++20 <chrono> time zone code does not compile
"$SRC/libstdc++-v3/configure" --host=aarch64-none-elf --build=$(gcc -dumpmachine) --prefix="$PREFIX" --disable-shared --enable-static --disable-multilib --disable-nls --with-newlib \
  --disable-libstdcxx-pch --with-default-libstdcxx-abi=new --disable-libstdcxx-threads --disable-libstdcxx-filesystem-ts --disable-libstdcxx-backtrace \
  CC="$B/cc64" CXX="$B/cxx64" AR=aarch64-none-elf-ar RANLIB=aarch64-none-elf-ranlib AS=aarch64-none-elf-as LD=aarch64-none-elf-ld NM=aarch64-none-elf-nm > configure.log 2>&1
make -j"$(nproc)" > make.log 2>&1
make install > install.log 2>&1
echo "libstdc++.a and libsupc++.a installed in $PREFIX/lib; headers in $PREFIX/include/c++ (aarch64-riscos-g++ finds them)"
