#!/bin/bash
# mklibgcc.sh - makes libgcc-mod.a, the libgcc that a module may link: a copy of the libgcc.a of the tool chain without the members that a module must not run.
#
# The libgcc.a of the tool chain is built for the default target of the compiler (ARMv7-A, VFP, hard float) and has one multilib only.  Its assembler members (the soft float routines of ieee754-df.S and
# ieee754-sf.S, the integer division of lib1funcs.S) are plain ARM code and run on any ARMv6; but the members that GCC compiles from C (libgcc2.c, fixed-point, ...) are compiled for ARMv7 with the VFP:
# __fixdfdi (a double to a long long) executes  vmov d16, r0, r1 ,  __popcountsi2 executes  movw / movt  and  __ctzdi2  rbit .  In a module that is wrong twice: the code does not exist on an ARMv6 CPU
# (the Raspberry Pi 1), and the VFP registers belong to the application that called the module (the module runs in SVC mode, under whatever task is running; RISC OS does not save the VFP state for it).
# The linker does not object (it links the member without a word), so the module would have been built and would have crashed or damaged the VFP state of a program later.
#
# (The members that reach their data through a global offset table are dropped as well: see got_members below.)
# This script keeps a member only if its disassembly has no VFP / NEON instruction (every mnemonic that starts with v), none of the ARMv6T2 / ARMv7 integer instructions (movw movt rbit ubfx sbfx bfi bfc mls
# udiv sdiv dmb dsb isb), and it is not a Thumb-only helper; libmodkit.a then names libgcc-mod.a instead of -lgcc, so that what is missing is an undefined reference at link time.
# The kit has its own versions of the dropped members that C code of a module can reach (lib/gccrt.c: double and float to 64-bit integers, popcount, parity, ctz, ffs, powi).
# The fixed-point members (_fract*, _satfract*, _ssaddHQ ... : the Embedded C types _Fract and _Accum, which GCC does not offer on ARM) are left out too, and the debug information is stripped: the
# libgcc.a of the tool chain is 9.8 MB, libgcc-mod.a about 0.1 MB.
#
# The same filter makes libstdcxx-mod.a from the libstdc++.a of the tool chain (install-modkit.sh).  Every member that is kept is stripped of its debug information and of its .ARM.attributes (the members
# are compiled for ARMv7 with the VFP, and say so, although the code that is left in them is plain ARMv6: the attributes would say it of the module), and the finished archive is disassembled once more:
# a script that cannot run objdump, or one that lets an instruction through, stops with an error instead of making an archive that is wrong.
#
# usage: mklibgcc.sh AR OBJDUMP LIBGCC.A OUT.A [REPORT]
set -eu
export LC_ALL=C
abs () { case $1 in */*) readlink -f "$1" ;; *) command -v "$1" ;; esac; }          # a program name (found in the PATH) or a path
AR=$(abs "${1:?usage: mklibgcc.sh AR OBJDUMP LIBGCC.A OUT.A [REPORT]}")
OBJDUMP=$(abs "${2:?}")
IN=$(readlink -f "${3:?}")
OUT=$(readlink -f "$(dirname "${4:?}")")/$(basename "$4")
REPORT=${5:-}
[ -z "$REPORT" ] || REPORT=$(readlink -f "$(dirname "$REPORT")")/$(basename "$REPORT")
[ -x "$AR" ] && [ -x "$OBJDUMP" ] && [ -f "$IN" ] || { echo "mklibgcc: AR, OBJDUMP or the archive is not there" >&2; exit 1; }
OBJCOPY=${OBJDUMP%objdump}objcopy
READELF=${OBJDUMP%objdump}readelf
[ -x "$OBJCOPY" ] && [ -x "$READELF" ] || { echo "mklibgcc: no objcopy and readelf next to objdump" >&2; exit 1; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
mkdir "$W/m"
(cd "$W/m" && "$AR" x "$IN")
(cd "$W/m" && ls *.o > ../all.txt)

# the instructions that a module must not run: the VFP and NEON (every mnemonic that starts with v), what exists from ARMv6T2 or ARMv7 on, the ARMv8 ones, a word that objdump could not decode, and the
# coprocessor 10 and 11 forms of the VFP.  Reads the output of  objdump -d  of several members (a  "file.o:  file format"  line in front of each), prints  "member first-bad-mnemonic"  lines.
scan () {
  awk -F'\t' '
    /^[^ \t].*:[ \t]+file format/ { f = $0; sub(/:[ \t]+file format.*/, "", f); next }
    NF >= 3 && f != "" {
      split($3, a, " "); m = a[1]
      if (m ~ /^v[a-z]/ || m ~ /^(movw|movt|rbit|ubfx|sbfx|bfi|bfc|mls|udiv|sdiv|dmb|dsb|isb)(eq|ne|cs|hs|cc|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le)?$/ ||
          m ~ /^(ldrht|strht|ldrsht|ldrsbt|pli|pldw|dbg|smc|hvc|eret|lda|stl|sevl|crc32)/ || m == ".inst" || m == "(bad)" ||
          (m ~ /^(mrc|mcr|mrrc|mcrr|ldc|stc|cdp)/ && $4 ~ /^p1[01],/))
        { if (!(f in bad)) bad[f] = m }
    }
    END { for (f in bad) print f, bad[f] }'
}
# objdump of every member of DIR in one run; it must succeed and say "file format" once per member (a failure must not look like "nothing found")
disassemble () {   # DIR LIST OUTFILE
  (cd "$1" && xargs "$OBJDUMP" -d < "$2") > "$3" 2> "$3.err" || { echo "mklibgcc: objdump failed:" >&2; head -5 "$3.err" >&2; exit 1; }
  n=$(grep -c ':[[:space:]]\+file format' "$3" || true)
  [ "$n" = "$(wc -l < "$2")" ] || { echo "mklibgcc: objdump described $n of $(wc -l < "$2") members" >&2; exit 1; }
}
# the members that reach their data through a global offset table: code that was compiled with -fPIC for this tool chain (the shared libraries of UnixLib) does it through a table at 0x8000 of the
# program (R_ARM_NONE __GOTT_INDEX__, R_ARM_GOT32 / GOT_BREL): a module has no such table, so the code would read the memory at 0x8038.  Prints "member got" for each.
got_members () {   # DIR LIST
  local f
  while read -r f; do
    "$READELF" -rW "$1/$f" > "$W/re.txt" 2> "$W/re.err" || { echo "mklibgcc: readelf failed on $f:" >&2; head -3 "$W/re.err" >&2; exit 1; }
    if grep -q -E 'R_ARM_GOT|R_ARM_TARGET2|__GOTT_INDEX__' "$W/re.txt"; then echo "$f got"; fi
  done < "$2"
}
disassemble "$W/m" "$W/all.txt" "$W/dis.txt"
{ scan < "$W/dis.txt"; got_members "$W/m" "$W/all.txt"; } | sort > "$W/bad.txt"
# the Thumb-only helpers (they are only called from Thumb code, which a module is not) and the members that are Linux system calls
ls "$W/m" | grep -E '^(_call_via_|_interwork_call_via_|_thumb1_case_|linux-|sfp-exceptions)' | awk '{print $0, "thumb-or-linux"}' >> "$W/bad.txt" || true
sort -u "$W/bad.txt" -o "$W/bad.txt"
awk '{print $1}' "$W/bad.txt" | sort -u > "$W/drop.txt"
# the fixed-point members (libgcc.a only): _fract* and _satfract* (conversions), and the arithmetic ones, whose names end in the mode of the type (QQ HQ SQ DQ TQ HA SA DA TA, also the unsigned ones UQQ ... UTA)
grep -E '^_(sat)?fract|(QQ|HQ|SQ|DQ|TQ|HA|SA|DA|TA)\.o$' "$W/all.txt" | grep -E '^_' > "$W/fixed.txt" || true
sort -u "$W/drop.txt" "$W/fixed.txt" -o "$W/drop.txt"
sort "$W/all.txt" | comm -23 - "$W/drop.txt" > "$W/keep.txt"
[ -s "$W/keep.txt" ] || { echo "mklibgcc: no member is left" >&2; exit 1; }
rm -f "$OUT"
(cd "$W/m" && xargs -n1 "$OBJCOPY" -g -R .ARM.attributes < ../keep.txt && xargs "$AR" rcs "$OUT" < ../keep.txt)
# the check of the result: disassemble the archive again, whatever the first pass said
mkdir "$W/c" && (cd "$W/c" && "$AR" x "$OUT" && ls *.o > ../chk.txt)
disassemble "$W/c" "$W/chk.txt" "$W/dis2.txt"
{ scan < "$W/dis2.txt"; got_members "$W/c" "$W/chk.txt"; } > "$W/bad2.txt"
if [ -s "$W/bad2.txt" ]; then echo "mklibgcc: the archive still has instructions that a module must not run:" >&2; head -5 "$W/bad2.txt" >&2; rm -f "$OUT"; exit 1; fi
echo "$(basename "$OUT"): $(wc -l < "$W/keep.txt") of $(wc -l < "$W/all.txt") members of $(basename "$IN") kept ($(wc -l < "$W/drop.txt") dropped: $(awk '{print $1}' "$W/bad.txt" | sort -u | wc -l) for VFP, ARMv7, Thumb, Linux or global offset table code, the others fixed-point)"
if [ -n "$REPORT" ]; then
  { echo "# members of $(basename "$IN") that $(basename "$OUT") leaves out: the first instruction that a module must not run, or thumb-or-linux; fixed-point members (_fract*, _satfract*, ...) are not listed"; cat "$W/bad.txt"; } > "$REPORT"
fi
