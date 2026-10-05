#!/bin/bash
# report 19: is the result of the asm in regvar1.c's f () copied out of r0 BEFORE the first call (bl strlen)?   usage: check-regvar-asm.sh COMPILER REGVAR1.C [-DFIXED]
# prints "captured" (the code is right) or "NOT captured" (the bug: the result is read from r0 after the call).  Works for the EABI compilers and for GCC 4.7.4 (it only reads the -S output).
cc=$1; src=$2; shift 2
"$cc" -O2 "$@" -S -o - "$src" | awk '
  /^f:/ {p = 1; next}
  p && /\.size[[:space:]]+f,/ {exit}
  p && /mov[[:space:]]+r0, #7/ {asm = 1; next}
  asm && /bl[[:space:]]+strlen/ {print (cap ? "captured" : "NOT captured"); done = 1; exit}
  asm && /^[[:space:]]+mov[a-z]*[[:space:]]+r[0-9]+, r0$/ {cap = 1}
  END {if (!done) print "no call found"}'
