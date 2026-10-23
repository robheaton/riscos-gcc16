#!/bin/bash
# gapcheck.sh [OUTDIR]: which public functions of the 32-bit UnixLib (libunixlib.a + libm.a of a 32-bit tool chain) are not found by the linker in the 64-bit libraries
# (libc.a libm.a libriscosstub.a libunixlib64.a + the startup objects), by compiling a program that takes the address of each one.  Names that picolibc defines as
# macros (so a name is not a symbol) also show here: check those by compiling a call.   Output: OUTDIR/missing.txt (names), OUTDIR/unresolved.txt (the linker's list).
set -u
O=${1:-/var/tmp/a64/gapcheck}; mkdir -p "$O"
L32=${L32:-/home/robh/gccsdk-next/tc-cxx/arm-riscos-gnueabihf/lib}; N32=${N32:-/home/robh/gccsdk-next/env-f/bin/arm-riscos-gnueabihf-nm}
P=${PREFIX:-/var/tmp/a64/target7-inst}; N64=${N64:-/var/tmp/a64/prefix/bin/aarch64-none-elf-nm}; L64=$P/aarch64-riscos/lib
$N32 -g --defined-only $L32/libunixlib.a $L32/libm.a 2>/dev/null | awk '$2 ~ /^[TWD]$/ {print $3}' | grep -v '^_' | LC_ALL=C sort -u > "$O/names32.txt"
$N64 -g --defined-only $L64/libc.a $L64/libm.a $L64/libriscosstub.a $L64/libunixlib64.a $L64/*.o 2>/dev/null | awk '$2 ~ /^[TWDBR]$/ {print $3}' | grep -v '^_' | LC_ALL=C sort -u > "$O/names64.txt"
LC_ALL=C comm -23 "$O/names32.txt" "$O/names64.txt" | grep -v '^\(DRender\|gmon\|debug_printf\|fastiroot\|tab54\|error_table\|hostalias\)' > "$O/missing.txt"
{ echo '#include <stdio.h>'; awk '{printf "extern void %s(void);\n",$1}' "$O/missing.txt"; echo 'void *tab[] = {'; awk '{printf "(void*)%s,\n",$1}' "$O/missing.txt"; echo '}; int main(void){ return tab[0]==0; }'; } > "$O/chk.c"
"$P/bin/aarch64-riscos-gcc" -munixlib -O0 -w -D_SDK_NO_LFS64_MACROS "$O/chk.c" -o "$O/chk,ff8" 2>&1 | grep -o "undefined reference to \`[A-Za-z0-9_]*'" | sed "s/.*\`//; s/'//" | sort -u > "$O/unresolved.txt"
echo "32-bit public names: $(wc -l < "$O/names32.txt");  not defined in the 64-bit libraries: $(wc -l < "$O/missing.txt");  not found by the linker: $(wc -l < "$O/unresolved.txt")"

# names that are macros in the headers (open64, fopen64, assert, sigemptyset ...) are there for a program that includes the headers: they do not count as missing
HDRS="stdio.h stdlib.h string.h unistd.h fcntl.h sys/stat.h sys/types.h signal.h setjmp.h assert.h pthread.h semaphore.h sys/mman.h dirent.h time.h locale.h wchar.h math.h netdb.h arpa/inet.h sys/socket.h"
{ echo '#define _GNU_SOURCE'; for h in $HDRS; do echo "#include <$h>"; done; awk '{printf "#ifdef %s\nMACRO_%s\n#endif\n",$1,$1}' "$O/unresolved.txt"; } > "$O/macros.c"
"$P/bin/aarch64-riscos-gcc" -munixlib -E -P "$O/macros.c" 2>/dev/null | sed -n "s/^MACRO_//p" | sort -u > "$O/macros.txt"
LC_ALL=C sort -u "$O/unresolved.txt" -o "$O/unresolved.txt"; LC_ALL=C comm -23 "$O/unresolved.txt" "$O/macros.txt" > "$O/really-missing.txt"
echo "of those, macros in the headers (not missing): $(wc -l < "$O/macros.txt");  really missing: $(wc -l < "$O/really-missing.txt")  (list: $O/really-missing.txt)"
