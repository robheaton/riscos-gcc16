#!/bin/bash
# The seventeen UnixLib patches of the bundle, applied in the order of my port's recipe to a copy of pristine trunk r7800, give - comments stripped, white space ignored - the same code as the
# libunixlib tree that my port's release build (16.2.0-11) was compiled from.   usage: check-fidelity.sh BUNDLE-DIR GCCSDK-DIR RELEASE-libunixlib-DIR      (prints "ok ..." / "FAIL ...")
B=$(cd "$1" && pwd); G=$2; R=$3; U=gcc4/recipe/files/gcc/libunixlib
ORDER="unixlib-gcc14-implicit-declarations unixlib-pthread-once unixlib-pthread-cond-timedwait unixlib-sysconf-nprocessors unixlib-sleep-threads unixlib-stdio-short-transfers unixlib-touch-stack-buffers unixlib-memcpy-split-vstm unixlib-eabi-stack-size unixlib-da-heap-fallback unixlib-mmap-refuse-impossible unixlib-free-signal-stack unixlib-vfork-child-pthread-fini unixlib-vfork-exec-heap-limit unixlib-inline-swi-register-variables unixlib-ddeutils-prefix-loop unixlib-scanf-long-long"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
files=$(for p in $ORDER; do grep '^--- a/' $B/patches/$p.patch | sed 's|^--- a/||'; done | sort -u)
for f in $files; do mkdir -p $W/t/$(dirname $f); cp $G/$f $W/t/$f; done
for p in $ORDER; do ( cd $W/t && patch -p1 -s --no-backup-if-mismatch < $B/patches/$p.patch ) || { echo "FAIL  $p does not apply in sequence"; exit 1; }; done
strip() { gcc -x c -fpreprocessed -dD -E -P "$1" 2>/dev/null | sed -e 's/^[[:space:]]*@.*//' -e 's/[[:space:]]@ .*//' -e 's/[[:space:]]\+/ /g' -e 's/^ //; s/ $//' | grep -v '^$'; }
n=0; bad=0; [ -n "${SHOWDIFF:-}" ] && showd=1
for f in $files; do rel=${f#$U/}; n=$((n+1))
  if [ ! -f "$R/$rel" ]; then echo "  no $rel in the release tree"; bad=$((bad+1)); continue; fi
  if ! diff <(strip $W/t/$f) <(strip $R/$rel) > $W/d; then echo "  DIFFERS: $rel"; [ -n "${showd:-}" ] && head -20 $W/d; bad=$((bad+1)); fi
done
[ $bad = 0 ] && echo "ok   $n files of the 17 UnixLib patches, applied in sequence, equal the release build's sources (comments stripped)" || echo "FAIL $bad of $n files differ from the release build's sources"
[ $bad = 0 ]
