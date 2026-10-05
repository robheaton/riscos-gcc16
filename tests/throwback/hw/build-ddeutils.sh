#!/bin/bash
# Build a loadable DDEUtils module (1.75, 12 Jul 2015) from the RISC OS Open sources (Apache 2.0): the throwback receiver tests need the module, and a machine that does not have it
# (the one this was written for has none loaded and none on the disk) can RMLoad this one.  The sources are the RiscOS tree of a BCM2835 / Raspberry Pi build (Sources/Programmer/DDEUtils and the
# header files that it includes from the other components); the assembler is GCCSDK's asasm (ObjAsm compatible); the module is one position independent area, so the raw bytes of that area ARE
# the module file (type &FFA).  NOT the ROM build (that is compressed): this is a plain build, untested on a machine until the hardware pack runs it.
#   usage: build-ddeutils.sh OUTFILE RISCOS_SOURCES_DIR   RISCOS_SOURCES_DIR = a checkout of the RISC OS Open sources (https://gitlab.riscosopen.org/RiscOS/Sources) with Programmer/DDEUtils
set -eu
OUT=${1:?usage: build-ddeutils.sh OUTFILE RISCOS_SOURCES_DIR}
SRC=${2:?usage: build-ddeutils.sh OUTFILE RISCOS_SOURCES_DIR}
ASASM=${ASASM:-$HOME/gccsdk/cross/bin/asasm}
OBJCOPY=${OBJCOPY:-$HOME/gccsdk/env/arm-riscos-gnueabihf/bin/objcopy}
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
D=$SRC/Programmer/DDEUtils
# Hdr: = HdrSrc's hdr directory (with Machine/, CPU/ ...) plus the hdr files of every other component (OSRSI6, Wimp, ModHand ...)
cp -rL "$SRC/Programmer/HdrSrc/hdr" "$W/Hdr"
for d in $(find "$SRC" -type d -name hdr | grep -v HdrSrc | sort); do for f in "$d"/*; do b=$(basename "$f"); [ -e "$W/Hdr/$b" ] || cp -rL "$f" "$W/Hdr/$b"; done; done 2>/dev/null || true
mkdir -p "$W/src"; cp -r "$D/s" "$D/hdr" "$D/VersionASM" "$W/src/"
ln -sf Debug "$W/src/s/debug"                       # the source says s.debug, the file is s/Debug (a case sensitive file system)
cd "$W/src"
# on a non RISC OS host asasm takes the path variable Hdr: from the environment variable HDR_PATH, and <Machine> from MACHINE
HDR_PATH=$W/Hdr MACHINE=RPi "$ASASM" -elf -32 -i hdr -i s -i . -o "$W/ddeutils.o" s/ddeutils 2>"$W/asasm.log" || { cat "$W/asasm.log"; exit 1; }
"$OBJCOPY" -O binary --only-section='ddeutils$$module' "$W/ddeutils.o" "$OUT"
python3 - "$OUT" <<'PY'
import struct, sys
d = open(sys.argv[1], "rb").read()
h = struct.unpack("<13I", d[:52])
s = lambda o: d[o:d.index(b"\0", o)].decode("latin-1")
assert h[0] == 0 and h[7] == 0x42580, "not the DDEUtils module header"
print("%s: %d bytes, \"%s\", %s, SWI chunk &%X, flags &%X (bit 0: 32 bit neutral)" % (sys.argv[1], len(d), s(h[4]), repr(s(h[5])), h[7], struct.unpack("<I", d[h[12]:h[12] + 4])[0]))
PY
