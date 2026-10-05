#!/bin/bash
# build HelloMod2 with the GCC 16 EABI cross compiler: hello2,ffa = the flat module image (relocation table appended), hello2.elf for inspection
set -e
HERE=$(cd "$(dirname "$0")" && pwd); cd "$HERE"
BIN=${BIN:-$HOME/gccsdk-next/env-f/bin}; CC=$BIN/arm-riscos-gnueabihf-gcc; LD=$BIN/arm-riscos-gnueabihf-ld
CFLAGS="-O2 -std=gnu11 -march=armv6 -mfloat-abi=soft -marm -ffreestanding -fno-pic -fno-pie -fvisibility=hidden -fno-stack-clash-protection -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-exceptions -fno-builtin -Wall -Wextra"
$CC $CFLAGS -c hello2.c -o hello2.o
$CC -march=armv6 -c hello2_hdr.s -o hello2_hdr.o
$LD -T hello2.ld -static -nostdlib -q -o hello2.elf hello2_hdr.o hello2.o 2>&1 | grep -v "RWX permissions\|dynamic-undefined-weak" || true
python3 ../tools/modreloc.py hello2.elf "hello2,ffa"
