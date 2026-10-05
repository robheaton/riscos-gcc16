#!/bin/bash
# GNU make 4.4.1 cross-built FOR RISC OS (arm-riscos-gnueabihf), to run natively next to the native GCC 16 tool chain (bin/make of the !GCC16 package).
# usage: build-make.sh [TARBALL] [WORKDIR]
#   TARBALL  default $HOME/gccsdk-next/src/make-4.4.1.tar.gz     (https://ftp.gnu.org/gnu/make/make-4.4.1.tar.gz)
#   WORKDIR  default $HOME/gccsdk-next/make-riscos               (src = patched source, build = out of tree, install/bin/make = the result)
# Needs the cross toolchain env-f ($HOME/gccsdk-next/env-f) and the GCC recipe's data/ (riscos-da.c, nolibm).
#
# Facts that matter:
#   * make 4.4.1 already knows __riscos__: no shell is ever used (sh_chars is empty: every recipe line is split and exec'd directly; UnixLib's execve turns the program
#     name into a RISC OS command line, so programs are found through Run$Path and *commands such as Copy or Delete also work), and no load-average code.
#   * -std=gnu17: gnulib's fnmatch.c still declares  extern char *getenv ();  which GCC 15+'s default (gnu23) rejects.
#   * --disable-job-server: -jN cannot run anything in parallel on RISC OS (a vfork child runs to completion before vfork returns), and the job server's blocking
#     token pipe is a deadlock risk; --disable-posix-spawn keeps make on vfork (UnixLib's vfork + exec is the path that is proven).
#   * the same link flags as the native compiler: the empty libm placeholder (a program with libm.so.1 in DT_NEEDED cannot be a vfork child) and riscos-da.o (the heap in a
#     dynamic area: with the heap in the Wimp slot every vfork of a recipe would copy the whole of make's heap, and a big child could grow over it).  The heap maximum is
#     UnixLib's default of 32 MB: make, gcc, collect2 and ld are alive together and their reservations of address space add up (512 MB each did not fit).
#     riscos-stack-8m.o gives make a main stack of 8 MB (needs libunixlib 16.2.0-6; older ones keep 1 MB).
set -eu
G=${GCCNEXT:-$HOME/gccsdk-next}
TARBALL=${1:-$G/src/make-4.4.1.tar.gz}
W=${2:-$G/make-riscos}
HERE=$(cd "$(dirname "$0")/.." && pwd)
GR=$G/recipe/gcc-16.2.0-riscos
E=$G/env-f
export PATH=$E/bin:/usr/bin:/bin:/usr/local/bin
export ac_cv_func_shl_load=no ac_cv_lib_dld_shl_load=no
mkdir -p "$W"; cd "$W"
[ -d src ] && mv src "src-prev-$(date +%Y%m%d-%H%M%S)"
mkdir src && tar -xzf "$TARBALL" -C src --strip-components=1
for p in "$HERE"/patches/*.patch; do echo "applying $(basename "$p")"; (cd src && patch -p1 --no-backup-if-mismatch < "$p"); done
rm -rf build install; mkdir build; cd build
arm-riscos-gnueabihf-gcc -O2 -c "$GR/data/riscos-da.c" -o riscos-da.o
arm-riscos-gnueabihf-gcc -O2 -c "$GR/data/riscos-stack-8m.c" -o riscos-stack-8m.o      # int __stack_size = 8 MB (libunixlib 16.2.0-6 and later; older ones ignore it)
../src/configure --build=x86_64-pc-linux-gnu --host=arm-riscos-gnueabihf --prefix=/RISCOS/make \
  --disable-nls --without-guile --disable-load --disable-job-server --disable-posix-spawn --disable-dependency-tracking \
  CFLAGS="-O2 -std=gnu17" LDFLAGS="-L$GR/data/nolibm $PWD/riscos-da.o $PWD/riscos-stack-8m.o -static-libgcc -Wl,--allow-shlib-undefined" > configure.log 2>&1
make -j"$(nproc)" > make.log 2>&1
mkdir -p ../install/bin
arm-riscos-gnueabihf-strip --strip-all -o ../install/bin/make make
ls -la ../install/bin/make
