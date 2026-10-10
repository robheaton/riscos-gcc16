# Floating point in modules: work notes (started 2026-10-09, after 16.2.0-16)

Design notes of the floating point and C++ work for modules (released in 16.2.0-17). These are the author's working notes, kept because they record why things are as they are; the documentation for users is docs/MODULES.md.

## Finding 1 (a fault of 16.2.0-14 .. -16, found 2026-10-09): libgcc members that are VFP / ARMv7 code

`libmodkit.a` was `GROUP ( libmodkit-core.a -lgcc )`. The libgcc.a of the tool chain has ONE multilib: ARMv7-A, VFPv3, hard float.
Its assembler members (ieee754-df.S etc.: `_arm_*.o`) are plain ARM code and fine. Its members that GCC compiled from C are not:
`__fixdfdi` family (double/float to long long: `vmov d16, r0, r1`, `vcmpe.f64`, `vmrs`), `__popcountsi2/di2`, `__paritysi2/di2` (movw/movt),
`__ctzdi2`, `__ffssi2/di2` (rbit), `__powidf2/powisf2`, complex `__muldc3` ..., `_BitInt` helpers, fp16, linux-atomic, sfp-exceptions, Thumb-1 helpers.
The linker links them without a word. Consequences: undefined instruction on an ARMv6 CPU (Pi 1: no movw; d16 needs VFPv3-D32), and in SVC mode
the module uses the VFP registers of the task that called it (RISC OS does not save VFP state for SVC code).
`docs/MODULES.md` said "no instruction of the VFP is used": wrong for those members. Reproduction: /tmp.../fp3.c `(long long) vd` + `__builtin_popcount (vu)`:
released kit = interpreter fault `instruction not modelled: ec410b30` (vmov d16,r0,r1); new kit = `ll=123456789 popcount=16`.

Fix (done in the work area, tested on the interpreter):
* `modkit/bin/mklibgcc.sh AR OBJDUMP libgcc.a OUT.a [REPORT]` makes `libgcc-mod.a` = libgcc.a without the members that have a VFP mnemonic (`v*`),
  movw movt rbit ubfx sbfx bfi bfc mls udiv sdiv dmb dsb isb, Thumb-only helpers, linux-*, sfp-exceptions, and without the fixed-point members;
  debug info stripped: 125 members, ~100 KB (from 1762 members, 9.8 MB). Report in `libgcc-mod.txt` next to it.
* `install-modkit.sh` runs it and writes `libmodkit.a` = `GROUP ( libmodkit-core.a libgcc-mod.a )` (no fallback to the full libgcc: a missing
  routine is now an undefined reference).
* `modkit/lib/gccrt.c` (1348 bytes): `__fixdfdi __fixunsdfdi __fixsfdi __fixunssfdi` (+ `__aeabi_d2lz/d2ulz/f2lz/f2ulz`, saturating, NaN = 0),
  `__popcountsi2/di2 __paritysi2/di2 __ctzdi2 __ffssi2/di2 __powidf2 __powisf2`. Integer code; powi uses the soft float routines.
* Tests: `modkit/tests/libtest/libtest-fp.c` (included by libtest.c): sections `fparith` (62,500 results), `fpconv` (40,000), `gccrt` (32,500):
  glibc = host-lib (sanitizers) = ARM interpreter, all identical (SCALE 1; the ARM run is now 260 s: 183 M steps).

## Design decisions for printf / scanf / strtod (not written yet)
* `%f %F %e %E %g %G %a %A`, flags `- + space # 0`, width, precision, `l`/`L` ignored (long double = double on this target). Exact (round-half-even on
  the exact binary value, as glibc): bignum digit generation, integer code only (no FP operations: bits of the double come from va_arg as 2 words).
  Bignum of 36 limbs (1152 bits) x2 on the stack = 288 bytes; two passes (first decides the rounding and the trailing 9s, second prints).
* `strtod strtof strtold atof`, scanf `%f %e %g %a` (+ `l`/`L`): exact, correctly rounded, hex floats, inf/nan/nan(chars), no FP operations (builds the bits);
  digits kept: up to 800 significant digits + a sticky digit; bignum capacity from the stack for the usual sizes, malloc beyond.
  scanf collects the characters as glibc does (valid prefix of the grammar), then converts; compare with glibc on random strings (`%n` for consumed).
* printf's FP code is linked whenever printf is (about 3 KB): no -u option; revisit if it turns out to be much bigger.
* math.h (fabs floor ceil trunc round fmod modf frexp ldexp sqrt copysign fmin fmax, then the transcendental ones from fdlibm-style sources if the licence allows)
  after printf/scanf. libm.a of UnixLib is hard float: cannot be used in a module.
* Hardware: pack 39 = libtest-hw with the new sections (expected hashes from the glibc run) + a small FP module; the user runs it on the Pi.


## State 2026-10-09 evening (after the design above was implemented)
Done and tested (host: oracle glibc vs the sanitizer build of the kit's sources; ARM: the A32 interpreter):
* lib/fpbig.[ch], lib/fpfmt.c (printf %f %e %g %a), lib/strtod.c (strtod strtof strtold atof + __modlib_strtofp), scanf.c (%f %e %g %a, glibc's collection grammar), lib/gccrt.c, bin/mklibgcc.sh.
* math.h (include/math.h): lib/fd_*.c = Sun's fdlibm 5.3 from GNU Classpath (33 files, kept as they are; fixes: sqrt's shift by 32 (a latent fdlibm bug), the pointer-cast idiom for the low word in
  cosh/sinh/pow, wrapv via fdlibm.h) + lib/m_*.c = my wrappers (errno as glibc: probed against glibc 2.43) and the exact functions (trunc round nearbyint lround... frexp modf logb nextafter fmin fmax fdim nan)
  + the float functions (work in double, round once). Sections fpmathx (exact functions + errno vs glibc: equal at SCALE 40) and fpmath (inexact functions: host build == ARM build bit for bit, and the
  error vs long double on the host: limits in libtest-fp.c, math.h comment). Library 104 KB -> 221 KB archive; RomCmp module 67,136 -> 72,228 bytes (printf's FP code is always linked).
* glibc facts found by probing: fmin/fmax return the SECOND argument for +0/-0 and a NaN for a signaling NaN; logb(0) sets no errno; ilogb(0, nan, inf) sets EDOM; pow(0,-1) sets no errno; a subnormal
  result of exp/ldexp sets no errno (only 0 does); nextafter sets ERANGE for a subnormal result (not for nextafter(0,1)).
* tools/scan-os-modules.py with the new library: library calls covered 66 of 66 C modules (MakePSFont needed math.h); CMHG covered for 62 of 66.
* test infrastructure: LT_ONLY=a,b in libtest (env var / system variable), BUILD=dir for run-arm.py, run-host.sh links -lm and builds fd_*.c without the shift/overflow sanitizers.
Size trick to document: a module that does not want %f in printf can define its own `int __modlib_fmtdouble (...)` (returns 0) and the 5 KB are not linked; same for `__modlib_strtofp` (scanf, strtod).
TODO still: hardware pack 40 with fpmathx/fpmath (hostlib hash as expected for fpmath: make-pack must read it from hostlib.out); docs (MODULES.md Floating point row etc.: see the list above);
make-native-tree.sh must copy libgcc-mod.a; ask the user before commit/release.


## State 2026-10-10 (after the Pi run of module40, the two reviews, and the C++ work) -- read this first
* Pi run of module40 (2026-10-09 20:29; the result file is kept outside the repository): everything passed; the one FAIL was the run script (no `Set Test$Var`). Fixed in pack/make-pack41.py.
* Reviews applied (printf: INT_MAX corners in fpfmt.c/printf.c; strtod/scanf; maths: pow(1,y), pow(0,y<0) ERANGE [my earlier "glibc fact" pow(0,-1) = no errno was a constant-folding artefact], m_fres, atan2/asinh/atanh
  errno, nan() errno; mklibgcc.sh robustness). Documented but not copied: glibc's irregular ERANGE for some subnormal results of expf/exp2f/powf. Tests: new sections fpmathsp (class + errno of every inexact function on
  special values, vs glibc), `tmpfile` (library only), fpprintf (EOVERFLOW cases; `#g` avoided because glibc is wrong when rounding carries into the next power of ten; a C11 check of ours instead), strtod compares NaN bits.
* C++ in modules works (see docs/MODULES.md "C++ in modules"). LESSON: instruction filters are not enough. The toolchain's -fPIC code (libstdc++.a members, 44 of them incl. string-inst.o) uses the RISC OS shared library
  model: `mov r3,#0x8000; ldr r3,[r3,#idx]; ldr r3,[r3]` with `R_ARM_NONE __GOTT_INDEX__` / R_ARM_GOT*; it passes the instruction filter and then reads address 0x8038 in a module. mklibgcc.sh now drops members with
  R_ARM_GOT*/TARGET2/__GOTT_INDEX__ relocations (libstdcxx-mod.a = 58 of 199 members: tree.o, list.o ...); std::string etc. are instantiated in the module's own code (include-cxx/bits/c++config.h sets
  _GLIBCXX_EXTERN_TEMPLATE 0). The first design (GOT in the image relocated by modreloc) was WRONG and is reverted; modreloc only gives a better message for an orphan .got. The interpreter caught it (a fault at 0x8038)
  only because cxxstd.cc grew: the earlier "passes" were luck. Keep tests/cxx/test-cxx.py + sim-cxxmod.py in every release run.
* Added for C++: lib/cxxrt.c (init/fini/atexit/guards/throw stubs), cxxnew/cxxnewa/cxxdel/cxxdela.c (replaceable operator new/delete), cxxeh.c (abort stubs for the EH entry points), cxxhash.c (unordered_* rehash policy and
  _Hash_bytes, own primes), cxxsp.c (make_shared _S_eq), tmpfile.c, time.c (difftime, clock_gettime, timespec_get), wchar.h/wctype.h (types only), errno.h (all UnixLib names), assert.h fix, include-cxx/{cmath,math.h,
  bits/gthr-default.h,bits/c++config.h}, module.mk (CXX rules, include-cxx first), tools/a32.py (ldrex/strex/clrex).
* Not supported (documented): exceptions, RTTI, thread_local, iostreams, <mutex>/<thread>, <stdexcept> classes, <random> (needs std::lgamma; no permissive source here: Classpath's fdlibm has no lgamma/erf, UnixLib's is glibc's),
  wide characters.
