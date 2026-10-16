#!/bin/bash
# build-kit.sh: builds the runtime objects of the module kit (kit/lib/modrt64.o, modswi64.o) with the AArch64 tool chain.  Run by sdk/build-sdk.sh too.
set -e
KIT=$(cd "$(dirname "$0")" && pwd)
A64=${A64:-/var/tmp/a64/prefix/bin}
CC=$A64/aarch64-none-elf-gcc
. "$KIT/modflags.sh"
mkdir -p "$KIT/../out/kit-lib"
$CC $MODFLAGS -O2 -Wall -Wextra -I"$KIT/include" -isystem "$KIT/../../modkit/include" -c "$KIT/modrt64.c" -o "$KIT/../out/kit-lib/modrt64.o"
$CC $MODFLAGS -c "$KIT/modswi64.S" -o "$KIT/../out/kit-lib/modswi64.o"
for f in cxxrt64 cxxheap64; do $CC $MODFLAGS -fno-exceptions -O2 -Wall -Wextra -I"$KIT/include" -isystem "$KIT/../../modkit/include" -c "$KIT/$f.c" -o "$KIT/../out/kit-lib/$f.o"; done
rm -f "$KIT/../out/kit-lib/libmodcxx64.a"
$A64/aarch64-none-elf-ar rcs "$KIT/../out/kit-lib/libmodcxx64.a" "$KIT/../out/kit-lib/cxxrt64.o" "$KIT/../out/kit-lib/cxxheap64.o"
# the C library of the kit (modkit/lib) for modules: libmodkit64.a.  Compiled with floating point allowed and MODLIB_HOST (its SWIs go through platform64.c); printf.c without MODLIB_HOST (it has the screen flavours of
# printf, puts and putchar then).  Left out: the C++ runtime (cxxrt64.c has the 64-bit one), the Shared C Library names, sockets and netdb, the runnable start code, kirq.S.
K="$KIT/../../modkit"
FLIB=$(echo "$MODFLAGS_FP" | sed 's/-fvisibility=hidden //')
SRCS=$(cd "$K/lib" && ls *.c | grep -v -E '^(cxxrt|cxxsp|cxxnew|cxxnewa|cxxdel|cxxdela|cxxeh|cxxhash|sclcompat|sockets|netdb|start)\.c$' | sed 's/\.c$//')
mkdir -p "$KIT/../out/kit-lib/o"
GCCINC=$($CC -print-file-name=include)
cat > "$KIT/../out/kit-lib/o/one.sh" <<EOS
#!/bin/bash
f=\$1
case \$f in fd_*) NOSH="-fwrapv" ;; *) NOSH="" ;; esac
case \$f in printf) HOSTDEF="" ;; *) HOSTDEF="-DMODLIB_HOST" ;; esac
$CC $FLIB -O2 -ffp-contract=off -fno-builtin -std=gnu11 \$NOSH -Wall -Wextra -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \$HOSTDEF -nostdinc -I$K/include -I$GCCINC -c $K/lib/\$f.c -o $KIT/../out/kit-lib/o/\$f.o 2> $KIT/../out/kit-lib/o/\$f.log || { echo "FAILED \$f"; cat $KIT/../out/kit-lib/o/\$f.log; exit 1; }
EOS
chmod +x "$KIT/../out/kit-lib/o/one.sh"
echo $SRCS | tr ' ' '\n' | xargs -P "$(nproc)" -n 1 "$KIT/../out/kit-lib/o/one.sh"
$CC $FLIB -O2 -fno-builtin -nostdinc -I"$K/include" -I"$GCCINC" -c "$K/lib/setjmp64.S" -o "$KIT/../out/kit-lib/o/setjmp64.o"
$CC $FLIB -O2 -nostdinc -I"$K/include" -I"$GCCINC" -c "$KIT/platform64.c" -o "$KIT/../out/kit-lib/o/platform64.o"
$CC $FLIB -c "$KIT/modsys64.S" -o "$KIT/../out/kit-lib/o/modsys64.o"
rm -f "$KIT/../out/kit-lib/libmodkit64.a"
$A64/aarch64-none-elf-ar rcs "$KIT/../out/kit-lib/libmodkit64.a" $(for f in $SRCS; do echo "$KIT/../out/kit-lib/o/$f.o"; done) "$KIT/../out/kit-lib/o/setjmp64.o" "$KIT/../out/kit-lib/o/platform64.o" "$KIT/../out/kit-lib/o/modsys64.o"
ls -l "$KIT/../out/kit-lib"
