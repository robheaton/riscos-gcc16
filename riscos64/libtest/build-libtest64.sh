#!/bin/bash
# build-libtest64.sh - the library test of modkit (modkit/tests/libtest/libtest.c, T_HOSTLIB) for RISC OS 64: the library sources (modkit/lib, all but the ones the host build leaves out) compiled for AArch64 under
# the names mk_*, the test program, the hooks of a64hooks.c, the start-up of the SDK (sdk/) and picolibc (for the names that the host build takes from the host: malloc, printf, puts, exit ...), linked as an AIF64
# program  libtest64,ff8.  The sections that need no SWI are compared with the output of the host's C library (glibc: the oracle) by compare.sh.
# usage: build-libtest64.sh [OUTDIR]    (default: ../out/libtest64)     SWIS=1: pass the SWIs of the files and the clock on to the kernel (step 2)      SCALE=n: the amount of work (default 1)
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
R64=$HERE/..
K=${MODKIT:-$R64/../modkit}
SDK=$R64/sdk
OUT=${1:-$R64/out/libtest64}
A64=${A64:-/var/tmp/a64/prefix/bin}
PICO=${PICOLIBC:-/var/tmp/a64/picolibc}
SCALE=${SCALE:-1}
P=$A64/aarch64-none-elf
GCCINC=$($P-gcc -print-file-name=include)
mkdir -p "$OUT/o"
python3 "$K/tests/libtest/mkrename.py" "$OUT" "$K/include" >/dev/null
FL="-std=gnu11 -O2 -ffp-contract=off -ffreestanding -ffixed-x18 -mstrict-align -fno-builtin -fno-tree-loop-distribute-patterns -fno-stack-protector -fno-asynchronous-unwind-tables"
LIBSRC=$(cd "$K/lib" && ls *.c | grep -v -E '^(heap|exit|assert|start|cxxrt|cxxsp|sclcompat|sockets|netdb)\.c$' | sed 's/\.c$//' | tr "\n" " ")
cat > "$OUT/o/compile-one.sh" <<EOS
#!/bin/bash
f=\$1
case \$f in fd_*) NOSH="-fwrapv" ;; *) NOSH="" ;; esac
$P-gcc $FL \$NOSH -Wall -Wextra -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -DMODLIB_HOST -Derrno=mk_errno -nostdinc -I$K/include -I$GCCINC -include $OUT/mk_rename.h -c $K/lib/\$f.c -o $OUT/o/\$f.o 2> $OUT/o/\$f.log || { echo "FAILED \$f"; cat $OUT/o/\$f.log; exit 1; }
cat $OUT/o/\$f.log
EOS
chmod +x "$OUT/o/compile-one.sh"
echo $LIBSRC | tr ' ' '\n' | xargs -P "$(nproc)" -n 1 "$OUT/o/compile-one.sh"
EXTRA=
[ -n "${SWIS:-}" ] && EXTRA="-DSWIS=1"
$P-gcc $FL -isystem "$PICO/include" -Wall -Wextra $EXTRA -c "$HERE/a64hooks.c" -o "$OUT/o/a64hooks.o"
$P-gcc $FL -isystem "$PICO/include" -Wall -Wextra -c "$HERE/a64main.c" -o "$OUT/o/a64main.o"
if [ -n "${SWIS:-}" ]; then $P-gcc $FL -c "$HERE/a64swi.S" -o "$OUT/o/a64swi.o"; fi
# the test: against the headers of the kit (the same headers as the library), the renamed functions
$P-gcc $FL -Wall -Wno-unused-function -DT_HOSTLIB -DMK_NO_VARS -DT_FSDIR="\"${FSDIR:-MK}\"" -DSCALE="$SCALE" -Dmain=libtest_main -nostdinc -I"$OUT" -I"$K/include" -I"$GCCINC" -I"$K/tests/libtest" \
    -c "$K/tests/libtest/libtest.c" -o "$OUT/o/libtest.o" 2> "$OUT/o/libtest.log" || { echo "FAILED libtest.c"; head -60 "$OUT/o/libtest.log"; exit 1; }
# the AIF64 start-up objects of the SDK (built by sdk/build-sdk.sh)
LIBOBJ=$(for f in $LIBSRC; do echo "$OUT/o/$f.o"; done)
EXTRAOBJ=
[ -n "${SWIS:-}" ] && EXTRAOBJ="$OUT/o/a64swi.o"
$P-gcc $FL -nostdlib -static -T "$SDK/aif64.ld" -Wl,--no-warn-rwx-segments -Wl,-Map="$OUT/o/libtest64.map" \
    "$SDK/lib/aifhdr.o" "$SDK/lib/crt0.o" "$SDK/lib/riscos_start.o" "$SDK/lib/riscos_syscalls.o" "$SDK/lib/swi.o" \
    "$OUT/o/a64main.o" "$OUT/o/libtest.o" "$OUT/o/a64hooks.o" $EXTRAOBJ $LIBOBJ \
    -L"$PICO/lib" -Wl,--start-group -lc -lm -lgcc -Wl,--end-group -o "$OUT/o/libtest64.elf"
$P-objcopy -O binary "$OUT/o/libtest64.elf" "$OUT/o/libtest64.bin"
python3 -I "$SDK/bin/mkaif64.py" "$OUT/o/libtest64.bin" "$OUT/libtest64,ff8" "$OUT/o/libtest64.elf"
$P-size "$OUT/o/libtest64.elf"
ls -l "$OUT/libtest64,ff8"
