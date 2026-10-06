#!/bin/bash
# Re-run every check behind the upstream reports and write verify/VERIFY-LOG.txt:
#   * each patch applies to PRISTINE upstream (svn trunk r7800) with  patch -p1  (and the combinations that make sense)
#   * the patched module/sul.s variants assemble with the GCCSDK 10.2.0 tool chain; the unpatched one rebuilds to the module of that tree; the machine code of each runs correctly on the A32 interpreter
#   * the patched UnixLib files compile with the GCCSDK 10.2.0 and the GCC 16.2.0 EABI compilers
#   * the ARMEABISupport host model: unpatched 30 failed checks, patched 0
#   * the UnixLib heap-growth host model (report 08): the real brk.c / stackalloc.c, unpatched 7 failed checks, patched 0, every breakage of the patch caught; the machine code of the new
#     start-up lines on the A32 interpreter (needs a built patched libunixlib.so: ~/gccsdk-next/unixlib, skipped when it is not there)
#   * every reproducer source compiles with both compilers
#   * reports 19 and 20 (the register-variable SWI wrappers of os.h, __get_dde_prefix): the real unix/unix.c and common/prefix.c compiled with GCC 16.2.0 / 10.2.0 / 4.7.4, the reproducer
#     (regvar1.c) run on the A32 interpreter, the old and the rewritten wrappers compared on the interpreter (32 wrappers x 300 seeds, 10 mutants), the machine code of the pristine and the
#     patched __get_dde_prefix run on the interpreter (11 cases, 5 mutants), the generator reproduces the patched header
#   * report 21 (scanf with long long conversions, stdio/scanf.c): the patched file and the two programs of the machine runs compile with both compilers; the host model of the old and the
#     patched vfscanf (repro/scanf/build-model.sh): 47315 cases 0 failures, the table of 15066 cases made by glibc: new 0 failed, old 5502, glibc 0
#   * reports 22 - 24 (the .fini_array, getrlimit (RLIMIT_STACK), POSIX semaphores): the patched files compile and assemble with both compilers, the three programs of the machine runs compile
#     with both compilers and with the host's gcc, and pass on a Linux host with glibc (the reference for what they expect: finitest 8 checks, rlimtest 6, semtest 18)
# usage: run-verify.sh      (needs: ~/gccsdk = a clean svn working copy of trunk (svn status clean), ~/gccsdk-next = this repository with the cross compiler env-f (docs/BUILDING.md step 4)
#                            and UnixLib built (step 5: ~/gccsdk-next/unixlib); Python 3, patch, gcc)
set -u
HERE=$(cd "$(dirname "$0")" && pwd); B=$(cd "$HERE/.." && pwd); LOG=$HERE/VERIFY-LOG.txt
G=$HOME/gccsdk; N=$HOME/gccsdk-next
CC10=$G/env/bin/arm-riscos-gnueabihf-gcc; CC16=$N/env-f/bin/arm-riscos-gnueabihf-gcc; STRIP=$G/env/arm-riscos-gnueabihf/bin/strip
export NM=$G/env/arm-riscos-gnueabihf/bin/nm
U=gcc4/recipe/files/gcc/libunixlib; fails=0
say() { echo "$@" | tee -a "$LOG"; }
chk() { if [ "$1" = 0 ]; then say "  ok    $2"; else say "  FAIL  $2"; fails=$((fails+1)); fi; }
: > "$LOG"
say "VERIFY-LOG  $(date '+%Y-%m-%d %H:%M')   run by verify/run-verify.sh"
( cd $G && say "upstream: $(svn info --show-item url gcc4 2>/dev/null) revision $(svn info --show-item revision gcc4 2>/dev/null); working copy changes in the files used: $(svn status gcc4/riscos/armeabisupport $U 2>&1 | wc -l)" )
say ""; say "1. every patch applies to pristine upstream"
W=$(mktemp -d)
apply() { # name patches...
  rm -rf $W/t && mkdir -p $W/t
  for p in "${@:2}"; do for f in $(grep '^--- a/' $B/patches/$p | sed 's|^--- a/||'); do [ -f $W/t/$f ] || { mkdir -p $W/t/$(dirname $f); cp $G/$f $W/t/$f; }; done; done
  ( cd $W/t && for p in "${@:2}"; do patch -p1 -s --no-backup-if-mismatch < $B/patches/$p || exit 1; done ); chk $? "$1"
}
for p in $(cd $B/patches && ls *.patch); do apply "$p alone" $p; done
apply "sul-1 then sul-2" sul-1-vfork-child-main-stack.patch sul-2-vfork-child-wimp-slot.patch
apply "sul-2 then sul-3" sul-2-vfork-child-wimp-slot.patch sul-3-vfork-child-execed-flag.patch
say ""; say "2. module/sul.s: assemble (GCCSDK 10.2.0) and run the machine code on the A32 interpreter"
M=$HERE/sul; mkdir -p $M
for v in pristine stack slot execed all; do
  if [ $v = pristine ]; then cp $G/$U/module/sul.s $M/sul-pristine.s; else
    rm -rf $W/s && mkdir -p $W/s/$U/module && cp $G/$U/module/sul.s $W/s/$U/module/sul.s
    case $v in stack) pp="sul-1-vfork-child-main-stack.patch";; slot) pp="sul-2-vfork-child-wimp-slot.patch";; execed) pp="sul-3-vfork-child-execed-flag.patch";; all) pp="sul-all-vfork-child.patch";; esac
    ( cd $W/s && patch -p1 -s < $B/patches/$pp ) && cp $W/s/$U/module/sul.s $M/sul-$v.s; fi
  $CC10 -xassembler-with-cpp -isystem "$G/$U/include" -I "$G/$U/incl-local" -D__UNIXLIB_CHUNKED_STACK=0 -g -O2 -o $M/sul-$v.o -c $M/sul-$v.s 2>>"$LOG"; chk $? "assembles: $v ($(stat -c %s $M/sul-$v.o >/dev/null 2>&1 && echo ok))"
  $STRIP -O binary -o $M/sul-$v.bin $M/sul-$v.o; say "        sul-$v.bin  $(stat -c %s $M/sul-$v.bin) bytes  sha1 $(sha1sum $M/sul-$v.bin | cut -c1-16)"
done
T=$HERE/tools
run() { python3 $T/$1 "${@:2}" 2>&1 | tail -1 | tee -a "$LOG" | grep -q "right\|^ok\|ok  :" ; chk $? "$1 ${*:2}"; }
run sim-sul-exit.py $M/sul-pristine.bin $M/sul-pristine.o orig
run sim-sul-exit.py $M/sul-stack.bin $M/sul-stack.o fixed
run sim-sul-exit.py $M/sul-all.bin $M/sul-all.o fixed
run sim-sul-slot.py $M/sul-pristine.bin $M/sul-pristine.o old
run sim-sul-slot.py $M/sul-slot.bin $M/sul-slot.o fixed2
run sim-sul-slot.py $M/sul-all.bin $M/sul-all.o fixed2
run sim-sul-fork.py $M/sul-pristine.bin $M/sul-pristine.o $M/sul-execed.bin $M/sul-execed.o
if [ -f $N/sul-build/sul-fixed2.bin ]; then run sim-sul-fork.py $N/sul-build/sul-fixed2.bin $N/sul-build/sul-fixed2.o $M/sul-all.bin $M/sul-all.o
else say "  (no $N/sul-build/sul-fixed2.bin: build it with recipe/gcc-16.2.0-riscos/scripts/build-sul.sh; the check of that build is skipped)"; fi
say ""; say "3. UnixLib: the patched files compile (the flags of the UnixLib build, GCC 10.2.0 and GCC 16.2.0)"
FL="-DHAVE_CONFIG_H -D__GNU_LIBRARY__ -DNO_LONG_DOUBLE -D_GNU_SOURCE=1 -D__UNIXLIB_NO_NONNULL -std=c99 -g -O2"
for cc in "$CC10|GCC 10.2.0" "$CC16|GCC 16.2.0"; do C=${cc%%|*}; NAME=${cc##*|}
  for spec in "pthread/pthinit.c|unixlib-vfork-child-pthread-fini|" "unix/unix.c|unixlib-free-signal-stack|$B/src/unixlib-free-signal-stack/incl-local" "sys/exec.c|unixlib-free-signal-stack|$B/src/unixlib-free-signal-stack/incl-local" "sys/mman-armeabi.c|unixlib-mmap-refuse-impossible|" "sys/stackalloc.c|unixlib-vfork-exec-heap-limit|$B/src/unixlib-vfork-exec-heap-limit/incl-local"; do
    f=${spec%%|*}; r=${spec#*|}; pn=${r%%|*}; inc=${r#*|}
    $C $FL -I$N/unixlib/build -isystem $G/$U/include ${inc:+-I$inc} -I $G/$U/incl-local -c $B/src/$pn/$f -o $W/x.o 2>>"$LOG"; chk $? "$NAME compiles $f (patched by $pn)"; done; done
for cc in "$CC10|GCC 10.2.0" "$CC16|GCC 16.2.0"; do C=${cc%%|*}; NAME=${cc##*|}
  $C -xassembler-with-cpp -isystem $G/$U/include -I $B/src/unixlib-vfork-exec-heap-limit/incl-local -I $G/$U/incl-local -D__UNIXLIB_CHUNKED_STACK=0 -g -O2 -fPIC -DPIC -c $B/src/unixlib-vfork-exec-heap-limit/sys/_syslib.s -o $W/x.o 2>>"$LOG"; chk $? "$NAME assembles sys/_syslib.s (patched by unixlib-vfork-exec-heap-limit)"; done
say ""; say "4. ARMEABISupport host model (the module's own memory.c / mmap.c, unpatched and patched, on a model of the OS)"
A=$G/gcc4/riscos/armeabisupport; rm -rf $W/o $W/n $W/p; mkdir -p $W/o $W/n $W/p/gcc4/riscos/armeabisupport; cp $A/*.c $A/*.h $W/o/; cp $A/*.c $A/*.h $W/n/; cp $A/memory.c $A/mmap.c $W/p/gcc4/riscos/armeabisupport/
( cd $W/p && patch -p1 -s < $B/patches/armeabisupport-mmap-failure-cleanup.patch ) && cp $W/p/gcc4/riscos/armeabisupport/memory.c $W/p/gcc4/riscos/armeabisupport/mmap.c $W/n/
r1=$($B/repro/armeabisupport-model/build-and-run.sh $W/o original 2>&1 | tail -1); say "  $r1"; echo "$r1" | grep -q "30 check" ; chk $? "unpatched: 30 failed checks expected"
r2=$($B/repro/armeabisupport-model/build-and-run.sh $W/n patched 2>&1 | tail -1); say "  $r2"; echo "$r2" | grep -q ": 0 check" ; chk $? "patched: 0 failed checks expected"
say ""; say "4b. UnixLib heap growth of a vfork + exec child (report 08): the REAL sys/brk.c and sys/stackalloc.c on a host model, with the numbers measured on the machine"
H=$B/repro/heap-model; rm -rf $W/hp && mkdir -p $W/hp/gcc4/recipe/files/gcc/libunixlib
( cd $G/$U && cp --parents incl-local/internal/unix.h incl-local/internal/asm_dec.s sys/_syslib.s sys/stackalloc.c sys/brk.c $W/hp/gcc4/recipe/files/gcc/libunixlib/ )
r1=$($H/build-and-run.sh $W/hp/gcc4/recipe/files/gcc/libunixlib original 2>&1 | tail -1); say "  $r1"; echo "$r1" | grep -q "original: 7 check" ; chk $? "unpatched: 7 failed checks expected (the bug: memory handed out inside the saved parent)"
( cd $W/hp && patch -p1 -s < $B/patches/unixlib-vfork-exec-heap-limit.patch ) ; chk $? "patch applies to the files of the model"
r2=$($H/build-and-run.sh $W/hp/gcc4/recipe/files/gcc/libunixlib patched 2>&1 | tail -1); say "  $r2"; echo "$r2" | grep -q "patched: 0 check" ; chk $? "patched: 0 failed checks expected"
r3=$($H/build-and-run.sh $W/hp/gcc4/recipe/files/gcc/libunixlib patched mutate 2>&1 | tail -1); say "  $r3"; echo "$r3" | grep -q ", 0 not caught" ; chk $? "every breakage of the patched check is caught"
t1=$($H/build-and-run.sh $G/$U original 2>&1 | grep TRACE | sort); t2=$($H/build-and-run.sh $W/hp/gcc4/recipe/files/gcc/libunixlib patched 2>&1 | grep TRACE | sort); [ -n "$t1" ] && [ "$t1" = "$t2" ]; chk $? "a program that was not started by exec: the sequence of requests and the slot sizes are identical before and after"
UL=$N/unixlib/build/.libs/libunixlib.so.5.0.0
if [ -f $UL ]; then
  python3 $T/sim-startup-loops.py $UL > $W/sim.out 2>&1; chk $? "start-up lines (himem_max_start .. himem_max_end) on the A32 interpreter: $(grep -c 'himem_max:' $W/sim.out) checks"
  python3 $T/mutate-himem-max.py $UL 2>&1 | tail -1 | tee -a "$LOG" | grep -q ", 0 not caught"; chk $? "every breakage of those instructions is caught"
  python3 $T/mutate-stack-da.py $UL 2>&1 | tail -1 | tee -a "$LOG" | grep -q ", 0 not caught"; chk $? "every breakage of the stack_try / da_try loops is caught"
else say "  (no built patched libunixlib at $UL: the interpreter checks are skipped)"; fi
say ""; say "5. every reproducer source compiles (GCC 10.2.0 and GCC 16.2.0)"
for cc in "$CC10|GCC 10.2.0" "$CC16|GCC 16.2.0"; do C=${cc%%|*}; NAME=${cc##*|}
  for f in $B/repro/sul/*.c $B/repro/armeabisupport/*.c $B/repro/svc-abort/*.c $B/repro/heap/*.c; do extra=""; case $f in *svc_abort_repro.c) extra="-fno-stack-clash-protection";; esac
    $C -std=gnu11 -O2 $extra -I $B/repro/sul -I $B/repro/armeabisupport -o $W/x.e1f $f -lm 2>>"$LOG"; chk $? "$NAME compiles ${f#$B/}"; done; done
say ""; say "6. the ten further UnixLib patches (reports 09 - 18): compile with GCC 10.2.0 and GCC 16.2.0, assemble the .s files"
FL2="$FL -I$N/unixlib/build"
for cc in "$CC10|GCC 10.2.0" "$CC16|GCC 16.2.0"; do C=${cc%%|*}; NAME=${cc##*|}
  for spec in "unix/stat.c|unixlib-gcc14-implicit-declarations|" "unix/lstat.c|unixlib-gcc14-implicit-declarations|" "unix/ul_close.c|unixlib-gcc14-implicit-declarations|" "string/strndup.c|unixlib-gcc14-implicit-declarations|" "stdlib/msort.c|unixlib-gcc14-implicit-declarations|" \
              "pthread/once.c|unixlib-pthread-once|-fexceptions" "pthread/cond.c|unixlib-pthread-cond-timedwait|" "signal/sleep.c|unixlib-sleep-threads|" \
              "stdio/fread.c|unixlib-stdio-short-transfers|" "stdio/fwrite.c|unixlib-stdio-short-transfers|" \
              "unix/dev.c|unixlib-touch-stack-buffers|-I$B/src/unixlib-touch-stack-buffers/incl-local" "netlib/recv.c|unixlib-touch-stack-buffers|-I$B/src/unixlib-touch-stack-buffers/incl-local" "netlib/recvfrom.c|unixlib-touch-stack-buffers|-I$B/src/unixlib-touch-stack-buffers/incl-local"; do
    f=${spec%%|*}; r=${spec#*|}; pn=${r%%|*}; extra=${r#*|}
    $C $FL2 -isystem $G/$U/include $extra -I $G/$U/incl-local -c $B/src/$pn/$f -o $W/x.o 2>>"$LOG"; chk $? "$NAME compiles $f (patched by $pn)"; done
  for spec in "sys/_syslib.s|unixlib-eabi-stack-size" "sys/_syslib.s|unixlib-da-heap-fallback" "string/_memcpymove-v7l.s|unixlib-memcpy-split-vstm"; do f=${spec%%|*}; pn=${spec#*|}
    $C -xassembler-with-cpp -isystem $G/$U/include -I $G/$U/incl-local -I $G/$U/string -D__UNIXLIB_CHUNKED_STACK=0 -g -O2 -fPIC -DPIC -c $B/src/$pn/$f -o $W/x.o 2>>"$LOG"; chk $? "$NAME assembles $f (patched by $pn)"; done; done
$CC16 $FL2 -isystem $G/$U/include -I $G/$U/incl-local -fexceptions -c $B/src/unixlib-pthread-once/pthread/once.c -o $W/once.o 2>>"$LOG" && $G/env/arm-riscos-gnueabihf/bin/readelf -u $W/once.o 2>/dev/null | grep -qi "personality"; chk $? "pthread/once.c built with -fexceptions has an unwind table with a personality routine (it is the C++ exceptions that the unwinder has to take through pthread_once)"
n=0; for f in stat.c lstat.c ul_close.c strndup.c msort.c; do d=unix; [ $f = strndup.c ] && d=string; [ $f = msort.c ] && d=stdlib
  $CC16 $FL2 -isystem $G/$U/include -I $G/$U/incl-local -c $G/$U/$d/$f -o $W/x.o 2>$W/err.txt && continue; grep -q "error: implicit declaration" $W/err.txt && n=$((n+1)); done
[ $n = 5 ]; chk $? "pristine trunk: GCC 16.2.0 rejects all 5 files that the gcc14 patch repairs ($n of 5 give 'error: implicit declaration')"
n=0; for f in unix/stat.c unix/lstat.c unix/ul_close.c string/strndup.c stdlib/msort.c; do
  $CC10 $FL2 -w -isystem $G/$U/include -I $G/$U/incl-local -c $G/$U/$f -o $W/a.o 2>/dev/null; $CC10 $FL2 -w -isystem $G/$U/include -I $G/$U/incl-local -c $B/src/unixlib-gcc14-implicit-declarations/$f -o $W/b.o 2>/dev/null
  for o in a b; do $G/env/arm-riscos-gnueabihf/bin/objdump -dr --no-show-raw-insn $W/$o.o | grep -v "file format" | sed -e 's/^ *[0-9a-f]*:\t//' | grep -v "^$" > $W/$o.txt; done
  cmp -s $W/a.txt $W/b.txt && [ -s $W/a.txt ] && n=$((n+1)); done
[ $n = 5 ]; chk $? "gcc14 patch: the code that GCC 10.2.0 generates for the 5 files is identical before and after ($n of 5)"
echo '#include <unistd.h>
_Static_assert (_SC_NPROCESSORS_ONLN != _SC_BC_DIM_MAX, "NPROCESSORS_ONLN equals BC_DIM_MAX");
_Static_assert (_SC_PHYS_PAGES != _SC_BC_BASE_MAX, "PHYS_PAGES equals BC_BASE_MAX");
_Static_assert (_SC_PAGE_SIZE == _SC_PAGESIZE, "PAGE_SIZE differs from PAGESIZE");
#ifndef _SC_NPROCESSORS_ONLN
#error _SC_NPROCESSORS_ONLN is not a macro: the #ifdef in sysconf.c is dead
#endif' > $W/sc.c
$CC16 -fsyntax-only -isystem $G/$U/include $W/sc.c 2>$W/sc.err; [ $? != 0 ] && [ $(grep -c "error" $W/sc.err) -ge 3 ]; chk $? "pristine <unistd.h>: _SC_NPROCESSORS_ONLN == _SC_BC_DIM_MAX, _SC_PHYS_PAGES == _SC_BC_BASE_MAX, and no macro for the #ifdef ($(grep -c error $W/sc.err) errors)"
$CC16 -fsyntax-only -isystem $B/src/unixlib-sysconf-nprocessors/include -isystem $G/$U/include $W/sc.c 2>>"$LOG"; chk $? "patched <unistd.h>: all four conditions hold (GCC 16.2.0)"
$CC10 -fsyntax-only -isystem $B/src/unixlib-sysconf-nprocessors/include -isystem $G/$U/include $W/sc.c 2>>"$LOG"; chk $? "patched <unistd.h>: all four conditions hold (GCC 10.2.0)"
for v in pristine patched; do s=$G/$U/string/_memcpymove-v7l.s; [ $v = patched ] && s=$B/src/unixlib-memcpy-split-vstm/string/_memcpymove-v7l.s
  $CC16 -xassembler-with-cpp -isystem $G/$U/include -I $G/$U/incl-local -I $G/$U/string -D__UNIXLIB_CHUNKED_STACK=0 -g -O2 -fPIC -DPIC -c $s -o $W/mm-$v.o 2>>"$LOG"; done
python3 $T/check-vstm-split.py $G/env/arm-riscos-gnueabihf/bin/objdump $W/mm-pristine.o $W/mm-patched.o 2>&1 | tee -a "$LOG" | grep -q "^ok"; chk $? "memcpy: the patched file differs from the original only by the split of each 64-byte vstm"
say ""; say "7. host models of the ten patches (the real source files where the host can run them)"
SM=$B/repro/stdio-model; rm -rf $W/sp && mkdir -p $W/sp/gcc4/recipe/files/gcc && cp -r $G/$U $W/sp/gcc4/recipe/files/gcc/ && ( cd $W/sp && patch -p1 -s < $B/patches/unixlib-stdio-short-transfers.patch )
r=$($SM/build-and-run.sh $G/$U "original" 2>&1 | tail -1); say "  $r"; echo "$r" | grep -q " [1-9][0-9]* failed check"; chk $? "stdio: the real fread.c / fwrite.c of trunk fail when read () / write () return short counts"
r=$(MODEL_FULL_ONLY=1 $SM/build-and-run.sh $G/$U "original, full transfers only" 2>&1 | tail -1); say "  $r"; echo "$r" | grep -q " 0 failed check"; chk $? "stdio: the same model passes on trunk when every transfer is complete (the model itself is sound)"
r=$($SM/build-and-run.sh $W/sp/gcc4/recipe/files/gcc/libunixlib patched 2>&1 | tail -1); say "  $r"; echo "$r" | grep -q " 0 failed check"; chk $? "stdio: patched fread.c / fwrite.c: 0 failed checks"
r=$($SM/build-and-run.sh $W/sp/gcc4/recipe/files/gcc/libunixlib patched mutate 2>&1 | tail -1); say "  $r"; echo "$r" | grep -q ", 0 not caught"; chk $? "stdio: every breakage of the new lines is caught"
TM=$B/repro/touch-model
r=$($TM/build-and-run.sh $B/src/unixlib-touch-stack-buffers/incl-local/internal/unix.h 2>&1 | tail -1); say "  $r"; echo "$r" | grep -q " 0 failed check"; chk $? "touch: __touch_stack_buffer touches exactly the pages of a stack buffer"
r=$($TM/build-and-run.sh $B/src/unixlib-touch-stack-buffers/incl-local/internal/unix.h mutate 2>&1 | tail -1); say "  $r"; echo "$r" | grep -q ", 0 not caught"; chk $? "touch: every breakage of it is caught"
gcc -O2 -pthread -w $B/repro/unixlib-models/once_model.c -o $W/once_model; $W/once_model old >/dev/null 2>&1; [ $? = 3 ]; chk $? "once: the old pthread_once (one mutex held across the init routine) deadlocks in the std::async pattern (the 3 s watchdog fires)"
$W/once_model new 2>&1 | tee -a "$LOG" | grep -q "all checks passed"; chk $? "once: the new state machine passes (exactly once, waiters wait, std::async pattern completes)"
rm -rf $W/cm && cp -r $B/repro/unixlib-models/cond $W/cm && ( cd $W/cm && ./build-model.sh 2>&1 | tee -a "$LOG" | grep -q "^PASS" ); chk $? "cond: cond_deadline () extracted from the patched cond.c: 3000000 random deadlines, never early, at most 1.1 cs late"
say ""; say "8. the UnixLib patches against the release build, and the test programs of reports 10 - 18 compile"
if [ -d $N/unixlib/root/libunixlib ]; then
  $T/check-fidelity.sh $B $G $N/unixlib/root/libunixlib 2>&1 | tee -a "$LOG" | grep -q "^ok"; chk $? "the 20 UnixLib patches applied in sequence = the sources of the 16.2.0-12 release build"
else say "  (no copy of the release build's sources at $N/unixlib: the fidelity check is skipped)"; fi
for cc in "$CC10|GCC 10.2.0|-std=c++17" "$CC16|GCC 16.2.0|-std=c++20"; do C=${cc%%|*}; r=${cc#*|}; NAME=${r%%|*}; CXXSTD=${r#*|}; CXX=${C%gcc}g++
  $CXX $CXXSTD -O2 -w -c $B/repro/unixlib-models/threadtest.cc -o $W/x.o 2>>"$LOG"; chk $? "$NAME compiles repro/unixlib-models/threadtest.cc ($CXXSTD)"
  for f in stkinfo chain daprobe; do $C -std=gnu11 -O2 -DSTACK_MB=64 -I $B/repro/unixlib-models -o $W/x.e1f $B/repro/unixlib-models/$f.c 2>>"$LOG"; chk $? "$NAME compiles repro/unixlib-models/$f.c"; done
  $C -std=gnu11 -O2 -fno-stack-clash-protection -o $W/x.e1f $B/repro/svc-abort/readtest2.c 2>>"$LOG"; chk $? "$NAME compiles repro/svc-abort/readtest2.c"; done
say ""; say "9. reports 19 and 20: the register-variable SWI wrappers (incl-local/internal/os.h) and __get_dde_prefix (common/prefix.c)"
CC47=$G/cross/bin/arm-unknown-riscos-gcc; OD=$G/env/arm-riscos-gnueabihf/bin/objdump; OSH=$B/src/unixlib-inline-swi-register-variables/incl-local; FL9="$FL -I$N/unixlib/build"
unixinit_seq() { $OD -d --no-show-raw-insn "$1" | awk '/<__unixinit>:/{p=1;next} /^[0-9a-f]+ <.*>:$/{p=0} p' | awk '/svc[[:space:]]+0x00062583/ {on=1; next} on && /bl[[:space:]]/ {print "BL"; exit} on && /^[[:space:]]*[0-9a-f]+:[[:space:]]+mov[[:space:]]+r[0-9]+, r0$/ {print "CAPTURED"}' | tr '\n' ' '; }
for cc in "$CC16|GCC 16.2.0|BL |CAPTURED BL |does NOT copy the size out of r0 before the first call (the bug)" "$CC10|GCC 10.2.0|CAPTURED BL |CAPTURED BL |copies the size out of r0 before the first call"; do
  IFS='|' read -r C NAME wp wn what <<< "$cc"
  $C $FL9 -isystem $G/$U/include -I $G/$U/incl-local -c $G/$U/unix/unix.c -o $W/up.o 2>>"$LOG"; chk $? "$NAME compiles the pristine unix/unix.c"
  $C $FL9 -isystem $G/$U/include -I $OSH -I $G/$U/incl-local -c $G/$U/unix/unix.c -o $W/un.o 2>>"$LOG"; chk $? "$NAME compiles the pristine unix/unix.c with the patched os.h"
  s=$(unixinit_seq $W/up.o); [ "$s" = "$wp" ]; chk $? "$NAME, pristine os.h: __unixinit $what after SWI DDEUtils_GetCLSize (sequence '$s')"
  s=$(unixinit_seq $W/un.o); [ "$s" = "$wn" ]; chk $? "$NAME, patched os.h: __unixinit copies the size out of r0 before the first call (sequence '$s')"
done
files9=$(cd $G/$U && grep -l "SWI_" -r . --include=*.c | grep -v "^./test/" | grep -v "/scl_" | sort)     # the scl_*.c files are SharedCLibrary-only and do not compile for EABI with either header
for cc in "$CC10|GCC 10.2.0" "$CC16|GCC 16.2.0"; do C=${cc%%|*}; NAME=${cc##*|}; n=0; t=0
  for f in $files9; do t=$((t+1)); $C $FL9 -isystem $G/$U/include -I $OSH -I $G/$U/incl-local -c $G/$U/$f -o $W/x.o 2>>"$LOG" && n=$((n+1)) || say "        does not compile: $f"; done
  [ $n = $t ]; chk $? "$NAME compiles the $t source files of trunk that use an SWI wrapper (all but the SharedCLibrary-only scl_*.c), with the patched os.h ($n of $t)"; done
for v in "$CC16|GCC 16.2.0|NOT captured|captured" "$CC10|GCC 10.2.0|captured|captured" "$CC47|GCC 4.7.4|captured|captured"; do
  IFS='|' read -r C NAME wp wn <<< "$v"
  r=$($T/check-regvar-asm.sh $C $B/repro/regvar/regvar1.c); [ "$r" = "$wp" ]; chk $? "$NAME: regvar1.c, wrapper as in os.h: the result of the asm is $r before the first call (expected: $wp)"
  r=$($T/check-regvar-asm.sh $C $B/repro/regvar/regvar1.c -DFIXED); [ "$r" = "$wn" ]; chk $? "$NAME: regvar1.c, rewritten wrapper: $r (expected: $wn)"; done
python3 $T/sim-regvar.py $B/repro/regvar/regvar1.c $CC16 $CC10 > $W/regvar.out 2>&1; cat $W/regvar.out | tee -a "$LOG" | sed 's/^/        /' > /dev/null
grep -q 'GCC 16.2.0.*wrapper as in os.h  *f ("hello") = 10 ' $W/regvar.out; chk $? "interpreter: GCC 16.2.0, wrapper as in os.h: f (\"hello\") = 10, not 12 (arg_size = the result of strlen)"
grep -q 'GCC 16.2.0.*FIXED wrapper  *f ("hello") = 12 ' $W/regvar.out; chk $? "interpreter: GCC 16.2.0, rewritten wrapper: f (\"hello\") = 12"
grep -q 'GCC 10.2.0.*wrapper as in os.h  *f ("hello") = 12 ' $W/regvar.out && grep -q 'GCC 10.2.0.*FIXED wrapper  *f ("hello") = 12 ' $W/regvar.out; chk $? "interpreter: GCC 10.2.0: f (\"hello\") = 12 with both wrappers"
A32_CC=$CC16 A32_LIBROOT=$G/$U A32_CONFIG_INC=$N/unixlib/build python3 $T/sim-swi-wrappers.py $G/$U/incl-local/internal/os.h $OSH/internal/os.h > $W/swi.out 2>&1; tail -n 12 $W/swi.out | grep "^RESULT\|^MUTANTS" | tee -a "$LOG"
grep -q "RESULT: all 32 wrappers behave identically" $W/swi.out; chk $? "the 32 rewritten wrappers behave like the old ones on the interpreter (300 seeds each)"
grep -q "MUTANTS: 10 of 10 caught" $W/swi.out; chk $? "every breakage of the rewritten wrappers is caught"
python3 $B/repro/regvar/fix-unixlib-regvars.py $G/$U/incl-local/internal/os.h $W/os-gen.h 2>>"$LOG"
sh() { gcc -x c -fpreprocessed -dD -E -P "$1" 2>/dev/null | sed -e 's/[[:space:]]\+/ /g' -e 's/^ //; s/ $//' | grep -v '^$'; }
diff <(sh $W/os-gen.h) <(sh $OSH/internal/os.h) > /dev/null; chk $? "fix-unixlib-regvars.py applied to the pristine os.h gives the header of the patch (comments aside)"
python3 $T/sim-get-dde-prefix.py $G/$U $N/unixlib/build $CC16 $G/$U/common/prefix.c $B/src/unixlib-ddeutils-prefix-loop/common/prefix.c $OSH > $W/pfx.out 2>&1; grep "^RESULT\|^MUTANTS" $W/pfx.out | tee -a "$LOG"
grep -q "^RESULT: 11 of 11 cases as expected" $W/pfx.out; chk $? "the machine code of __get_dde_prefix: the pristine file loops for ever when a prefix is set, the patched file gives the right string in all 11 cases"
grep -q "^MUTANTS: 5 of 5 caught" $W/pfx.out; chk $? "every breakage of the patched prefix.c is caught"
for cc in "$CC10|GCC 10.2.0" "$CC16|GCC 16.2.0"; do C=${cc%%|*}; NAME=${cc##*|}
  $C $FL9 -isystem $G/$U/include -I $OSH -I $G/$U/incl-local -c $B/src/unixlib-ddeutils-prefix-loop/common/prefix.c -o $W/x.o 2>>"$LOG"; chk $? "$NAME compiles the patched common/prefix.c"
  $C -O2 -std=gnu11 -DNO_MAIN -c $B/repro/regvar/regvar1.c -o $W/x.o 2>>"$LOG"; chk $? "$NAME compiles repro/regvar/regvar1.c"; done
say ""; say "10. report 21: scanf with long long conversions (stdio/scanf.c): the patched file and the programs of the machine runs compile; the host model of the old and the patched vfscanf"
for cc in "$CC10|GCC 10.2.0" "$CC16|GCC 16.2.0"; do C=${cc%%|*}; NAME=${cc##*|}
  $C $FL9 -isystem $G/$U/include -I $G/$U/incl-local -c $B/src/unixlib-scanf-long-long/stdio/scanf.c -o $W/x.o 2>>"$LOG"; chk $? "$NAME compiles the patched stdio/scanf.c"
  for f in scantest scanfcheck; do $C -std=gnu11 -O2 -o $W/x.e1f $B/repro/scanf/$f.c 2>>"$LOG"; chk $? "$NAME compiles repro/scanf/$f.c"; done; done
W=$W/scanfmodel $B/repro/scanf/build-model.sh $G > $W/scanf-model.out 2>&1; chk $? "repro/scanf/build-model.sh builds the models and runs"
tail -n 12 $W/scanf-model.out | sed 's/^/        /' | tee -a "$LOG" > /dev/null
grep -q "^scanf model test: 47315 cases, 0 failures" $W/scanf-model.out; chk $? "scanf model: old == new for the modifiers none / h / l, new == glibc for hh ll j q z t and %Lf, the canary cases: 47315 cases, 0 failures"
sc() { awk -v m="$1" 'index($0, m) {f=1; next} f && /^scanfcheck:/ {print; exit}' $W/scanf-model.out; }
sc "table on the NEW code" | grep -q "15066 cases, 0 failed"; chk $? "scanf model: the table of 15066 cases (made by glibc) on the patched function: 0 failed"
sc "table on the OLD code" | grep -q "15066 cases, 5502 failed"; chk $? "scanf model: the same table on the unpatched function: 5502 failed (the number the machine gave)"
sc "table on glibc itself" | grep -q "15066 cases, 0 failed"; chk $? "scanf model: the table on glibc itself: 0 failed (the table is right)"
say ""; say "11. reports 22 - 24: the .fini_array (stdlib/atexit.c, sys/_syslib.s), getrlimit (RLIMIT_STACK) (resource/initialise.c, sys/_syslib.s), POSIX semaphores (pthread/sem.c, include/semaphore.h)"
FL11="$FL -I$N/unixlib/build"
for cc in "$CC10|GCC 10.2.0" "$CC16|GCC 16.2.0"; do C=${cc%%|*}; NAME=${cc##*|}
  for spec in "stdlib/atexit.c|unixlib-fini-array|" "resource/initialise.c|unixlib-getrlimit-stack|" "pthread/sem.c|unixlib-semaphores|-isystem $B/src/unixlib-semaphores/include"; do
    f=${spec%%|*}; r=${spec#*|}; pn=${r%%|*}; extra=${r#*|}
    $C $FL11 $extra -isystem $G/$U/include -I $G/$U/incl-local -c $B/src/$pn/$f -o $W/x.o 2>>"$LOG"; chk $? "$NAME compiles $f (patched by $pn)"; done
  for pn in unixlib-fini-array unixlib-getrlimit-stack; do
    $C -xassembler-with-cpp -isystem $G/$U/include -I $G/$U/incl-local -D__UNIXLIB_CHUNKED_STACK=0 -g -O2 -fPIC -DPIC -c $B/src/$pn/sys/_syslib.s -o $W/x.o 2>>"$LOG"; chk $? "$NAME assembles sys/_syslib.s (patched by $pn)"; done
  for f in fini-array/finitest stack-limit/rlimtest semaphores/semtest; do $C -std=gnu11 -O2 -o $W/x.e1f $B/repro/$f.c 2>>"$LOG"; chk $? "$NAME compiles repro/$f.c"; done; done
$CC16 -std=gnu11 -O2 -DBIGSTACK -o $W/x.e1f $B/repro/stack-limit/rlimtest.c 2>>"$LOG"; chk $? "GCC 16.2.0 compiles repro/stack-limit/rlimtest.c with -DBIGSTACK (__stack_size = 64 MB)"
for f in fini-array/finitest stack-limit/rlimtest semaphores/semtest; do gcc -O2 -pthread -o $W/h_${f#*/} $B/repro/$f.c 2>>"$LOG"; chk $? "the host gcc compiles repro/$f.c"; done
$W/h_finitest 2>&1 | tail -1 | tee -a "$LOG" | grep -q "8 checks, 0 failed"; chk $? "finitest on glibc: the events come in the order that the patch gives (8 checks, 0 failed)"
( ulimit -s 1024; $W/h_rlimtest 1048576 2>&1 | tail -1 | tee -a "$LOG" | grep -q "6 checks, 0 failed" ); chk $? "rlimtest on glibc with a 1 MB stack: 6 checks, 0 failed"
$W/h_semtest 2>&1 | tail -1 | tee -a "$LOG" | grep -q "18 checks, 0 failed"; chk $? "semtest on glibc: 18 checks, 0 failed"
rm -rf $W
sed -i "s#$HOME#~#g" "$LOG"
say ""; say "RESULT: $fails check(s) failed"
exit $((fails != 0))
