#!/bin/bash
# run-host.sh - the C library of modkit on the host: the library sources compiled with ASan and UBSan under the names mk_*, the test program libtest.c built against them (T_HOSTLIB) and against glibc
# (T_ORACLE), both run, the lines  SECTION name n=... hash=...  compared.  Usage: run-host.sh [SCALE]  (default 1);  -v for a verbose diff:  VERBOSE=1 run-host.sh
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
K=$HERE/../..
B=${BUILD:-$HERE/build}
SCALE=${1:-1}
mkdir -p "$B"
# the folder of the files that the stdio test makes (the same folder for the glibc run and the library run)
FS=${FSDIR:-$(mktemp -d /tmp/mkstdio-XXXXXX)}/
trap '[ -z "${FSDIR:-}" ] && rm -rf "$FS"' EXIT
python3 "$HERE/mkrename.py" "$B" "$K/include" >/dev/null
GCCINC=$(gcc -print-file-name=include)
SAN="-fsanitize=address,undefined -fno-sanitize-recover=all"
# every source of lib/ that is not ARM only (heap.c, exit.c, assert.c and start.c use SWIs in inline assembler / need printf / are the start of a runnable module; sclcompat.c is the names of the Shared C Library (ARM inline assembler); cxxrt.c is the C++ runtime of a module: it defines __dso_handle, the host has one; cxxsp.c names a symbol that only a program using make_shared has)
LIBSRC=$(cd "$K/lib" && ls *.c | grep -v -E '^(heap|exit|assert|start|cxxrt|cxxsp|sclcompat|sockets|netdb)\.c$' | sed 's/\.c$//' | tr "\n" " ")
for f in $LIBSRC; do
  case $f in fd_*) NOSH="-fno-sanitize=shift,signed-integer-overflow -fwrapv" ;; *) NOSH="" ;; esac       # Sun's fdlibm shifts and adds signed words as two's complement (fdlibm.h asks for -fwrapv)
  gcc -std=gnu11 -O1 -g -fno-builtin $SAN $NOSH -Wall -Wextra -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -DMODLIB_HOST -Derrno=mk_errno -nostdinc -I"$K/include" -I"$GCCINC" \
      -include "$B/mk_rename.h" -c "$K/lib/$f.c" -o "$B/$f.o"
done
gcc -std=gnu11 -O1 -g $SAN -c "$HERE/hosthooks.c" -o "$B/hosthooks.o"
gcc -std=gnu11 -O1 -g -fno-builtin $SAN -Wall -Wno-unused-function -DT_HOSTLIB -DT_FSDIR="\"$FS\"" -DSCALE="$SCALE" -I"$B" -idirafter "$K/include" -c "$HERE/libtest.c" -o "$B/libtest-hostlib.o"
gcc $SAN -o "$B/libtest-hostlib" "$B/libtest-hostlib.o" "$B/hosthooks.o" $(for f in $LIBSRC; do echo "$B/$f.o"; done) -lm
gcc -std=gnu11 -O1 -g -fno-builtin -Wall -Wno-unused-function -DT_ORACLE -DT_FSDIR="\"$FS\"" -DSCALE="$SCALE" -o "$B/libtest-oracle" "$HERE/libtest.c" -lm
V=""; [ -n "${VERBOSE:-}" ] && V="-v"
"$B/libtest-oracle" $V > "$B/oracle.out" 2>&1 || true
"$B/libtest-hostlib" $V > "$B/hostlib.out" 2>&1 || { echo "the library build stopped:"; tail -5 "$B/hostlib.out"; }
echo "--- oracle:  $(grep -c ^SECTION "$B/oracle.out") sections, $(grep -c '^FAIL' "$B/oracle.out") FAIL lines"
echo "--- hostlib: $(grep -c ^SECTION "$B/hostlib.out") sections, $(grep -c '^FAIL' "$B/hostlib.out") FAIL lines"
grep -E '^(SECTION|FAIL|TOTAL)' "$B/oracle.out" > "$B/oracle.sec"
grep -E '^(SECTION|FAIL|TOTAL)' "$B/hostlib.out" > "$B/hostlib.sec"
# the sections that only the library build has (answers worked out in the test: rand, swix, clock) are not compared with glibc
SECS=$(grep ^SECTION "$B/oracle.sec" | awk '{print $2}' | paste -sd'|')
if diff <(grep ^SECTION "$B/oracle.sec") <(grep -E "^SECTION ($SECS) " "$B/hostlib.sec") ; then echo "SAME: every section of the library build equals glibc's"; else echo "DIFFERENT: see above"; fi
grep '^FAIL' "$B/hostlib.sec" | head -20 || true
# the model of the file system against what the Pi gave (the section probe): the lines must be the same but for the known differences
python3 "$HERE/check-probe.py" "$B/hostlib.out" | tail -1
