#!/bin/bash
# compare-4.7.4-names.sh GCCSDK_DIR TOOLCHAIN_DIR - which public names of GCCSDK 4.7.4's module library does the module kit lack?
#
# GCCSDK 4.7.4 links a module that is built with -mmodule (its default: -mfloat-abi=hard -mfpu=fpa) against libunixlib.a and libm.a of its scl-module/hard/fpa folder: UnixLib's own additions to the C
# library, and the stubs that call the Shared C Library's entries (chunks 1, 2, 4 and 5: the ISO C and C99 functions that the Shared C Library of RISC OS has).  This script lists the names that those two
# libraries define (global code and data symbols that do not start with an underscore) and says which of them the kit defines in libmodkit-core.a and libgcc-mod.a of the tool chain.  VARIANT=softfp/vfp
# (or another folder under scl-module) compares with that variant instead; it has no stubs for the Shared C Library.  A name that the kit has only as an inline function of a header (imaxabs, imaxdiv, strtoimax, strtoumax in <inttypes.h>) is listed as
# missing: a source that uses it compiles and links all the same.
# usage: [VARIANT=hard/fpa] compare-4.7.4-names.sh ~/gccsdk <tool chain folder>      (GCCSDK_DIR has cross/bin/arm-unknown-riscos-nm and cross/arm-unknown-riscos/lib/scl-module/hard/fpa)
set -eu
export LC_ALL=C
G=$(readlink -f "${1:?usage: compare-4.7.4-names.sh GCCSDK_DIR TOOLCHAIN_DIR}")
T=$(readlink -f "${2:?usage: compare-4.7.4-names.sh GCCSDK_DIR TOOLCHAIN_DIR}")
L=$G/cross/arm-unknown-riscos/lib/scl-module/${VARIANT:-hard/fpa}
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
names() { awk '$2=="T"||$2=="W"||$2=="D"||$2=="B"||$2=="R"||$2=="V"{print $3}' | grep -v '^_' | sort -u; }
"$G/cross/bin/arm-unknown-riscos-nm" -g --defined-only "$L/libunixlib.a" "$L/libm.a" 2>/dev/null | names > "$W/theirs"
"$T/bin/arm-riscos-gnueabihf-nm" -g --defined-only "$T/arm-riscos-gnueabihf/lib/libmodkit-core.a" "$T/arm-riscos-gnueabihf/lib/libgcc-mod.a" 2>/dev/null | names > "$W/ours"
echo "public names in GCCSDK 4.7.4's module library, ${VARIANT:-hard/fpa} (libunixlib.a + libm.a): $(wc -l < "$W/theirs")"
echo "public names in the kit (libmodkit-core.a + libgcc-mod.a):              $(wc -l < "$W/ours")"
echo "names of 4.7.4's that the kit defines:                                  $(comm -12 "$W/theirs" "$W/ours" | wc -l)"
echo "names of 4.7.4's that the kit does not define:                          $(comm -23 "$W/theirs" "$W/ours" | wc -l)"
comm -23 "$W/theirs" "$W/ours" | sed 's/^/  /'
echo "names that only the kit has:                                            $(comm -13 "$W/theirs" "$W/ours" | wc -l)"
