#!/bin/bash
# refresh-sdk.sh PREFIX [WORKDIR]: puts the current SDK files (sdk/include, the start-up files, the system calls, libriscosstub.a) and UnixLib 64 into an installed aarch64-riscos tool chain (made by build-target.sh),
# without building the compiler again.  Use it after changing something under riscos64/sdk or riscos64/unixlib.
set -e
HERE=$(cd "$(dirname "$0")" && pwd); SDK=$HERE/../sdk
PREFIX=$(cd "${1:?PREFIX of the installed tool chain}" && pwd); W=${2:-/var/tmp/refresh-sdk.$$}
T=$PREFIX/aarch64-riscos; CC=$PREFIX/bin/aarch64-riscos-gcc
mkdir -p "$W/stubs"
cp -a "$SDK/include/." "$T/include/"
python3 "$SDK/fix-picolibc-headers.py" "$T/include"
for f in aifhdr.S crt0.S swi.S riscos_start.c riscos_syscalls.c; do
  $CC -O2 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -c "$SDK/crt/$f" -o "$T/lib/${f%.*}.o"
done
cp "$SDK/aif64.ld" "$T/lib/"
rm -f "$W"/stubs/*.o "$T/lib/libriscosstub.a"
for f in "$SDK"/crt/stubs/*.c "$SDK"/crt/posix/*.c; do $CC -O2 -ffreestanding -fno-pic -fno-pie -fexceptions -Wall -Wextra -I"$SDK/include" -c "$f" -o "$W/stubs/$(basename "${f%.c}").o"; done
"$PREFIX/bin/aarch64-riscos-ar" rcs "$T/lib/libriscosstub.a" "$W"/stubs/*.o
UL=$SDK/../unixlib
CC=$CC AR=$PREFIX/bin/aarch64-riscos-ar "$UL/build.sh" "$W/unixlib" > "$W/unixlib-build.log" 2>&1
cp "$W/unixlib/libunixlib64.a" "$T/lib/" && mkdir -p "$T/include/unixlib" && cp "$UL/include/unixlib/local.h" "$T/include/unixlib/"
echo "SDK refreshed in $T"
