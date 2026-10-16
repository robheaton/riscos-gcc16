#!/bin/bash
# build-libtestmod.sh [OUTDIR]: the module LibTest64,ffa: the C library of the kit (modkit/lib, compiled for AArch64 as for libtest64, with the AArch64 branches of heap.c, exit.c and setjmp64.S) and libtest.c in a module.
# The library is compiled with floating point allowed (the maths needs it; Pyromaniac keeps the caller's floating point state across a module's SWI).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
R64=$HERE/..
K=${MODKIT:-$R64/../modkit}
KIT=$R64/kit
OUT=${1:-$R64/out/libtestmod}
A64=${A64:-/var/tmp/a64/prefix/bin}
P=$A64/aarch64-none-elf
GCCINC=$($P-gcc -print-file-name=include)
mkdir -p "$OUT/o"
python3 "$K/tests/libtest/mkrename.py" "$OUT" "$K/include" >/dev/null
FL="-std=gnu11 -O2 -ffp-contract=off -ffreestanding -fno-pic -fno-pie -mcmodel=small -ffixed-x18 -mstrict-align -mno-outline-atomics -fno-builtin -fno-tree-loop-distribute-patterns -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -D__riscos64__ -DMODULE"
LIBSRC=$(cd "$K/lib" && ls *.c | grep -v -E '^(cxxrt|cxxsp|sclcompat|sockets|netdb|start)\.c$' | sed 's/\.c$//' | tr "\n" " ")
cat > "$OUT/o/compile-one.sh" <<EOS
#!/bin/bash
f=\$1
case \$f in fd_*) NOSH="-fwrapv" ;; *) NOSH="" ;; esac
case \$f in printf) HOSTDEF="" ;; *) HOSTDEF="-DMODLIB_HOST" ;; esac     # printf.c has the screen flavours of printf, puts and putchar only without MODLIB_HOST
$P-gcc $FL \$NOSH -Wall -Wextra -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \$HOSTDEF -Derrno=mk_errno -nostdinc -I$K/include -I$GCCINC -include $OUT/mk_rename.h -c $K/lib/\$f.c -o $OUT/o/\$f.o 2> $OUT/o/\$f.log || { echo "FAILED \$f"; cat $OUT/o/\$f.log; exit 1; }
cat $OUT/o/\$f.log
EOS
chmod +x "$OUT/o/compile-one.sh"
echo $LIBSRC | tr ' ' '\n' | xargs -P "$(nproc)" -n 1 "$OUT/o/compile-one.sh"
$P-gcc $FL -c "$K/lib/setjmp64.S" -o "$OUT/o/setjmp64.o"
$P-gcc $FL -Wall -Wextra -nostdinc -I"$K/include" -I"$GCCINC" -DSWIS=1 -DREALVARS=1 -DREALRAISE=1 -c "$HERE/a64hooks.c" -o "$OUT/o/a64hooks.o"
$P-gcc $FL -c "$HERE/a64swi.S" -o "$OUT/o/a64swi.o"
$P-gcc $FL -c "$HERE/libtestmod.S" -o "$OUT/o/libtestmod_s.o"
$P-gcc $FL -Wall -Wno-unused-function -DT_HOSTLIB -DMK_NO_VARS -DT_FSDIR="\"${FSDIR:-MK}\"" -DSCALE="${SCALE:-1}" -Dmain=libtest_main -nostdinc -I"$OUT" -I"$K/include" -I"$GCCINC" -I"$K/tests/libtest" \
    -c "$K/tests/libtest/libtest.c" -o "$OUT/o/libtest.o" 2> "$OUT/o/libtest.log" || { echo "FAILED libtest.c"; head -40 "$OUT/o/libtest.log"; exit 1; }
# the module: the header from the CMHG file, the module's own code, the kit's runtime
python3 "$KIT/cmunge64.py" -d "$OUT/libtestmodh.h" -o "$OUT/o/libtestmod_hdr.o" "$HERE/libtestmod.cmhg"
$P-gcc $FL -Wall -Wextra -Wno-unused-parameter -I"$KIT/include" -I"$OUT" -c "$HERE/libtestmod.c" -o "$OUT/o/libtestmod.o"
"$KIT/build-kit.sh" >/dev/null
LIBOBJ=$(for f in $LIBSRC; do echo "$OUT/o/$f.o"; done)
$P-gcc -nostdlib -static -T "$KIT/module64.ld" -Wl,--no-warn-rwx-segments -Wl,--emit-relocs -Wl,-Map="$OUT/o/mod.map" \
    "$OUT/o/libtestmod_hdr.o" "$OUT/o/libtestmod.o" "$OUT/o/libtestmod_s.o" "$OUT/o/libtest.o" "$OUT/o/a64hooks.o" "$OUT/o/a64swi.o" "$OUT/o/setjmp64.o" $LIBOBJ \
    "$KIT/../out/kit-lib/modrt64.o" "$KIT/../out/kit-lib/modswi64.o" -lgcc -o "$OUT/o/mod.elf"
python3 "$R64/tools/modreloc64.py" "$OUT/o/mod.elf" "$OUT/LibTest64,ffa"
$P-size "$OUT/o/mod.elf"
