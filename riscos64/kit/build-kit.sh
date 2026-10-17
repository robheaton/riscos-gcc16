#!/bin/bash
# build-kit.sh: builds the runtime objects of the module kit (kit/lib/modrt64.o, modswi64.o) with the AArch64 tool chain.  Run by sdk/build-sdk.sh too.
set -e
KIT=$(cd "$(dirname "$0")" && pwd)
A64=${A64:-/var/tmp/a64/prefix/bin}
CC=$A64/aarch64-none-elf-gcc
AR=$A64/aarch64-none-elf-ar
. "$KIT/modflags.sh"
# KITCC=.../aarch64-riscos-gcc (target/build-target.sh): build with the real target, which knows the options (-mmodule, -mmodule-fp) itself; KITAR = its ar; KITOUT = where the objects and libraries go
OUT=${KITOUT:-$KIT/../out/kit-lib}
if [ -n "${KITCC:-}" ]; then CC=$KITCC; AR=${KITAR:?KITAR with KITCC}; MODFLAGS=-mmodule; MODFLAGS_FP="-mmodule-fp -fvisibility=default"; fi
mkdir -p "$OUT"
$CC $MODFLAGS -O2 -Wall -Wextra -I"$KIT/include" -isystem "$KIT/../../modkit/include" -c "$KIT/modrt64.c" -o "$OUT/modrt64.o"
$CC $MODFLAGS -c "$KIT/modswi64.S" -o "$OUT/modswi64.o"
for f in cxxrt64 cxxheap64; do $CC $MODFLAGS -fno-exceptions -O2 -Wall -Wextra -I"$KIT/include" -isystem "$KIT/../../modkit/include" -c "$KIT/$f.c" -o "$OUT/$f.o"; done
rm -f "$OUT/libmodcxx64.a"
$AR rcs "$OUT/libmodcxx64.a" "$OUT/cxxrt64.o" "$OUT/cxxheap64.o"
# the C library of the kit (modkit/lib) for modules: libmodkit64.a.  Compiled with floating point allowed and MODLIB_HOST (its SWIs go through platform64.c); printf.c without MODLIB_HOST (it has the screen flavours of
# printf, puts and putchar then).  Left out: the C++ runtime (cxxrt64.c has the 64-bit one), the Shared C Library names, sockets and netdb, the runnable start code, kirq.S.
K="$KIT/../../modkit"
FLIB=$(echo "$MODFLAGS_FP" | sed 's/-fvisibility=hidden //')
SRCS=$(cd "$K/lib" && ls *.c | grep -v -E '^(cxxrt|cxxsp|cxxnew|cxxnewa|cxxdel|cxxdela|cxxeh|cxxhash|sclcompat|sockets|netdb|start)\.c$' | sed 's/\.c$//')
mkdir -p "$OUT/o"
GCCINC=$($CC -print-file-name=include)
cat > "$OUT/o/one.sh" <<EOS
#!/bin/bash
f=\$1
case \$f in fd_*) NOSH="-fwrapv" ;; *) NOSH="" ;; esac
case \$f in printf) HOSTDEF="" ;; *) HOSTDEF="-DMODLIB_HOST" ;; esac
$CC $FLIB -O2 -ffp-contract=off -fno-builtin -std=gnu11 \$NOSH -Wall -Wextra -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \$HOSTDEF -nostdinc -I$K/include -I$GCCINC -c $K/lib/\$f.c -o $OUT/o/\$f.o 2> $OUT/o/\$f.log || { echo "FAILED \$f"; cat $OUT/o/\$f.log; exit 1; }
EOS
chmod +x "$OUT/o/one.sh"
echo $SRCS | tr ' ' '\n' | xargs -P "$(nproc)" -n 1 "$OUT/o/one.sh"
$CC $FLIB -O2 -fno-builtin -nostdinc -I"$K/include" -I"$GCCINC" -c "$K/lib/setjmp64.S" -o "$OUT/o/setjmp64.o"
$CC $FLIB -O2 -nostdinc -I"$K/include" -I"$GCCINC" -c "$KIT/platform64.c" -o "$OUT/o/platform64.o"
$CC $FLIB -c "$KIT/modsys64.S" -o "$OUT/o/modsys64.o"
rm -f "$OUT/libmodkit64.a"
$AR rcs "$OUT/libmodkit64.a" $(for f in $SRCS; do echo "$OUT/o/$f.o"; done) "$OUT/o/setjmp64.o" "$OUT/o/platform64.o" "$OUT/o/modsys64.o"
ls -l "$OUT"
