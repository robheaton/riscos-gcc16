#!/bin/bash
# mkoslib-lib.sh TOOLCHAIN_DIR [OSLIB_INCLUDE_DIR]  -  builds libOSLib32.a, the OSLib of the EABI: every OSLib function (the X-functions xos_cli ... and the ones that raise an error: os_cli ...) as a
# small veneer (mkoslib --library, from OSLib's own headers), one object each, so that  -lOSLib32  in the Makefile of a GCCSDK 4.7.4 module links with this tool chain unchanged.
# The veneers call the SWI through __modlib_xswi (OS_CallASWI) and raise an error through __modlib_raise (OS_GenerateError); both are in libmodkit-core.a, and the archive has its own weak copies
# (oslibsup.o) so that a program that is not a module links too.  OSLIB_INCLUDE_DIR is the oslib folder of the headers (default: $OSLIB/oslib, else ~/gccsdk/env/include/oslib).
set -eu
TC=$(readlink -f "${1:?usage: mkoslib-lib.sh TOOLCHAIN_DIR [OSLIB_INCLUDE_DIR]}")
K=$(readlink -f "$(dirname "$(readlink -f "$0")")/..")
T=arm-riscos-gnueabihf
INC=${2:-${OSLIB:+$OSLIB/oslib}}; INC=${INC:-$HOME/gccsdk/env/include/oslib}
CC=$TC/bin/$T-gcc; AR=$TC/bin/$T-ar
[ -f "$INC/os.h" ] || { echo "mkoslib-lib: no OSLib headers in $INC" >&2; exit 1; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
if [ -x "$TC/bin/$T-mkoslib" ]; then "$TC/bin/$T-mkoslib" -I "$INC" --library "$W/c" >/dev/null 2>"$W/skipped.txt"
else python3 "$K/bin/mkoslib.py" -I "$INC" --library "$W/c" >/dev/null 2>"$W/skipped.txt"; fi
( cd "$W/c" && : > "$W/failed.txt" && ls *.c | xargs -P "$(nproc)" -I{} sh -c '"$0" -mmodule -O2 -std=gnu99 -w -I"$1" -c "$2" -o "${2%.c}.o" 2>/dev/null || echo "$2" >> "$3"' "$CC" "$(dirname "$INC")" {} "$W/failed.txt" )
# the weak copies of the support routines (the strong ones of libmodkit-core.a win when both are linked)
sed -e 's/^\t\.global\t/\t.weak\t/' "$K/lib/modswi.S" > "$W/oslibsup.S"
"$CC" -march=armv6 -c "$W/oslibsup.S" -o "$W/c/oslibsup.o"
# OSLib's headers go with the library (#include <oslib/os.h> as with GCCSDK 4.7.4)
rm -rf "$TC/$T/include/oslib" && mkdir -p "$TC/$T/include" && cp -r "$INC" "$TC/$T/include/oslib"
# (-mmodule searches include-modkit and not the include folder of the sysroot: a link there)
VER=$("$CC" -dumpversion)
[ -d "$TC/lib/gcc/$T/$VER/include-modkit" ] && ln -sfn "../../../../../$T/include/oslib" "$TC/lib/gcc/$T/$VER/include-modkit/oslib"
rm -f "$TC/$T/lib/libOSLib32.a"
( cd "$W/c" && "$AR" rcs "$TC/$T/lib/libOSLib32.a" oslibsup.o $(ls *.o | grep -v '^oslibsup.o$') )
echo "libOSLib32.a: $(ls "$W"/c/*.o | wc -l) objects, $(wc -l < "$W/skipped.txt") functions skipped (Toolbox calls that are messages, not SWIs), $(wc -l < "$W/failed.txt") that do not compile, $(stat -c %s "$TC/$T/lib/libOSLib32.a") bytes"
