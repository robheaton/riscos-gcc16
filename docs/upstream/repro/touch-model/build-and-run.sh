#!/bin/bash
# usage: build-and-run.sh PATCHED-unix.h [mutate]     (the patched incl-local/internal/unix.h: patches/unixlib-touch-stack-buffers.patch applied)
set -u
HERE=$(cd "$(dirname "$0")" && pwd); H=$1; MODE=${2:-}
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
extract() { # header out : the EABI variant of the function, with the two target-specific lines replaced
  awk '/^#if defined \(__ARM_EABI__\)$/ && !d {p=1; next} p && /^#else$/ {p=0; d=1} p {print}' "$1" \
   | sed -e 's|__asm__ ("mov	%0, sp" : "=r" (__sp));|__sp = model_sp;|' -e 's|(void) \*__p;|record ((unsigned int) (uintptr_t) __p);|' > "$2"; }
run() { gcc -O1 -g -w -I"$W" "$HERE/touch_model.c" -o "$W/m" && MODEL_LABEL="$2" CASES= "$W/m" ${CASES:-}; }
extract "$H" "$W/touch_fn.inc"
grep -q model_sp "$W/touch_fn.inc" && grep -q record "$W/touch_fn.inc" || { echo "could not cut out the function"; exit 99; }
if [ "$MODE" != mutate ]; then run x "touch model"; exit $?; fi
n=0; missed=0
mut() { n=$((n+1)); cp "$W/touch_fn.inc" "$W/orig.inc"; sed -i "$2" "$W/touch_fn.inc"
  if cmp -s "$W/orig.inc" "$W/touch_fn.inc"; then echo "  mutation $n NOT APPLIED: $1"; missed=$((missed+1)); cp "$W/orig.inc" "$W/touch_fn.inc"; return; fi
  r=$(CASES=200000 run x "mutant $n" 2>&1 | tail -1); cp "$W/orig.inc" "$W/touch_fn.inc"
  case "$r" in *" 0 failed check(s)") echo "  MISSED  mutation $n: $1"; missed=$((missed+1));; *) echo "  caught  mutation $n: $1";; esac; }
mut "limit of 1 MB becomes 1 MB + 4 KB (first test)"      's/__lo - __sp >= 0x100000u/__lo - __sp >= 0x101000u/'
mut "limit of 1 MB becomes 1 MB - 4 KB (first test)"      's/__lo - __sp >= 0x100000u/__lo - __sp >= 0xff000u/'
mut ">= becomes > in the first test"                      's/__lo - __sp >= 0x100000u/__lo - __sp > 0x100000u/'
mut "the clamp of the end is dropped"                     's/if (__hi < __lo || __hi - __sp > 0x100000u)/if (__hi < __lo)/'
mut "the overflow test of the end is dropped"             's/__hi < __lo || __hi - __sp > 0x100000u/__hi - __sp > 0x100000u/'
mut "the start is not rounded down to a page"             's/(__lo \& ~0xfffu)/__lo/'
mut "the loop stops one page early"                       's/(unsigned int) __p < __hi/(unsigned int) __p + 4096 < __hi/'
mut "the loop steps 8 KB"                                 's/__p += 4096/__p += 8192/'
mut "the loop runs one page too far"                      's/(unsigned int) __p < __hi/(unsigned int) __p <= __hi/'
mut "the function does nothing"                           's/^  for (volatile/  if (0) for (volatile/'
echo "mutation checks: $n mutations, $missed not caught"; [ "$missed" = 0 ]
