#!/bin/bash
# build-picolibc.sh: builds picolibc (a small C library, BSD-licensed: https://github.com/picolibc/picolibc) for the AArch64 GCC of build-a64-toolchain.sh, for programs of RISC OS 64 (see sdk/).
# The system calls (open, read, write, sbrk, _exit ...) are not in picolibc: sdk/crt/riscos_syscalls.c makes them with SWIs.  Pinned to the commit that was tested.   usage: ./build-picolibc.sh [WORKDIR]
#   A64 = the bin directory of aarch64-none-elf-gcc (default /var/tmp/a64/prefix/bin)    PREFIX = where it is installed (default /var/tmp/a64/picolibc)
# Needs meson and ninja.  The source is cloned into WORKDIR/src and built in WORKDIR/build (a new empty directory: nothing is run from the clone but its build system).
set -e
W=${1:-/var/tmp/a64/picolibc-work}
A64=${A64:-/var/tmp/a64/prefix/bin}
PREFIX=${PREFIX:-/var/tmp/a64/picolibc}
COMMIT=c8df19e                   # picolibc 1.8.12
export PATH=$A64:$PATH
mkdir -p "$W"
[ -d "$W/src" ] || { git clone -q https://github.com/picolibc/picolibc.git "$W/src"; git -C "$W/src" checkout -q $COMMIT; }
[ -e "$W/patched" ] || { git -C "$W/src" apply "$(dirname "$0")/picolibc-freopen-close.patch" && touch "$W/patched"; }   # fclose () of a stream that freopen () gave a file now closes it
mkdir -p "$W/build"
# -ffixed-x18: x18 is reserved on RISC OS 64.  -mstrict-align as in the tests.  No semihosting, our own crt (picocrt off), the console through read/write (posix-console), no thread local storage, a plain
# global errno, one thread, long long and long double in printf and scanf, positional arguments (%1$d) and %n.
meson setup "$W/build" "$W/src" --cross-file "$W/src/scripts/cross-aarch64-none-elf.txt" --prefix="$PREFIX" --buildtype=release \
  -Dmultilib=false -Dsemihost=false -Dpicocrt=false -Dposix-console=true -Dthread-local-storage=false -Dnewlib-global-errno=true -Dsingle-thread=true \
  -Dio-long-long=true -Dio-long-double=true -Dio-pos-args=true -Dprintf-percent-n=true -Dtests=false -Dspecsdir="$PREFIX/lib" "-Dc_args=-ffixed-x18 -mstrict-align" > "$W/configure.log"
ninja -C "$W/build" install > "$W/build.log"
ls -l "$PREFIX/lib/libc.a" "$PREFIX/lib/libm.a"
