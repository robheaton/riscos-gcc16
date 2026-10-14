#!/bin/sh
# asasm-fix.sh OBJECT... - make the ELF objects that GCCSDK's asasm writes linkable into a module (and a program) of this tool chain.  Installed as  <tool chain>/bin/arm-riscos-gnueabihf-asasm-fix.
#
# Two things stop an object of asasm (ObjAsm's assembler) from linking:
#   * asasm writes EABI version 0 into the ELF header, and ld refuses it next to the EABI version 5 of GCC 16's objects:
#       ld: error: source object callback.o has EABI version 0, but target Module has EABI version 5
#   * asasm names the sections after the AREA (C$$Code, C$$Data ...), and the linker script of a module (module.ld) places only .text*, .rodata*, .data* and .bss*: modreloc refuses the module and names them.
# This sets the EABI version to 5 (the flags word at offset 36 of the header keeps its other bits) and gives each section that is neither standard nor a debugging section the name of its kind: .text
# (executable), .data (writable), .bss (writable, no contents), .rodata (the others).  The objects are changed in place.  Needs objcopy and readelf of the tool chain, od, dd and awk.
set -eu
[ $# -ge 1 ] || { echo "usage: $0 OBJECT.o ..." >&2; exit 2; }
D=$(cd "$(dirname "$0")" && pwd)
tool() { if [ -x "$D/arm-riscos-gnueabihf-$1" ]; then echo "$D/arm-riscos-gnueabihf-$1"; else echo "arm-riscos-gnueabihf-$1"; fi; }
OBJCOPY=$(tool objcopy); READELF=$(tool readelf)
for f in "$@"; do
  [ -f "$f" ] || { echo "asasm-fix: $f: no such file" >&2; exit 1; }
  # a little endian 32 bit ELF file for ARM
  [ "$(od -An -tx1 -N6 "$f" | tr -d ' \n')" = "7f454c460101" ] && [ "$(od -An -tx1 -j18 -N2 "$f" | tr -d ' \n')" = "2800" ] || { echo "asasm-fix: $f is not a 32 bit little endian ARM ELF object" >&2; exit 1; }
  # the EABI version: the top byte of e_flags (offset 39)
  printf '\005' | dd of="$f" bs=1 seek=39 count=1 conv=notrunc 2>/dev/null
  # the sections: [ N] name type addr off size es flags ...
  ARGS=$("$READELF" -S -W "$f" | sed 's/^ *\[ */[/' | awk '
    /^\[[0-9]+\]/ {
      name = $2; type = $3; flg = ($8 ~ /^[A-Za-z]+$/) ? $8 : ""
      if (index(flg, "A") == 0) next                                   # not loaded: symbol tables, relocations, notes ...
      if (name ~ /^\.(text|rodata|data|bss|init_array|fini_array|ctors|dtors)/) next
      if (name ~ /^\.(ARM|note|comment|debug)/) next
      if (type == "NOBITS") kind = ".bss"; else if (index(flg, "X")) kind = ".text"; else if (index(flg, "W")) kind = ".data"; else kind = ".rodata"
      printf "--rename-section %s=%s ", name, kind
    }')
  [ -z "$ARGS" ] || "$OBJCOPY" $ARGS "$f"
done
