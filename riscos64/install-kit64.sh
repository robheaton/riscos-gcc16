#!/bin/bash
# install-kit64.sh PREFIX [--toolchain BINDIR] [--picolibc DIR]: installs the RISC OS 64 module kit and SDK into PREFIX:
#   PREFIX/bin/aarch64-riscos-gcc, cmunge64, modreloc64, mkaif64, mkoslib   the commands (links; aarch64-riscos-gcc finds the rest from its own place)
#   PREFIX/riscos64/{sdk,kit,tools,out/kit-lib}   the driver, the support objects, the headers and libraries of the kit (libmodkit64.a, libmodcxx64.a, modrt64.o ...), module64.mk
#   PREFIX/modkit/{include,lib,bin}              the headers and sources of the C library of the 32-bit kit (the 64-bit library is built from them) and mkmodhdr.py, mkoslib.py
# --toolchain: the bin folder of aarch64-none-elf-gcc (tools/build-a64-toolchain.sh; default /var/tmp/a64/prefix/bin, C and C++); --picolibc: where picolibc is installed (tools/build-picolibc.sh), for programs.
# The libraries are built here, with that compiler.  Needs python3.
set -e
P=${1:?usage: install-kit64.sh PREFIX [--toolchain BINDIR] [--picolibc DIR]}; shift
A64=${A64:-/var/tmp/a64/prefix/bin}; PICO=${PICOLIBC:-/var/tmp/a64/picolibc}
while [ $# -gt 0 ]; do case "$1" in --toolchain) A64=$2; shift;; --picolibc) PICO=$2; shift;; *) echo "install-kit64: unknown option $1" >&2; exit 1;; esac; shift; done
HERE=$(cd "$(dirname "$0")" && pwd); K=$HERE/../modkit
[ -x "$A64/aarch64-none-elf-gcc" ] || { echo "install-kit64: no aarch64-none-elf-gcc in $A64 (tools/build-a64-toolchain.sh builds it)" >&2; exit 1; }
mkdir -p "$P/bin" "$P/riscos64" "$P/modkit"
P=$(cd "$P" && pwd)
for d in sdk kit tools; do rm -rf "$P/riscos64/$d"; mkdir -p "$P/riscos64/$d"; tar -C "$HERE/$d" --exclude=lib --exclude=__pycache__ -cf - . | tar -C "$P/riscos64/$d" -xf -; done
for d in include lib bin; do rm -rf "$P/modkit/$d"; mkdir -p "$P/modkit/$d"; tar -C "$K/$d" --exclude=__pycache__ -cf - . | tar -C "$P/modkit/$d" -xf -; done
printf 'A64=%s\nPICOLIBC=%s\n' "$A64" "$PICO" > "$P/riscos64/sdk/config.sh"
A64=$A64 "$P/riscos64/kit/build-kit.sh" > /dev/null
[ -d "$PICO/lib" ] && A64=$A64 PICOLIBC=$PICO "$P/riscos64/sdk/build-sdk.sh" > /dev/null || echo "install-kit64: no picolibc in $PICO: programs (not modules) cannot be built until you run tools/build-picolibc.sh and install again" >&2
ln -sf ../riscos64/sdk/bin/aarch64-riscos-gcc "$P/bin/aarch64-riscos-gcc"
printf '#!/bin/sh\nexec python3 "$(dirname "$(readlink -f "$0")")/../riscos64/kit/cmunge64.py" "$@"\n' > "$P/bin/cmunge64"
printf '#!/bin/sh\nexec python3 "$(dirname "$(readlink -f "$0")")/../riscos64/tools/modreloc64.py" "$@"\n' > "$P/bin/modreloc64"
printf '#!/bin/sh\nexec python3 "$(dirname "$(readlink -f "$0")")/../riscos64/tools/mkaif64.py" "$@"\n' > "$P/bin/mkaif64"
printf '#!/bin/sh\nMKOSLIB_NM=%s/aarch64-none-elf-nm exec python3 "$(dirname "$(readlink -f "$0")")/../modkit/bin/mkoslib.py" "$@"\n' "$A64" > "$P/bin/mkoslib"
chmod +x "$P/bin/cmunge64" "$P/bin/modreloc64" "$P/bin/mkaif64" "$P/bin/mkoslib"
echo "installed in $P: put $P/bin on PATH; a module:  aarch64-riscos-gcc -mmodule -O2 m.c m_hdr.o -o M   (see $P/riscos64/kit/README.md and module64.mk)"
