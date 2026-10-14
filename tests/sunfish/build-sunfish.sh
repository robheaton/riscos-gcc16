#!/bin/bash
# build-sunfish.sh SUNFISH_CHECKOUT OUTDIR [TOOLCHAIN]
#
# Builds Sunfish, Alex Waugh's NFS client filing system (https://github.com/ajw498/NFS, MIT licence; revision afa0a8c is the one that was tested), as a RISC OS module with the module kit, from its
# UNMODIFIED source, with no Acorn TCPIPLibs and no libiconv (the kit has sockets, netdb and iconv).  The commands are those of the "gcc" path of src/Makefile (SUNFISHMODOBJS), written for this tool chain:
#   * ProcessSpec (Perl) writes the RPC code (auto/*.c) from the *-spec files;
#   * cmunge makes the module header, and gcc -mmodule compiles the 41 C files (-D_DEFAULT_SOURCE: the sources call strdup and strcasecmp and are compiled with -std=c99, where the kit's headers, like glibc's,
#     declare only what ISO C has);
#   * the four ObjAsm files (base.s, callback.s, free.s, memcpy.s) are assembled by GCCSDK's asasm (any recent GCCSDK has it); `arm-riscos-gnueabihf-asasm-fix` gives its ELF objects the EABI version 5
#     and the section names .text and .rodata that the linker script of modules places;
#   * the link is the usual one-step link (the driver runs modreloc): gcc -mmodule -o Sunfish ... ;
#   * sfcheck and sfnfs, two small UnixLib programs for the tests in README.md, are built with the same tool chain.
# Results in OUTDIR: Sunfish,ffa (the module, file type &FFA), Sunfish.elf, sfcheck,e1f, sfnfs,e1f.
#
# Needs: perl, the tool chain with the kit (the third argument, or TOOLCHAIN), GCCSDK's asasm (ASASM=/path/to/asasm; default: asasm on the PATH), and OSLib's headers (OSLIB=<folder that has
# oslib/os.h>; default: <tool chain>/arm-riscos-gnueabihf/include if install-oslib.py put them there).  OPT sets the optimisation (default: -O0 -mpoke-function-name, the Makefile's debug build).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
SUN=$(readlink -f "${1:?usage: build-sunfish.sh SUNFISH_CHECKOUT OUTDIR [TOOLCHAIN]}")
mkdir -p "${2:?usage: build-sunfish.sh SUNFISH_CHECKOUT OUTDIR [TOOLCHAIN]}"
OUT=$(readlink -f "$2")
TC=$(readlink -f "${3:-${TOOLCHAIN:?give the tool chain as the third argument or in TOOLCHAIN}}")
T=arm-riscos-gnueabihf
ASASM=${ASASM:-asasm}
OSLIB=${OSLIB:-$TC/$T/include}
OPT=${OPT:--O0 -mpoke-function-name}
export PATH=$TC/bin:$PATH
[ -d "$SUN/src" ] && [ -f "$SUN/src/ProcessSpec" ] || { echo "build-sunfish: $SUN is not a checkout of ajw498/NFS (no src/ProcessSpec)" >&2; exit 1; }
[ -f "$OSLIB/oslib/os.h" ] || { echo "build-sunfish: OSLib's headers are not in $OSLIB (set OSLIB=<folder that has oslib/os.h>, or run install-oslib.py)" >&2; exit 1; }
command -v "$ASASM" >/dev/null || { echo "build-sunfish: asasm (GCCSDK's assembler) not found: set ASASM=/path/to/asasm" >&2; exit 1; }
command -v perl >/dev/null || { echo "build-sunfish: perl is needed for ProcessSpec" >&2; exit 1; }

# a copy of the sources: the checkout stays as it is
rm -rf "$OUT/src" && mkdir -p "$OUT/src" && cp -r "$SUN/src/." "$OUT/src/"
cd "$OUT/src"
mkdir -p auto o o-asm
for s in rpc mount1 mount3 nfs2 nfs3 nfs4 portmapper pcnfsd; do perl ProcessSpec "$s" "$s-spec"; done

CFLAGS="-DMODULE -Wall -mthrowback -std=c99 -pedantic $OPT -D_DEFAULT_SOURCE -Iauto -I. -I$OSLIB"
cmunge -32bit -tgcc -throwback cmhg/sunfish -o o/sunfishhdr_m.o -d auto/sunfishdefs.h
cc() { "$T-gcc" -c -mmodule $CFLAGS "${@:3}" -o "o/$1" "$2"; }       # cc OUTPUT SOURCE [extra flags]
GEN="pcnfsd-process1 pcnfsd-process2 portmapper-calls pcnfsd-calls mount1-calls mount3-calls nfs2-calls nfs3-calls rpc-process1 rpc-process2 portmapper-process1 portmapper-process2 mount1-process1 mount1-process2 mount3-process1 mount3-process2 nfs2-process1 nfs2-process2 nfs3-process1 nfs3-process2 nfs4-process1 nfs4-process2"
DUAL="imageentry_func imageentry_file imageentry_args imageentry_bytes imageentry_openclose imageentry_common"
for n in $GEN; do cc "${n}_m.o" "auto/$n.c"; done
for n in $DUAL; do cc "${n}2_m.o" "$n.c"; cc "${n}3_m.o" "$n.c" -DNFS3; done          # (the NFS2 and NFS3 versions of the file operations)
for n in pools utils primitives rpc sunfish imageentry_newimage; do cc "${n}_m.o" "$n.c"; done

# the four assembler files: asasm's ELF objects need the EABI version 5 and the section names of the module linker script (asasm-fix does both)
for s in base callback free memcpy; do "$ASASM" "$s.s" -o "o-asm/$s.o"; done
"$T-asasm-fix" o-asm/base.o o-asm/callback.o o-asm/free.o o-asm/memcpy.o

OBJS=""
for n in $GEN; do OBJS="$OBJS o/${n}_m.o"; done
for n in $DUAL; do OBJS="$OBJS o/${n}2_m.o o/${n}3_m.o"; done
OBJS="$OBJS o-asm/base.o o-asm/callback.o o-asm/memcpy.o o-asm/free.o o/pools_m.o o/utils_m.o o/primitives_m.o o/rpc_m.o o/sunfish_m.o o/imageentry_newimage_m.o o/sunfishhdr_m.o"
"$T-gcc" -mmodule -mthrowback -o "$OUT/Sunfish.elf" $OBJS
"$T-gcc" -mmodule -mthrowback -o "$OUT/Sunfish,ffa" $OBJS
# the test programs
"$T-gcc" -O2 -Wall -o "$OUT/sfcheck,e1f" "$HERE/sfcheck.c"
"$T-gcc" -O2 -Wall -o "$OUT/sfnfs,e1f" "$HERE/sfnfs.c"
echo "build-sunfish: $(ls -l "$OUT/Sunfish,ffa" | awk '{print $5}') bytes: $OUT/Sunfish,ffa  (and sfcheck,e1f, sfnfs,e1f)"
