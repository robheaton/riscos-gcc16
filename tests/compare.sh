#!/bin/bash
# Compare assembly from the installed GCC 10.2.0 and the new GCC 16.2 for the RISC OS-specific paths.
OLD=$HOME/gccsdk/env/bin/arm-riscos-gnueabihf-gcc
NEW=${NEW:-$HOME/gccsdk-next/env-f/bin/arm-riscos-gnueabihf-gcc}
cd $HOME/gccsdk-next/tests; mkdir -p out
for f in pic nonleaf alloca fp; do
  for fl in "-O2" "-O2 -fPIC" "-O2 -fpic"; do
    tag=$(echo "$fl" | tr -d ' -')
    $OLD $fl -S -o out/$f.$tag.old.s $f.c 2>out/$f.$tag.old.err
    $NEW $fl -S -o out/$f.$tag.new.s $f.c 2>out/$f.$tag.new.err
    printf "%-9s %-12s old:%s new:%s  GOTT-seq(old/new): %s/%s  fp-usage(old/new): %s/%s\n" $f "$fl" \
      "$([ -s out/$f.$tag.old.s ] && echo ok || echo FAIL)" "$([ -s out/$f.$tag.new.s ] && echo ok || echo FAIL)" \
      "$(grep -c 'GOTT_INDEX' out/$f.$tag.old.s)" "$(grep -c 'GOTT_INDEX' out/$f.$tag.new.s)" \
      "$(grep -c -E '\bfp\b' out/$f.$tag.old.s)" "$(grep -c -E '\bfp\b' out/$f.$tag.new.s)"
  done
done
