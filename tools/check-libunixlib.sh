#!/bin/bash
# Sanity check of a freshly built libunixlib.so.5.0.0 BEFORE it is packaged: look for the code every fix must have left in it (a patch that silently did not apply
# once produced a package without the fix-level answer).  usage: check-libunixlib.sh LIBUNIXLIB.so [OBJDUMP]
LIB=${1:?usage: $0 libunixlib.so [objdump]}
# the objdump of the cross tool chain: the second argument, $OBJDUMP, the one of the usual layout (env-f), or the one on PATH
OD=${2:-${OBJDUMP:-}}
if [ -z "$OD" ]; then
  for c in "$HOME/gccsdk-next/env-f/bin/arm-riscos-gnueabihf-objdump" "$(command -v arm-riscos-gnueabihf-objdump || true)"; do [ -n "$c" ] && [ -x "$c" ] && { OD=$c; break; }; done
fi
[ -x "$OD" ] || { echo "no arm-riscos-gnueabihf-objdump found: give it as the second argument or in \$OBJDUMP" >&2; exit 2; }
fail=0
dis() { "$OD" -d --no-show-raw-insn "$LIB" | awk -v f="<$1>:" '$0 ~ f {p=1; next} /^[0-9a-f]+ <.*>:$/ {p=0} p'; }
need() { # function, pattern, what
  if dis "$1" | grep -q -E "$2"; then echo "  ok   $1: $3"; else echo "  FAIL $1: $3 NOT FOUND"; fail=1; fi
}
need sysconf   'cmp[[:space:]]+r[0-9]+, #18176'            "answers the private selector 0x4700 (fix level)"
need sysconf   '#18177'                                    "answers the private selector 0x4701 (compiler version)"
need __fsread  'bic[[:space:]]+ip, r[0-9]+, #4080'         "touches the pages of a stack buffer before OS_GBPB"
need __sockread 'bic[[:space:]]+[a-z0-9]+, r[0-9]+, #4080' "touches the pages of a stack buffer before the socket read"
need recv      'bic[[:space:]]+[a-z0-9]+, r[0-9]+, #4080'  "touches the pages of a stack buffer before recv"
need pthread_once 'bl|b' "pthread_once present"
# 16.2.0-6 (fix level 8): sysconf answers 8; the main stack of an EABI program is allocated in a loop that retries with half the size (stack_try); the heap dynamic area is created
# in a loop that retries with half the maximum size (da_try: the X form of OS_DynamicArea, a retry on error)
need sysconf   'mov[[:space:]]+r0, #13([[:space:]]|$)'     "answers fix level 13"
need stack_try 'svc[[:space:]]+0x00079d02'                  "asks ARMEABISupport for the stack (StackOp)"
need stack_try 'lsr[[:space:]]+r[0-9]+, r[0-9]+, #1$'       "retries with half the stack size when there is no room"
need da_try    'svc[[:space:]]+0x00020066'                  "creates the heap dynamic area with the X form of OS_DynamicArea"
need da_try    'lsr[[:space:]]+r[0-9]+, r[0-9]+, #1$'       "retries with half the maximum size when there is no room"
need da_last   'svc[[:space:]]+0x00000066'                  "the last try is the plain OS_DynamicArea (the error handler, as before)"
# 16.2.0-5: a function with a frame must carry the stack-clash probe (the compiler default), and memcpy/memmove must not use a 64-byte store-multiple
need __fsread  'mov[[:space:]]+ip, #4096'               "stack-clash probe in the prologue (built with -fstack-clash-protection)"
NM=${OD%objdump}nm
for f in memcpy memmove; do
  read -r addr size <<< "$("$NM" -S "$LIB" | awk -v f="$f" '$4 == f && $3 == "T" {print $1, $2; exit}')"
  if [ -z "$addr" ]; then echo "  FAIL $f: symbol not found"; fail=1; continue; fi
  end=$(printf '0x%x' $(( 0x$addr + 0x$size )))
  n=$("$OD" -d --no-show-raw-insn --start-address=0x$addr --stop-address=$end "$LIB" | grep -c -E "vstm(ia|db)[[:space:]]+[a-z0-9]+!?, \{d0-d7\}|vstmdb[[:space:]]+[a-z0-9]+!?, \{d2-d9\}")
  v=$("$OD" -d --no-show-raw-insn --start-address=0x$addr --stop-address=$end "$LIB" | grep -c -E "vstm")
  if [ "$v" -lt 10 ]; then echo "  FAIL $f: found only $v store-multiples: the check looked at the wrong code"; fail=1
  elif [ "$n" != 0 ]; then echo "  FAIL $f: still has $n 64-byte vstm"; fail=1
  else echo "  ok   $f: no 64-byte vstm ($v smaller store-multiples)"; fi
done
# the two start-up loops (16.2.0-6 and later): run the library's own machine code for them on a small ARM interpreter, with models of StackOp and OS_DynamicArea (sim-startup-loops.py)
if "$OD" -d --no-show-raw-insn "$LIB" | grep -q "<stack_try>:"; then
  sim=$(python3 "$(dirname "$0")/sim-startup-loops.py" "$LIB" 2>&1); rc=$?
  if [ $rc = 0 ]; then echo "  ok   start-up loops: $(echo "$sim" | grep -c '^  ok ') scenarios of the stack and heap loops behave as intended (tools/sim-startup-loops.py)"
  else echo "$sim" | grep -v '^  ok '; echo "  FAIL start-up loops: simulation failed"; fail=1; fi
fi
# 16.2.0-7 (fix level 9): mmap ()/mremap () refuse a request that can never be served before ARMEABISupport makes an area for it (mmap_too_big: 2 GB, the OS clamp of OS_DynamicArea 8, only asked
# for 16 MB or more); the one page signal stack is freed when the last user of the image ends (__signalhandler_stack_free, called by _exit and execve right after __env_riscos)
need mmap_too_big 'cmp[[:space:]]+r[0-9]+, #16777216'        "only reads the OS clamp for requests of 16 MB or more"
need mmap_too_big 'mov[[:space:]]+r0, #102'                   "asks OS_DynamicArea (read only, reason 8) for the clamp"
need mmap        'bl[[:space:]]+[0-9a-f]+ <mmap_too_big>'     "refuses a request that cannot be served (ENOMEM) before ARMEABISupport is asked"
need mremap      'bl[[:space:]]+[0-9a-f]+ <mmap_too_big>'     "does the same when the mapping grows"
need __signalhandler_stack_free '#40194'                       "uses ARMEABISupport_StackOp (GET_STACK, then FREE)"
for f in _exit execve; do
  seq=$(dis "$f" | grep -E "bl[[:space:]]+[0-9a-f]+ <(__env_riscos|__signalhandler_stack_free)>" | sed -E 's/.*<(.*)>.*/\1/' | tr '\n' ' ')
  if [ "$seq" = "__env_riscos __signalhandler_stack_free " ]; then echo "  ok   $f: frees the signal stack right after the environment handlers are off"
  else echo "  FAIL $f: calls seen: '$seq' (expected __env_riscos then __signalhandler_stack_free)"; fail=1; fi
done
if "$NM" -D "$LIB" | grep -q "__signalhandler_stack_free"; then echo "  FAIL __signalhandler_stack_free is exported: add it to the local: list of vscript (a PLT call at exit time)"; fail=1
else echo "  ok   __signalhandler_stack_free is local (direct calls, no PLT entry: nothing is resolved at exit time)"; fi
# 16.2.0-8 (fix level 10): the _exit of a vfork child that ends without exec must leave the RMA block of the program image (shared with its parent) alone: __pthread_prog_fini returns while
# __dynamic_area_refcount is more than 1, and _exit calls it BEFORE __dynamic_area_exit (which takes the process off the count)
need __pthread_prog_fini 'cmp[[:space:]]+r[0-9]+, #1$'             "looks at __dynamic_area_refcount: more than one user of the program image"
need __pthread_prog_fini 'bxhi[[:space:]]+lr'                      "returns at once for a vfork child that shares the image, before the RMA block is freed"
seq=$(dis _exit | grep -E "bl[[:space:]]+[0-9a-f]+ <(__pthread_prog_fini|__dynamic_area_exit)>" | sed -E 's/.*<(.*)>.*/\1/' | tr '\n' ' ')
if [ "$seq" = "__pthread_prog_fini __dynamic_area_exit " ]; then echo "  ok   _exit: __pthread_prog_fini is called before __dynamic_area_exit takes the process off the count"
else echo "  FAIL _exit: calls seen: '$seq' (expected __pthread_prog_fini then __dynamic_area_exit)"; fail=1; fi
# 16.2.0-9 (fix level 11): appspace_himem must never be raised above the permitted RAM limit that the program was started with (the heap of a vfork + exec child grew over the copy of its parent):
# the start-up code stores appspace_himem_max (offset 52 of __ul_memory) and __stackalloc_incr_wimpslot compares the request with it
need __stackalloc_incr_wimpslot 'ldr[[:space:]]+[a-z0-9]+, \[[a-z0-9]+, #52\]'     "reads appspace_himem_max (offset 52 of __ul_memory)"
need __stackalloc_incr_wimpslot 'bcc[[:space:]]|bcs[[:space:]]|bls[[:space:]]|bhi[[:space:]]'   "refuses a request that would pass it (conditional branch to the failure return)"
if "$OD" -d --no-show-raw-insn "$LIB" | grep -q "<himem_max_start>:"; then echo "  ok   __main: start-up lines himem_max_start .. himem_max_end present"
else echo "  FAIL __main: himem_max_start not found: the start-up code does not set appspace_himem_max"; fail=1; fi
# the machine code of the new functions on the interpreter (scenarios + mutants: tools/sim-exit-hooks.py)
if "$OD" -d --no-show-raw-insn "$LIB" | grep -q "<__signalhandler_stack_free>:"; then
  sim=$(python3 "$(dirname "$0")/sim-exit-hooks.py" "$LIB" 2>&1); rc=$?
  if [ $rc = 0 ]; then echo "  ok   exit hooks: $(echo "$sim" | grep -c '^  ok ') scenarios and $(echo "$sim" | grep -o '[0-9]* mutants, [0-9]* caught' | head -1) (tools/sim-exit-hooks.py)"
  else echo "$sim" | grep -v '^  ok '; echo "  FAIL exit hooks: simulation failed"; fail=1; fi
fi
# 16.2.0-10 (fix level 12): the inline SWI wrappers no longer read a register variable after the asm.  GCC 16 had made __unixinit use the result of the strlen () that follows the DDEUtils_GetCLSize
# SWI as the size of the DDEUtils command line (arguments cut to the length of the program name, in a too small heap block): between that SWI and the first call the size must be copied out of r0 by an
# UNCONDITIONAL mov (the conditional movvs / movvc only handle the error pointer).  __get_dde_prefix measured the prefix through *end_prefix (the pointer advances: ldrb ..., [rN, #1]!).
seq=$(dis __unixinit | awk '/svc[[:space:]]+0x00062583/ {on=1; next} on && /bl[[:space:]]/ {print "BL"; exit} on && /^[[:space:]]*[0-9a-f]+:[[:space:]]+mov[[:space:]]+r[0-9]+, r0$/ {print "CAPTURED"}' | tr '\n' ' ')
case "$seq" in
  "CAPTURED BL ") echo "  ok   __unixinit: the size of the DDEUtils command line is copied out of r0 before the first call (no register variable is read after the asm)";;
  *) echo "  FAIL __unixinit: after the GetCLSize SWI the sequence was '$seq' (expected: an unconditional mov rN, r0, then the first call)"; fail=1;;
esac
need __get_dde_prefix 'ldrb[[:space:]]+r[0-9]+, \[r[0-9]+, #1\]!'   "measures the DDEUtils prefix by advancing a pointer (the endless loop on *prefix is gone)"
# every wrapper that stores a result read from a register after the asm: the differential test of the wrappers on the interpreter (tools/sim-swi-wrappers.py) is run by hand from the recipe patches
# 16.2.0-11 (fix level 13): vfscanf understands ll / q / j (strtoll / strtoull into a long long), hh (a char), z / t, and %Lf stores a long double.  The old function was 0xe08 bytes; the new one has the
# second conversion function pointer, the modifier cases and the long long stores (the host model tests/unixlib-fix/scanf runs the same source against glibc: 47315 cases).  A build without the patch fails here.
read -r addr size <<< "$("$NM" -S "$LIB" | awk '$4 == "vfscanf" && $3 == "T" {print $1, $2; exit}')"
if [ -z "$addr" ]; then echo "  FAIL vfscanf: symbol not found"; fail=1
elif [ $(( 0x$size )) -lt $(( 0xf00 )) ]; then echo "  FAIL vfscanf: only $(( 0x$size )) bytes (0xe08 = 3592 is the unpatched one): the long long scanf patch is not in"; fail=1
else echo "  ok   vfscanf: $(( 0x$size )) bytes (the unpatched one is 3592): the long long conversions are in"; fi
[ $fail = 0 ] && echo "libunixlib check: OK" || { echo "libunixlib check: FAILED"; exit 1; }
