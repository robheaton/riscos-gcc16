#!/bin/bash
# build.sh - build the RISC OS 64 tests: AIF64 absolutes (,ff8) and a module (,ffa) with the AArch64 tool chain (tools/build-a64-toolchain.sh), and the host builds that give the expected output.
# usage: ./build.sh [OUTDIR]     (default: ./out)    A64=<bin directory of aarch64-none-elf-gcc>  (default $HOME/a64/prefix/bin: the result of tools/build-a64-toolchain.sh)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-$HERE/out}
A64=${A64:-$HOME/a64/prefix/bin}
P=$A64/aarch64-none-elf
CFLAGS="-O2 -ffreestanding -nostdlib -fno-pic -fno-pie -mstrict-align -mcmodel=small -fno-stack-protector -mno-outline-atomics -fno-asynchronous-unwind-tables -fno-builtin-memcpy -fno-builtin-memset -fno-builtin-memmove -fno-builtin-memcmp -fno-builtin-strlen -fno-math-errno -ffp-contract=off -ffixed-x18 -Wall -Wextra -I$HERE/src"
mkdir -p "$OUT/o" "$OUT/expected" "$OUT/host"
cd "$OUT"
for s in aifhdr start swi; do $P-gcc $CFLAGS -c "$HERE/src/$s.S" -o o/$s.o; done
$P-gcc $CFLAGS -c "$HERE/src/rt.c" -o o/rt.o
TESTS="t01hello t02swi t03int t04fp t05file t07mod"
for t in $TESTS; do
  $P-gcc $CFLAGS -c "$HERE/src/$t.c" -o o/$t.o
  # the floating point test uses the FP registers; the integer tests are compiled without them
  $P-gcc -nostdlib -static -T "$HERE/src/aif64.ld" -Wl,--no-warn-rwx-segments -o o/$t.elf o/aifhdr.o o/start.o o/swi.o o/rt.o o/$t.o
  $P-objcopy -O binary o/$t.elf o/$t.bin
  python3 "$HERE/tools/mkaif64.py" o/$t.bin "$t,ff8" o/$t.elf
done
# the module
$P-gcc $CFLAGS -c "$HERE/src/module64.S" -o o/module64.o
$P-gcc -nostdlib -static -T "$HERE/src/module64.ld" -Wl,--no-warn-rwx-segments -o o/mod64.elf o/module64.o
$P-objcopy -O binary o/mod64.elf "Mod64,ffa"
# module test 0.4: Mod64D (writable data, a zero-initialised area, pointers that the module relocates itself, a service call entry, error blocks) and its test, and Mod64F (floating point state) and its test.
# Mod64D is built like the modules of a kit would be: -fPIC -fvisibility=hidden (no GOT, pointer tables in .data.rel.ro), -mgeneral-regs-only (no floating point or vector register at all), -ffixed-x18, linked
# at 4 by mod64d.ld with --emit-relocs, and tools/modreloc64.py makes the image (and lists the words that the module relocates itself); tools/check_mod64d.py checks it.
MCFLAGS="-O2 -ffreestanding -nostdlib -fPIC -fvisibility=hidden -mcmodel=small -mgeneral-regs-only -mstrict-align -ffixed-x18 -mno-outline-atomics -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-builtin -fno-tree-loop-distribute-patterns -Wall -Wextra -Wno-unused-parameter -I$HERE/src"
$P-gcc $MCFLAGS -c "$HERE/src/mod64d_hdr.S" -o o/mod64d_hdr.o
$P-gcc $MCFLAGS -c "$HERE/src/mod64d.c" -o o/mod64d.o
$P-gcc $MCFLAGS -c "$HERE/src/mod64d_swi.S" -o o/mod64d_swi.o			# swi_x for module code (EL1): keeps its pointer on the stack and checks that x19-x28 survive the svc
$P-gcc -nostdlib -static -T "$HERE/src/mod64d.ld" -Wl,--no-warn-rwx-segments -Wl,--emit-relocs -o o/mod64d.elf o/mod64d_hdr.o o/mod64d.o o/mod64d_swi.o
python3 -I "$HERE/tools/modreloc64.py" o/mod64d.elf "Mod64D,ffa"
python3 -I "$HERE/tools/check_mod64d.py" o/mod64d.elf "Mod64D,ffa"
# Mod64D must not touch the floating point state (that is the question Mod64F asks): no FP or vector register, and no x18, in its object code or its assembler sources
for f in o/mod64d.o o/mod64d_swi.o; do
  dis=$($P-objdump -d $f | grep -E '^\s+[0-9a-f]+:\s+[0-9a-f]{8}\s' | cut -f3-)
  if echo "$dis" | grep -E '\b[xw]18\b' | grep -q .; then echo "$f: x18 is used" >&2; exit 1; fi
  # a branch target such as d24 is not a register: the branches and ADR/ADRP are left out of the check for the floating point registers
  if echo "$dis" | grep -v -E '^(b|bl|b\.[a-z]+|cbz|cbnz|tbz|tbnz|adr|adrp)\s' | grep -E '\b[qvdsh]([0-9]|[12][0-9]|3[01])\b' | grep -q .; then echo "$f: floating point or vector registers are used" >&2; exit 1; fi
done
if grep -v '^\s*/\*' "$HERE/src/mod64d_hdr.S" | grep -E '\b([qvdsh]([0-9]|[12][0-9]|3[01])|[xw]18)\b' | grep -v '^\s*\.' | grep -q .; then echo "mod64d_hdr.S: floating point, vector or x18 registers are used" >&2; exit 1; fi
$P-gcc $CFLAGS -c "$HERE/src/t08data.c" -o o/t08data.o
$P-gcc -nostdlib -static -T "$HERE/src/aif64.ld" -Wl,--no-warn-rwx-segments -o o/t08data.elf o/aifhdr.o o/start.o o/swi.o o/rt.o o/t08data.o
$P-objcopy -O binary o/t08data.elf o/t08data.bin
python3 "$HERE/tools/mkaif64.py" o/t08data.bin "t08data,ff8" o/t08data.elf
# Mod64F: assembler only, linked at 4 (mod64sf.ld: it has no address that needs relocating); the test program t09fp uses the vector registers on purpose
$P-gcc $CFLAGS -c "$HERE/src/mod64f.S" -o o/mod64f.o
$P-gcc -nostdlib -static -T "$HERE/src/mod64sf.ld" -Wl,--no-warn-rwx-segments -o o/mod64f.elf o/mod64f.o
$P-objcopy -O binary o/mod64f.elf "Mod64F,ffa"
python3 -I "$HERE/tools/check_mod64.py" --name Mod64F --swis "Probe,Clobber" --chunk 0x59A80 --nocmds "Mod64F,ffa"
# Mod64S: assembler only, linked at 4 (mod64sf.ld), a plain service entry with no table (it reaches its state with ADR)
$P-gcc $CFLAGS -c "$HERE/src/mod64s.S" -o o/mod64s.o
$P-gcc -nostdlib -static -T "$HERE/src/mod64sf.ld" -Wl,--no-warn-rwx-segments -o o/mod64s.elf o/mod64s.o
$P-objcopy -O binary o/mod64s.elf "Mod64S,ffa"
python3 -I "$HERE/tools/check_mod64.py" --name Mod64S --swis "Setup,Svc,Ring" --chunk 0x59AC0 --nocmds "Mod64S,ffa"
$P-gcc $CFLAGS -c "$HERE/src/fpregs.S" -o o/fpregs.o
$P-gcc $CFLAGS -c "$HERE/src/t09fp.c" -o o/t09fp.o
$P-gcc -nostdlib -static -T "$HERE/src/aif64.ld" -Wl,--no-warn-rwx-segments -o o/t09fp.elf o/aifhdr.o o/start.o o/swi.o o/rt.o o/fpregs.o o/t09fp.o
$P-objcopy -O binary o/t09fp.elf o/t09fp.bin
python3 "$HERE/tools/mkaif64.py" o/t09fp.bin "t09fp,ff8" o/t09fp.elf
# the host builds: the expected output of the tests that do not depend on the machine
for t in t03int t04fp; do
  gcc -O1 -fno-math-errno -ffp-contract=off -fno-builtin-memcpy -fno-builtin-memset -fno-builtin-memmove -fno-builtin-memcmp -fno-builtin-strlen -w -I"$HERE/src" -o host/$t "$HERE/src/$t.c" "$HERE/host/rt_host.c" 2>&1 | head -5
  ./host/$t > expected/$t.txt || true
done
ls -l *,ff8 *,ffa
