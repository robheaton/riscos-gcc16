#!/bin/bash
# build-sdk.sh: builds the support objects of aarch64-riscos-gcc (the AIF64 header, the start-up code, the system calls) into sdk/lib, with the AArch64 GCC of tools/build-a64-toolchain.sh.
# picolibc itself is built by tools/build-picolibc.sh (meson).   usage: ./build-sdk.sh        A64=<bin directory of aarch64-none-elf-gcc>
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
A64=${A64:-/var/tmp/a64/prefix/bin}
PICO=${PICOLIBC:-/var/tmp/a64/picolibc}
CC=$A64/aarch64-none-elf-gcc
CFLAGS="-O2 -ffreestanding -ffixed-x18 -mstrict-align -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -isystem $PICO/include"
mkdir -p "$HERE/lib"
for f in aifhdr crt0 swi; do $CC $CFLAGS -c "$HERE/crt/$f.S" -o "$HERE/lib/$f.o"; done
for f in riscos_start riscos_syscalls; do $CC $CFLAGS -c "$HERE/crt/$f.c" -o "$HERE/lib/$f.o"; done
ls -l "$HERE/lib"
