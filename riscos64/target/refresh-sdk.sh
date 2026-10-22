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
# picolibc's locale functions know only the "C" locale: they become __pico_<name> and crt/posix/locale.c has the ones that the programs see (once: a name that is renamed already is not renamed again)
for n in setlocale nl_langinfo nl_langinfo_l localeconv strftime strftime_l strptime strcoll strcoll_l strxfrm strxfrm_l wcscoll wcsxfrm; do
  if "$PREFIX/bin/aarch64-riscos-nm" "$T/lib/libc.a" 2>/dev/null | grep -q " [TW] $n\$"; then "$PREFIX/bin/aarch64-riscos-objcopy" --redefine-sym $n=__pico_$n "$T/lib/libc.a"; fi
done
# picolibc's vfprintf (every printf function ends in it) becomes __pico_vfprintf and crt/libc-patch/numfmt.c has the vfprintf that the programs see (LC_NUMERIC): the member is renamed and the new one is
# put into libc.a itself (libc.a comes after libriscosstub.a on the link line, so printf.o's reference to vfprintf is found there); a library that has been done already is not done again
if ! "$PREFIX/bin/aarch64-riscos-nm" "$T/lib/libc.a" 2>/dev/null | grep -q ' [TW] __pico_vfprintf$'; then
  VW=$(mktemp -d); (cd "$VW" && "$PREFIX/bin/aarch64-riscos-ar" x "$T/lib/libc.a" libc_stdio_vfprintf.c.o && "$PREFIX/bin/aarch64-riscos-objcopy" --redefine-sym vfprintf=__pico_vfprintf libc_stdio_vfprintf.c.o && "$PREFIX/bin/aarch64-riscos-ar" r "$T/lib/libc.a" libc_stdio_vfprintf.c.o); rm -rf "$VW"
fi
VO=$(mktemp -d); $CC -O2 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -c "$SDK/crt/libc-patch/numfmt.c" -o "$VO/ul64_numfmt.o" && "$PREFIX/bin/aarch64-riscos-ar" r "$T/lib/libc.a" "$VO/ul64_numfmt.o"; rm -rf "$VO"
if "$PREFIX/bin/aarch64-riscos-nm" "$T/lib/libc.a" 2>/dev/null | grep -q ' [TW] sysconf$'; then "$PREFIX/bin/aarch64-riscos-objcopy" --redefine-sym sysconf=__pico_sysconf "$T/lib/libc.a"; fi
# <limits.h>: GCC's own header is found first and (installed by a compiler built without headers) does not go on to the C library's: NAME_MAX, PATH_MAX, the _POSIX_ limits ... come from <sys/syslimits.h>, added to its end
GCCLIM=$PREFIX/lib/gcc/aarch64-riscos/$("$PREFIX/bin/aarch64-riscos-gcc" -dumpversion)/include/limits.h
sed -i 's|^#define MB_LEN_MAX 1$|#define MB_LEN_MAX 16|' "$GCCLIM"	# MB_LEN_MAX must be at least MB_CUR_MAX (6) of the UTF-8 locales (wcrtomb into a buffer of MB_LEN_MAX bytes)
grep -q 'SDK: syslimits' "$GCCLIM" || printf '\n/* SDK: syslimits */\n#include <sys/syslimits.h>\n#ifndef SSIZE_MAX\n#define SSIZE_MAX __LONG_MAX__\n#endif\n' >> "$GCCLIM"
rm -f "$W"/stubs/*.o "$T/lib/libriscosstub.a"
for f in "$SDK"/crt/stubs/*.c "$SDK"/crt/posix/*.c; do $CC -O2 -ffreestanding -fno-pic -fno-pie -fexceptions -Wall -Wextra -I"$SDK/include" -c "$f" -o "$W/stubs/$(basename "${f%.c}").o"; done
"$PREFIX/bin/aarch64-riscos-ar" rcs "$T/lib/libriscosstub.a" "$W"/stubs/*.o
rm -f "$T/lib/libtermcap.a"; "$PREFIX/bin/aarch64-riscos-ar" rcs "$T/lib/libtermcap.a"	# (empty: the termcap functions are in libriscosstub.a; configure scripts look for -ltermcap)
UL=$SDK/../unixlib
CC=$CC AR=$PREFIX/bin/aarch64-riscos-ar "$UL/build.sh" "$W/unixlib" > "$W/unixlib-build.log" 2>&1
cp "$W/unixlib/libunixlib64.a" "$T/lib/" && mkdir -p "$T/include/unixlib" && cp "$UL/include/unixlib/local.h" "$T/include/unixlib/"
echo "SDK refreshed in $T"
