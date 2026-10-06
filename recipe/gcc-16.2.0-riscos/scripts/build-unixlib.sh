#!/bin/bash
# Build UnixLib 5.0 with the new GCC 16.2 cross compiler, out of tree, reusing the generated configure/Makefile.in
# from the 10.2.0 build tree (it needs ~/gccsdk/build/gcc/gcc-10.2.0/libunixlib; nothing there is modified).
# Result: $W/build/{.libs/libunixlib.so.5.0.0,libm.*,crt0.o,gcrt0.o}  (~11 s on 22 cores; libunixlib.a is not produced: module/sul.s is FPA-only).
set -e
SRC=${SRC:-$HOME/gccsdk/build/gcc/gcc-10.2.0}
W=${W:-$HOME/gccsdk-next/unixlib}
N=${N:-$HOME/gccsdk-next/env-f}      # the clean from-recipe toolchain: -fstack-clash-protection is its default
HERE=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$W/root" "$W/build"
# UnixLib must be built with -fstack-clash-protection (the target default of the new compiler): refuse an older compiler that would silently build it without.
printf 'extern void use(char *);\nvoid f(void) { char b[64]; use(b); }\n' > "$W/probe-check.c"
if ! "$N/bin/arm-riscos-gnueabihf-gcc" -O2 -S -o - "$W/probe-check.c" | grep -q 'mov.*#4096'; then
  echo "ERROR: $N/bin/arm-riscos-gnueabihf-gcc does not use -fstack-clash-protection by default: build UnixLib with the compiler that does"; exit 1
fi
cd "$W/root"
# Apply one patch to the copy of the sources.  A patch that does not apply is a hard error (it was once ignored by `|| true`: a malformed patch silently left the
# fix-level selector out of a shipped package); one that is already applied (the root dir is reused) is skipped.
apply_patch() {
  local f="$HERE/patches-unixlib/$1"
  if patch -p1 -N --dry-run < "$f" > "$W/patch-check.log" 2>&1; then
    patch -p1 -N < "$f" || { echo "ERROR: applying $1 failed"; exit 1; }
  elif grep -q "Reversed (or previously applied) patch detected" "$W/patch-check.log"; then
    echo "  (already applied: $1)"
  else
    echo "ERROR: patch $1 does not apply:"; cat "$W/patch-check.log"; exit 1
  fi
}
[ -d libunixlib ] || cp -rL "$SRC/libunixlib" ./libunixlib
for f in ltmain.sh config.guess config.sub install-sh missing compile depcomp config-ml.in ar-lib mkinstalldirs; do
  [ -e "$SRC/$f" ] && cp -L "$SRC/$f" . || true
done
# GCC >= 14 makes implicit function declarations an error: five one-line missing-declaration fixes.
apply_patch unixlib-gcc14-implicit-declarations.patch
# UnixLib pthread_once() held one global mutex across the init routine: std::async (call_once(join) vs call_once(set result)) deadlocked.
apply_patch unixlib-pthread-once.patch
# pthread_cond_timedwait() worked out its timeout from time () (whole seconds): every timed wait overran by up to a second.
apply_patch unixlib-pthread-cond-timedwait.patch
# _SC_NPROCESSORS_ONLN / _SC_PHYS_PAGES shared a value with _SC_BC_DIM_MAX / _SC_BC_BASE_MAX (sysconf returned 2048 CPUs).
apply_patch unixlib-sysconf-nprocessors.patch
# sysconf (0x4700) returns the fix level (the N of SharedLibs-C-armeabihf 10.2.0-N): lets test programs tell the installed libunixlib apart.
apply_patch unixlib-sysconf-fixlevel.patch
# sleep()/usleep()/nanosleep(): with several threads, the second sleeper replaced the process-wide alarm and woke the first one too early.
apply_patch unixlib-sleep-threads.patch
# fread()/fwrite(): after a short read()/write() the direct-transfer loop carried on at the START of the buffer again (right byte count, wrong data).
apply_patch unixlib-stdio-short-transfers.patch
# Reads into a stack buffer the program has not used yet: the OS (supervisor mode) loses the store that takes the page fault (ARMEABISupport maps stack pages on demand);
# touch the pages of such a buffer in user mode first (read/fread/recv/recvfrom).
apply_patch unixlib-touch-stack-buffers.patch
# memcpy/memmove (the NEON routines): a 64-byte aligned vstm that is the first access to a page of the lazily mapped stack is not restarted (SIGSEGV): split each into two 32-byte stores.
apply_patch unixlib-memcpy-split-vstm.patch
# The main stack of an EABI program was always 1MB: it is now as big as the program's __stack_size says (ARMEABISupport maps stack pages lazily, so a big stack costs only address
# space, of which all stacks share one 256MB range: when that is short, half the size is tried, down to 1MB).  For programs that recurse deeply: the compilers.
apply_patch unixlib-eabi-stack-size.patch
# The heap dynamic area is created with a MAXIMUM size that only reserves address space, and the reservations of the programs alive at once add up: when OS_DynamicArea says no
# ("Unable to allocate logical address space") try half of the size, down to 2MB, instead of ending the program at start-up.
apply_patch unixlib-da-heap-fallback.patch
# sysconf (0x4700) answers 8.
apply_patch unixlib-sysconf-fixlevel-8.patch
# mmap()/mremap(): a request that can never be served (2GB or more, or over the OS clamp on the size of one dynamic area, OS_DynamicArea 8) is refused with ENOMEM before
# ARMEABISupport makes an "mmap#N" area for it that it would never give back (malloc (2GB - 1) left two 100MB areas behind per call; a request over the clamp pinned all the memory it claimed).
apply_patch unixlib-eabi-mmap-limit.patch
# The one page signal stack that start-up takes from ARMEABISupport is freed when the process ends (4KB of the shared "UnixLib stacks" range per process were left until the root process ended).
apply_patch unixlib-eabi-free-signal-stack.patch
# sysconf (0x4700) answers 9.
apply_patch unixlib-sysconf-fixlevel-9.patch
# The _exit of a vfork child that ends WITHOUT exec freed the RMA block of the program image that it shares with its parent (__pthread_prog_fini): the parent freed it a second time, and a
# loop of such children corrupted the RMA heap and froze the machine (RunSul3, 2026-10-03).  The block is now left alone while the image is shared (__dynamic_area_refcount > 1).
apply_patch unixlib-vfork-child-pthread-fini.patch
# sysconf (0x4700) answers 10.
apply_patch unixlib-sysconf-fixlevel-10.patch
# A program that was started by vfork () + exec () is told by SharedUnixLibrary to stay below the copy of its parent, which SharedUnixLibrary keeps between the permitted RAM limit and the end of the Wimp slot,
# but UnixLib's heap code treated that memory as spare slot memory: the malloc heap of such a child (in the Wimp slot) grew over the copy and the parent resumed destroyed (the native g++ driver after cc1plus; a
# 62 MB heap in a 64 MB slot, RunHeap2 2026-10-04).  __stackalloc_incr_wimpslot never raises appspace_himem above the permitted RAM limit that the program was started with (new field appspace_himem_max, set at
# start-up); brk () then fails with ENOMEM and malloc falls back to mmap.  Proven on the machine by RunHeap3.
apply_patch unixlib-vfork-exec-heap-limit.patch
# sysconf (0x4700) answers 11.
apply_patch unixlib-sysconf-fixlevel-11.patch
# The inline SWI wrappers of incl-local/internal/os.h read register variables after the asm: GCC 16 turned that into the length of the program name in place of the size of the DDEUtils
# command line (__unixinit), so that every program with arguments longer than its name was started with them cut (and in a too small heap block) whenever DDEUtils was loaded (RunTb8 - RunTb10, 2026-10-04).
apply_patch unixlib-inline-swi-register-variables.patch
# __get_dde_prefix () looped forever when a DDEUtils prefix was set (it tested *prefix instead of *end_prefix).
apply_patch unixlib-ddeutils-prefix-loop.patch
# sysconf (0x4700) answers 12.
apply_patch unixlib-sysconf-fixlevel-12.patch
# scanf (vfscanf, the old BSD one) knew only l, L and h and converted every integer with strtol / strtoul, storing a long: "%llx" of a 16 digit number gave ULONG_MAX in the low word and left the
# high word as it was, %hhx stored a short, %j %z %t %q were not understood, and %Lf stored a float.  Now ll / q / j (strtoll / strtoull, a long long), hh (a char), z / t (as big as their types),
# %Lf (strtold) and %n with them.  lto1 reads the 64 bit id of its section names with sscanf (".%llx"): it could never find the sections of its own objects (native -flto, 2026-10-05).
apply_patch unixlib-scanf-long-long.patch
# sysconf (0x4700) answers 13.
apply_patch unixlib-sysconf-fixlevel-13.patch
# The .fini_array of an EABI program was never run (crt0.o hands the bounds of the array to __main; "FIXME: what about FINI_ARRAY?"): an __attribute__ ((destructor)) function of a C program
# and the exit hook of libgcov (it writes the .gcda files) were lost.  __main now registers a function with atexit () that calls them, last entry first, before the constructors run.
apply_patch unixlib-fini-array.patch
# getrlimit (RLIMIT_STACK) answered the maximum Wimp slot: the EABI main stack is fixed (1MB unless __stack_size asks for more), and programs size recursion and thread stacks by the limit.
apply_patch unixlib-getrlimit-stack.patch
# POSIX semaphores: sem_wait polled with pthread_yield () and leaked a queue entry per call, sem_timedwait was ENOSYS (and C++20 / libgomp need both).  Now a mutex and a condition variable.
apply_patch unixlib-sem-blocking-timedwait.patch
# <semaphore.h> did not declare sem_timedwait (prepare-sysroot.sh applies this one to the headers of the cross tool chain too).
apply_patch unixlib-semaphore-timedwait-decl.patch
# sysconf (0x4700) answers 14.
apply_patch unixlib-sysconf-fixlevel-14.patch
# TEST BUILDS ONLY (debugging aids that are not part of the library): EXTRA_PATCHES="unixlib-ul-trace.patch" EXTRA_DEFS="-DULTRACE" builds a libunixlib that appends a line to a log file at the steps of
# fork/vfork (see the comment in sys/_vfork.s); the same patch built without EXTRA_DEFS must give the very library of the release (checked by tools/check-ul-trace.sh).  Unset: nothing changes.
for ep in ${EXTRA_PATCHES:-}; do apply_patch "$ep"; done
cd "$W/build"
mkdir -p .deps                       # needed with --disable-dependency-tracking (old automake .S rules)
export PATH=$N/bin:/usr/bin:/bin:/usr/local/bin
../root/libunixlib/configure --srcdir=../root/libunixlib --build=x86_64-pc-linux-gnu --host=arm-riscos-gnueabihf \
  --target=arm-riscos-gnueabihf --enable-shared --disable-multilib --disable-dependency-tracking \
  --prefix="$W/install" CC="$N/bin/arm-riscos-gnueabihf-gcc" AR="$N/bin/arm-riscos-gnueabihf-ar" \
  RANLIB="$N/bin/arm-riscos-gnueabihf-ranlib" CFLAGS="-g -O2${EXTRA_DEFS:+ $EXTRA_DEFS}" > configure.log 2>&1
make -k -j"$(nproc)" MAKEINFO=true > make.log 2>&1 || true   # module/sul.s (FPA assembler for the legacy module) fails on EABI: expected
# pthread_once() (patches-unixlib/unixlib-pthread-once.patch) must be compiled with -fexceptions: a C++ exception thrown by the init
# routine (std::call_once with a throwing callable) has to propagate through it, and reset the once flag on the way.  The rest of
# UnixLib has no unwind tables, so rebuild just this object.  (touch is safe here: $W/root is a copy made with cp -rL.)
touch "$W/root/libunixlib/pthread/once.c"
make once.lo CFLAGS="-g -O2 -fexceptions" MAKEINFO=true > once.log 2>&1
make -k MAKEINFO=true >> make.log 2>&1 || true      # relink libunixlib.la (the FPA-only module/sul.s error is expected)
if "$N/bin/arm-riscos-gnueabihf-readelf" -u .libs/libunixlib.so.5.0.0 | grep -A1 "<pthread_once>" | grep -q "Personality"; then
  echo "pthread_once has unwind info with a personality routine: OK"
else
  echo "ERROR: pthread_once is not exception-aware"; exit 1
fi
ls -la .libs/libunixlib.so.5.0.0 crt0.o
