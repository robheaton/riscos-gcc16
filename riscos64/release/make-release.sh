#!/bin/bash
# make-release.sh PREFIX VERSION OUTDIR [REPO]: packs the installed aarch64-riscos tool chain of target/build-target.sh into OUTDIR/aarch64-riscos-gcc-16.2.0-VERSION-linux-x86_64.tar.xz
# (+ .sha256), after stripping the host programs, with the README, the licences, the documents, the patches (the source offer for GCC and binutils) and a self test script.  REPO = the
# repository root with docs/ and licenses/ (default ~/riscos-gcc16).  The tool chain is relocatable: it finds its files relative to bin/.
set -e
P=$(cd "${1:?PREFIX}" && pwd); V=${2:?VERSION}; O=${3:?OUTDIR}; REPO=${4:-$HOME/riscos-gcc16}
HERE=$(cd "$(dirname "$0")" && pwd); R64=$HERE/..
N=aarch64-riscos-gcc-16.2.0-$V-linux-x86_64
mkdir -p "$O"; O=$(cd "$O" && pwd)
S=$O/$N
[ -e "$S" ] && { echo "$S exists: remove it yourself"; exit 1; }
echo "$(date +%T) copying $P"
cp -a "$P" "$S"
cd "$S"
echo "$(date +%T) removing what is not needed and stripping"
rm -f bin/aarch64-riscos-lto-dump bin/aarch64-riscos-gcov bin/aarch64-riscos-gcov-dump bin/aarch64-riscos-gcov-tool
rm -rf libexec/gcc/aarch64-riscos/16.2.0/plugin libexec/gcc/aarch64-riscos/16.2.0/install-tools libexec/gcc/aarch64-riscos/16.2.0/lto-dump
find . -name '*.la' -path './libexec/*' -delete
for f in $(find bin libexec aarch64-riscos/bin -type f 2>/dev/null); do
  if file -b "$f" | grep -q '^ELF 64-bit LSB.*x86-64'; then strip --strip-unneeded "$f" 2>/dev/null || true; fi
done
# documents, licences, the source offer
mkdir -p share/riscos64/doc share/riscos64/licenses share/riscos64/source
for d in RISCOS64.md MODULES64.md UNIXLIB64.md USERLAND64.md KNOWN-ISSUES.md; do [ -f "$REPO/docs/$d" ] && cp "$REPO/docs/$d" share/riscos64/doc/; done
cp "$R64/sdk/README.md" share/riscos64/doc/SDK.md 2>/dev/null || true
cp "$R64/kit/README.md" share/riscos64/doc/KIT.md 2>/dev/null || true
cp "$REPO"/licenses/* share/riscos64/licenses/ 2>/dev/null || true
cp "$REPO/LICENSES.md" "$REPO/SOURCES.md" share/riscos64/ 2>/dev/null || true
cp -r "$R64/target/patches" "$R64/target/new-files" share/riscos64/source/
cp "$R64/target/build-target.sh" "$R64/target/refresh-sdk.sh" "$R64/target/README.md" share/riscos64/source/ 2>/dev/null || true
cp -r "$R64/sdk" share/riscos64/source/sdk-src && rm -rf share/riscos64/source/sdk-src/bin share/riscos64/source/sdk-src/lib
cp -r "$R64/unixlib" share/riscos64/source/unixlib-src && rm -rf share/riscos64/source/unixlib-src/out
echo "$V" > VERSION
cp "$HERE/README-release.txt" README.txt
sed -i "s/@VERSION@/$V/g" README.txt
cp "$HERE/selftest.sh" selftest.sh && chmod +x selftest.sh
echo "$(date +%T) packing"
cd "$O"
tar -cf - "$N" | xz -T0 -6 > "$N.tar.xz"
( cd "$O" && sha256sum "$N.tar.xz" > "$N.tar.xz.sha256" )
ls -l "$N.tar.xz"; cat "$N.tar.xz.sha256"
