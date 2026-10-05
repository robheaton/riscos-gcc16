#!/bin/bash
# package-cross-toolchain.sh SRC_PREFIX OUT_DIR VERSION [README_FILE]
#
# Makes OUT_DIR/riscos-gcc16-cross-VERSION-x86_64-linux.tar.xz from an INSTALLED cross toolchain (the PREFIX of configure-gcc16-full.sh, with the binutils 2.45.1 port
# behind the binutils entries): a relocatable tree that can be unpacked anywhere and used by putting its bin/ on PATH.
#   * the binutils entries of the install tree are symbolic links into the binutils install: they are replaced by the real files (nothing in the tarball points outside it)
#   * host programs (x86-64) are stripped; the target libraries and objects (ARM) are left as built
#   * libtool .la files (absolute build paths, no use for a cross compiler) are removed
#   * the licence texts and a README are added
# GCC finds its files relative to the place of the driver, so the tree needs no further set-up.  tests/cross-smoke.sh checks a relocated copy.
set -eu
SRC=$(readlink -f "${1:?usage: package-cross-toolchain.sh SRC_PREFIX OUT_DIR VERSION [README_FILE]}")
OUT=$(mkdir -p "${2:?}" && readlink -f "$2")
VER=${3:?version, e.g. 16.2.0-11}
README=${4:-}
T=arm-riscos-gnueabihf
NAME=riscos-gcc16-cross-$VER-x86_64-linux
STAGE=$OUT/stage/$NAME
rm -rf "$OUT/stage"; mkdir -p "$STAGE"
echo "copying $SRC ..."
rsync -a "$SRC"/ "$STAGE"/

echo "replacing links that leave the tree by the files they point to ..."
n=0
while IFS= read -r l; do
  t=$(readlink -f "$l")
  case "$t" in "$STAGE"/*) continue;; esac
  [ -f "$t" ] || { echo "ERROR: $l -> $t is not a file"; exit 1; }
  rm "$l"; cp -p "$t" "$l"; n=$((n+1))
done < <(find "$STAGE" -type l)
echo "  $n links replaced"
if find "$STAGE" -type l | while read -r l; do t=$(readlink -f "$l"); case "$t" in "$STAGE"/*) ;; *) echo "$l";; esac; done | grep -q .; then
  echo "ERROR: links that leave the tree remain"; exit 1
fi

echo "removing libtool .la files, stripping host programs ..."
find "$STAGE" -name '*.la' -delete
s=0
while IFS= read -r -d '' f; do
  if [ "$(head -c 4 "$f" | od -An -c | tr -d ' ')" = '177ELF' ] && readelf -h "$f" 2>/dev/null | grep -q 'Advanced Micro Devices X86-64'; then
    strip --strip-unneeded "$f" 2>/dev/null && s=$((s+1)) || true
  fi
done < <(find "$STAGE" -type f \( -perm -u+x -o -name '*.so*' \) -print0)
echo "  $s host programs stripped"

HERE=$(cd "$(dirname "$0")" && pwd)
LIC=$HERE/../../../licenses                      # licenses/ at the top of the repository
[ -d "$LIC" ] || { echo "ERROR: $LIC not found"; exit 1; }
mkdir -p "$STAGE/licenses" && cp "$LIC"/*.txt "$STAGE/licenses/"
[ -n "$README" ] && cp "$README" "$STAGE/README.txt"
{
  echo "riscos-gcc16 cross toolchain $VER"
  echo "built:  $(date -u '+%Y-%m-%d %H:%M UTC') on $(uname -sr), $(ldd --version | head -1)"
  echo "gcc:    $("$STAGE/bin/$T-gcc" --version | head -1)"
  echo "ld:     $("$STAGE/bin/$T-ld" --version | head -1)"
  echo "files:  $(find "$STAGE" -type f | wc -l)   size: $(du -sh "$STAGE" | cut -f1)"
} > "$STAGE/BUILD-INFO.txt"
cat "$STAGE/BUILD-INFO.txt"

echo "writing $NAME.tar.xz ..."
tar -C "$OUT/stage" --sort=name --owner=0 --group=0 --numeric-owner -cf - "$NAME" | xz -T0 -9 > "$OUT/$NAME.tar.xz"
( cd "$OUT" && sha256sum "$NAME.tar.xz" > "$NAME.tar.xz.sha256" )
ls -la "$OUT/$NAME.tar.xz"; cat "$OUT/$NAME.tar.xz.sha256"
