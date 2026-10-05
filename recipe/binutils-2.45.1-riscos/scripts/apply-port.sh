#!/bin/bash
# apply-port.sh <binutils-2.45.1-source-dir>
# Applies the RISC OS (arm-riscos-gnueabihf, GCCSDK EABI) port to a pristine binutils 2.45.1 tree.
# Download: https://ftp.gnu.org/gnu/binutils/binutils-2.45.1.tar.xz
set -e
SRC=${1:?usage: apply-port.sh <binutils-2.45.1-source-dir>}
R=$(cd "$(dirname "$0")/.." && pwd)
cd "$SRC"
test -f bfd/elf32-arm.c || { echo "$SRC does not look like a binutils tree"; exit 1; }
for p in "$R"/patches/*.patch; do
  echo "applying $(basename "$p")"
  patch -p1 --no-backup-if-mismatch < "$p"
done
echo "done: new files gas/config/te-riscos.h and ld/emulparams/armelf_riscos_eabi.sh are part of the patches"
