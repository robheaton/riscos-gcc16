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
