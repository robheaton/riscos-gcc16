#!/bin/bash
# cross-smoke.sh TOOLCHAIN_DIR [REFERENCE_TOOLCHAIN_DIR]
# Compiles and links small C, C++, Fortran, LTO and shared-library programs with the cross toolchain in TOOLCHAIN_DIR (the unpacked tarball), checks that the compiler
# finds everything inside that directory, and that the objects are ELF 32-bit ARM EABI5 for the shared UnixLib.  With a second directory it also compares the
# results byte for byte with that toolchain's (to prove a relocated copy gives the same code).  The programs cannot be run here: copy them to RISC OS to run them.
set -u
TC=$(readlink -f "${1:?usage: cross-smoke.sh TOOLCHAIN_DIR [REFERENCE_TOOLCHAIN_DIR]}")
REF=${2:+$(readlink -f "$2")}
HERE=$(cd "$(dirname "$0")" && pwd)
T=arm-riscos-gnueabihf
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
fails=0
ok()   { echo "  ok    $1"; }
bad()  { echo "  FAIL  $1"; fails=$((fails+1)); }
chk()  { if eval "$2" > "$W/chk.log" 2>&1; then ok "$1"; else bad "$1"; sed 's/^/        /' "$W/chk.log" | head -5; fi; }
export PATH="$TC/bin:$PATH"
cd "$W"
echo "toolchain: $TC"
chk "gcc runs and reports GCC 16.2.0"                          "$T-gcc --version | head -1 | grep -q '16\.2\.0'"
chk "g++ runs"                                                 "$T-g++ --version | head -1 | grep -q '16\.2\.0'"
chk "gfortran runs"                                            "$T-gfortran --version | head -1 | grep -q '16\.2\.0'"
chk "ld is binutils 2.45.1"                                    "$T-ld --version | head -1 | grep -q '2\.45\.1'"
for what in cc1 cc1plus f951 lto1 collect2 lto-wrapper; do
  chk "$what is found inside the toolchain directory"         "p=\$($T-gcc -print-prog-name=$what); case \$p in $TC/*) true;; *) echo \"\$p\"; false;; esac"
done
chk "libunixlib.so is found inside the toolchain directory"   "p=\$($T-gcc -print-file-name=libunixlib.so); case \$p in $TC/*) true;; *) echo \"\$p\"; false;; esac"
chk "the linker plugin is found inside the toolchain directory" "p=\$($T-gcc -print-prog-name=liblto_plugin.so); case \$p in $TC/*) true;; *) echo \"\$p\"; false;; esac"
chk "-fstack-clash-protection is the default (probe sequence in a function with a frame)" \
    "printf 'extern void use(char *);\nvoid f(void) { char b[64]; use(b); }\n' | $T-gcc -O2 -S -x c -o - - | grep -q 'mov.*#4096'"
echo "C"
chk "C: gcc -O2 hello.c"            "$T-gcc -O2 -o hello_c $HERE/hello.c"
chk "C: ELF 32-bit ARM EABI5, needs libunixlib.so.5" "file hello_c | grep -q 'ELF 32-bit LSB.*ARM, EABI5' && $T-readelf -d hello_c | grep -q 'NEEDED.*libunixlib.so.5'"
chk "C: -std=c23 (the default is gnu23)" "$T-gcc -O2 -std=c23 -o /dev/null -x c - <<< 'constexpr int k = 3; int main(void) { return k - 3; }'"
echo "C++"
chk "C++: g++ -O2 hello.cc (exceptions, threads, iostream)" "$T-g++ -O2 -o hello_cc $HERE/hello.cc"
chk "C++: needs libstdc++.so.6 (the SharedLibs-C++ package)"  "$T-readelf -d hello_cc | grep -q 'NEEDED.*libstdc++.so.6'"
chk "C++: -std=c++23 <print>, <expected>" "$T-g++ -O2 -std=c++23 -o /dev/null -x c++ - <<< '#include <print>
#include <expected>
int main() { std::expected<int,int> e = 1; std::println(\"{}\", e.value()); }'"
echo "Fortran"
chk "Fortran: gfortran -O2 hello.f90" "$T-gfortran -O2 -o hello_f $HERE/hello.f90"
chk "Fortran: needs libgfortran.so.5 (the SharedLibs-Fortran package)" "$T-readelf -d hello_f | grep -q 'NEEDED.*libgfortran.so.5'"
echo "LTO and shared libraries"
chk "LTO: gcc -O2 -flto of two files (linker plugin)" "$T-gcc -O2 -flto -o lto $HERE/lto_a.c $HERE/lto_b.c"
chk "LTO: the call of twice () was inlined across the files" "! $T-objdump -d lto | grep -q 'bl.*<twice>'"
chk "shared library: -fPIC -shared, then linked against" "$T-gcc -O2 -fPIC -shared -o libfoo.so $HERE/foo.c && $T-gcc -O2 -o usefoo $HERE/usefoo.c -L. -lfoo && $T-readelf -d usefoo | grep -q 'NEEDED.*libfoo.so'"
echo "Coverage and profile-guided optimisation"
chk "coverage: gcc --coverage compiles and links (libgcov has the exit function)" "$T-gcc -O0 --coverage -c -o cov.o $HERE/hello.c && $T-gcc --coverage -o hello_cov cov.o && $T-nm hello_cov | grep -q ' __gcov_exit'"
chk "coverage: the compiler wrote the notes file" "test -f hello.gcno || test -f cov.gcno"
chk "profile-guided optimisation: -fprofile-generate links" "$T-gcc -O2 -fprofile-generate -o hello_pgo $HERE/hello.c && $T-nm hello_pgo | grep -q ' __gcov_init'"
chk "gcov runs" "$T-gcov --version | head -1 | grep -q '16\.2\.0'"
echo "Profiling (gprof) and throwback"
chk "gprof: $T-gprof runs"                                      "$T-gprof --version | head -1 | grep -q 'GNU gprof'"
chk "-pg: the profiling call is push {lr} ; bl __gnu_mcount_nc"  "$T-gcc -O1 -pg -S -o - $HERE/hello.c | grep -q 'bl.*__gnu_mcount_nc'"
chk "-pg: the driver links gcrt0.o (and crt0.o without -pg)"     "$T-gcc -pg -### $HERE/hello.c 2>&1 | grep -q 'gcrt0.o' && ! $T-gcc -### $HERE/hello.c 2>&1 | grep -q 'gcrt0.o'"
# a -pg program needs the libunixlib.so of 16.2.0-13 or later, which exports __gnu_mcount_nc (step 5 of docs/BUILDING.md puts it in the sysroot): with the GCCSDK 10.2.0 one the link is skipped, not failed
if $T-nm -D --defined-only "$($T-gcc -print-file-name=libunixlib.so)" | grep -q ' __gnu_mcount_nc$'; then
  chk "-pg: a program links, with __gnu_mcount_nc from libunixlib.so" "$T-gcc -O1 -pg -o hello_pg $HERE/hello.c && $T-nm -D hello_pg | grep -q 'U __gnu_mcount_nc'"
else
  echo "  skip  -pg: this toolchain's libunixlib.so is still the GCCSDK 10.2.0 one: no link check (install-unixlib-sysroot.sh puts the fixed one there)"
fi
chk "-mthrowback: the driver gives --throwback to as and to ld"  "test \$($T-gcc -mthrowback -### $HERE/hello.c 2>&1 | grep -c -e '--throwback') -ge 2"
chk "without -mthrowback the driver adds no --throwback"         "! $T-gcc -### $HERE/hello.c 2>&1 | grep -q -e '--throwback'"
echo "Modules (gcc -mmodule, cmunge: modkit)"
if [ -e "$TC/arm-riscos-gnueabihf/lib/libmodkit.a" ]; then
  chk "-mmodule: ARMv6, soft float, ARM state, no pic, freestanding"  "$T-gcc -mmodule -O2 -### -c $HERE/hello.c 2>&1 | grep cc1 | grep -q -e '-march=armv6' && $T-gcc -mmodule -O2 -### -c $HERE/hello.c 2>&1 | grep cc1 | grep -q 'mfloat-abi=soft' && $T-gcc -mmodule -O2 -### -c $HERE/hello.c 2>&1 | grep cc1 | grep -q -e '-ffreestanding'"
  chk "-mmodule: __TARGET_MODULE__ is defined and __TARGET_UNIXLIB__ is not" "$T-gcc -mmodule -dM -E -x c /dev/null | grep -q __TARGET_MODULE__ && ! $T-gcc -mmodule -dM -E -x c /dev/null | grep -q __TARGET_UNIXLIB__ && $T-gcc -dM -E -x c /dev/null | grep -q __TARGET_UNIXLIB__"
  chk "-mmodule: the headers of modkit come first, inside the toolchain directory, and the UnixLib headers are not searched" "$T-gcc -mmodule -E -v -x c /dev/null 2>&1 | sed -n '/search starts here/,/End of search/p' | sed 's#/bin/\\.\\./#/#' | grep -q \"$TC/lib/gcc/.*/include-modkit\" && ! $T-gcc -mmodule -E -v -x c /dev/null 2>&1 | grep -q 'arm-riscos-gnueabihf/include'"
  chk "-mmodule: no start files, the linker script and libmodkit.a are found inside the toolchain directory" "l=\$($T-gcc -mmodule -### -o x.elf mh.o 2>&1 | grep collect2); echo \"\$l\" | grep -q -e \"-T $TC/\" && echo \"\$l\" | grep -q \"$TC/.*libmodkit.a\" && ! echo \"\$l\" | grep -q 'crt0.o\\|crti.o\\|crtbegin'"
  chk "cmunge: modhello.cmhg gives the header and the object (CMunge's command line)" "cmunge -tgcc -32bit -p -d modhello.h -o modhello_hdr.o $HERE/modhello.cmhg && test -s modhello.h && test -s modhello_hdr.o"
  chk "-mmodule: compile, link (one command: the driver runs modreloc): the output is the flat module image, with its name in the header and a table of address words" "$T-gcc -mmodule -O2 -Wall -I. -c $HERE/modhello.c -o modhello.o && $T-gcc -mmodule -o modhello,ffa modhello.o modhello_hdr.o && ! head -c 4 modhello,ffa | grep -q ELF && grep -q ModHello modhello,ffa"
  chk "-mmodule: an output named *.elf stays an ELF file (for a debugger or a simulation), and modreloc makes the same module image of it" "$T-gcc -mmodule -o modhello.elf modhello.o modhello_hdr.o && head -c 4 modhello.elf | grep -q ELF && $T-modreloc -q modhello.elf modhello2,ffa && cmp modhello,ffa modhello2,ffa"
  chk "-mmodule -r (a partial link) is not turned into a module" "$T-gcc -mmodule -r -o modpart.o modhello.o modhello_hdr.o && head -c 4 modpart.o | grep -q ELF"
  chk "the module has no call into a C library (every symbol is the module's or modkit's)" "test -f modhello.elf && ! $T-nm -u modhello.elf | grep -q ."
  chk "cmunge accepts the CMHG options of 16.2.0-18 (swi-decoding-code:, a function per SWI, private-word:, carry-capable:, error-capable:, handler:, no-handler:) and the header defines VENEER_SETCARRY and VECTOR_ERROR" "cmunge -tgcc -32bit -p -d modopts.h -o modopts_hdr.o $HERE/modopts.cmhg && test -s modopts_hdr.o && grep -q VENEER_SETCARRY modopts.h && grep -q 'VECTOR_ERROR' modopts.h"
  chk "cmunge -zbase and -apcs 3/32 are accepted (Image__RO_Base is declared in the header)" "cmunge -tgcc -32bit -zbase -apcs 3/32 -p -d modsock.h -o modsock_hdr.o $HERE/modsock.cmhg && grep -q 'Image__RO_Base' modsock.h && test -s modsock_hdr.o"
  chk "-mmodule: sockets, netdb, select, ioctl and __modlib_stack_left compile (the headers of the kit: sys/socket.h, netinet/in.h, arpa/inet.h, netdb.h) and link with nothing undefined" "$T-gcc -mmodule -O2 -Wall -I. -c $HERE/modsock.c -o modsock.o && $T-gcc -mmodule -o modsock.elf modsock.o modsock_hdr.o && ! $T-nm -u modsock.elf | grep -q ."
  chk "libgcc-mod.a and libstdcxx-mod.a are in the tool chain (install-modkit.sh makes them)" "test -s $TC/arm-riscos-gnueabihf/lib/libgcc-mod.a && test -s $TC/arm-riscos-gnueabihf/lib/libstdcxx-mod.a"
  if [ -e "$TC/arm-riscos-gnueabihf/lib/libgcc-mod.a" ]; then
    chk "-mmodule: libmodkit.a names libgcc-mod.a (the members of libgcc.a without VFP or ARMv7 code), not -lgcc" "grep -q libgcc-mod.a $TC/arm-riscos-gnueabihf/lib/libmodkit.a && ! grep -q -e '-lgcc' $TC/arm-riscos-gnueabihf/lib/libmodkit.a && test -s $TC/arm-riscos-gnueabihf/lib/libgcc-mod.a"
    chk "-mmodule: a double converted to a long long, popcount, powi, printf of a double and sqrt link, and not one member comes from libgcc.a (the VFP ones)" "$T-gcc -mmodule -O2 -Wall -c $HERE/modfp.c -o modfp.o && $T-gcc -mmodule -o modfp.elf modfp.o -Wl,-Map=modfp.map && ! grep -q 'libgcc\\.a(' modfp.map && grep -q 'libgcc-mod\\.a(' modfp.map && grep -q 'libmodkit-core\\.a(gccrt\\.o)' modfp.map"
  fi
  if [ -e "$TC/arm-riscos-gnueabihf/lib/libstdcxx-mod.a" ]; then
    CXXDIRS=$(echo | $T-g++ -x c++ -E -v - 2>&1 | sed -n '/^#include <...>/,/^End of search/p' | grep '/c++/' | sed 's/^ *//')
    CXXI=""; for d in $CXXDIRS; do CXXI="$CXXI -isystem $d"; done
    MODCXXFLAGS="-mmodule -U__STDC_HOSTED__ -D__STDC_HOSTED__=1 -isystem $TC/share/riscos-modkit/include-cxx $CXXI -isystem $TC/share/riscos-modkit/include -isystem $($T-gcc -print-file-name=include-modkit) -isystem $($T-gcc -print-file-name=include) -std=gnu++17 -O2 -fno-exceptions -fno-rtti -fno-threadsafe-statics"
    chk "C++ modules: libmodkit.a names libstdcxx-mod.a (the members of libstdc++.a that are plain ARMv6 code and not position independent), and share/riscos-modkit has include-cxx and module.mk" "grep -q libstdcxx-mod.a $TC/arm-riscos-gnueabihf/lib/libmodkit.a && test -s $TC/arm-riscos-gnueabihf/lib/libstdcxx-mod.a && test -f $TC/share/riscos-modkit/include-cxx/cmath && test -f $TC/share/riscos-modkit/module.mk"
    chk "C++ modules: cmunge accepts module-is-c-plus-plus:" "cmunge -tgcc -32bit -p -d modcxx.h -o modcxx_hdr.o $HERE/modcxx.cmhg && test -s modcxx_hdr.o"
    chk "C++ modules: std::string, std::map, std::unordered_map, unique_ptr, virtual functions, new and delete compile and link as a module (the driver runs modreloc, which refuses a global offset table); no libstdc++ and no libgcc VFP member, no undefined symbol" "$T-g++ $MODCXXFLAGS -I. -c $HERE/modcxx.cc -o modcxx.o && $T-gcc -mmodule -o modcxx.elf modcxx.o modcxx_hdr.o -Wl,-Map=modcxx.map && $T-gcc -mmodule -o modcxx,ffa modcxx.o modcxx_hdr.o && ! head -c 4 modcxx,ffa | grep -q ELF && ! grep -q 'libstdc++\\.a(\\|libgcc\\.a(' modcxx.map && ! $T-nm -u modcxx.elf | grep -q ."
  fi
else
  echo "  skip  modules: this toolchain has no modkit (install-modkit.sh puts it in)"
fi
echo "Tuning"
chk "cortex-a72 tuning flags are accepted" "$T-gcc -O2 -mcpu=cortex-a72 -mfpu=neon-fp-armv8 -mfloat-abi=hard -o /dev/null -x c - <<< 'int main(void){return 0;}'"
if [ -n "${REF:-}" ]; then
  echo "comparison with $REF (same objects, same programs)"
  RT=$REF/bin/$T
  for pair in "gcc:hello.c:hello_c" "g++:hello.cc:hello_cc" "gfortran:hello.f90:hello_f"; do
    IFS=: read -r drv src out <<< "$pair"
    $RT-$drv -O2 -g0 -o ref_$out $HERE/$src 2>/dev/null; $T-$drv -O2 -g0 -o new_$out $HERE/$src 2>/dev/null
    chk "$src: the program is byte for byte the one of the reference toolchain" "cmp ref_$out new_$out"
  done
  $RT-gcc -O2 -flto -g0 -o ref_lto $HERE/lto_a.c $HERE/lto_b.c 2>/dev/null; $T-gcc -O2 -flto -g0 -o new_lto $HERE/lto_a.c $HERE/lto_b.c 2>/dev/null
  chk "LTO program: byte for byte the one of the reference toolchain" "cmp ref_lto new_lto"
fi
echo
[ $fails = 0 ] && { echo "cross-smoke: all checks passed"; exit 0; } || { echo "cross-smoke: $fails check(s) FAILED"; exit 1; }
