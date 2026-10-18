#!/bin/bash
# package-gnu.sh WORKDIR OUTDIR [VERSION]: makes the application !GNU (the tools of ports/build-gnu.sh, build-ports.sh, build-lua.sh and sh/) and a zip, in the way of package-coreutils.sh:
# Apps/Utilities/!GNU with one program per tool, a !Boot that sets GNU$Dir and an Alias$ for each, !Run, !Help, the licences, and RiscPkg/Control.  WORKDIR holds the build trees (sed-build, grep-build,
# gawk-build, make-build, lua-5.4.7, bzip2-1.0.8 + bz-o, sqlite3,ff8 and sh,ff8 as made by the scripts).
set -e
W=$(cd "${1:?work directory}" && pwd); O=${2:?output directory}; V=${3:-1}
HERE=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$O"; O=$(cd "$O" && pwd)
D=$O/Apps/Utilities/!GNU
rm -rf "$O/Apps" "$O/RiscPkg"; mkdir -p "$D/Licences" "$O/RiscPkg"
add () { cp "$1" "$D/$2,ff8"; progs+=("$2"); }
progs=()
add "$W/sh,ff8" sh
add "$W/sed-build/sed/sed" sed
add "$W/grep-build/src/grep" grep
add "$W/gawk-build/gawk" gawk
add "$W/make-build/make" make
add "$W/m4-build/src/m4" m4
add "$W/which-build/which" which
add "$W/bc-build/bc/bc" bc
add "$W/bc-build/dc/dc" dc
add "$W/xxd,ff8" xxd
add "$W/xz-build/src/xz/xz" xz
for n in less lessecho zstd lz4 tree unzip zip jq pcre2grep pcre2test cjpeg djpeg jpegtran pngfix xmlwf; do [ -f "$W/more/$n,ff8" ] && add "$W/more/$n,ff8" $n; done
add "$( [ -f "$W/lua-run/luax,ff8" ] && echo "$W/lua-run/luax,ff8" || echo "$W/lua-5.4.7/lua,ff8" )" lua
add "$( [ -f "$W/bzip2,ff8" ] && echo "$W/bzip2,ff8" || echo "$W/bz-o/bzip2,ff8" )" bzip2
add "$W/sqlite3,ff8" sqlite3
add "$W/diffutils-build/src/diff" diff
add "$W/diffutils-build/src/cmp" cmp
add "$W/diffutils-build/src/diff3" diff3
add "$W/diffutils-build/src/sdiff" sdiff
add "$W/findutils-build/find/find" find
add "$W/findutils-build/xargs/xargs" xargs
add "$W/patch-build/src/patch" patch
add "$W/tar-build/src/tar" tar
add "$W/gzip-build/gzip" gzip
cp "$W/sed-4.9/COPYING" "$D/Licences/sed-GPL3"; cp "$W/m4-1.4.19/COPYING" "$D/Licences/m4-GPL3"; cp "$W/xz-5.4.7/COPYING" "$D/Licences/xz-COPYING" 2>/dev/null || true; cp "$W/which-2.21/COPYING" "$D/Licences/which-GPL3" 2>/dev/null || true; cp "$W/bc-1.07.1/COPYING" "$D/Licences/bc-GPL3" 2>/dev/null || true; cp "$W/grep-3.11/COPYING" "$D/Licences/grep-GPL3"; cp "$W/gawk-5.3.1/COPYING" "$D/Licences/gawk-GPL3"; cp "$W/make-4.4.1/COPYING" "$D/Licences/make-GPL3" 2>/dev/null || true
cp "$W/lua-5.4.7/doc/readme.html" "$D/Licences/lua-readme" 2>/dev/null || true; cp "$W/bzip2-1.0.8/LICENSE" "$D/Licences/bzip2-LICENSE"; for p in diffutils-3.10 findutils-4.10.0 patch-2.7.6 tar-1.35 gzip-1.13; do cp "$W/$p/COPYING" "$D/Licences/${p%%-*}-GPL3" 2>/dev/null || true; done
{
  echo "| !Boot file for GNU tools (RISC OS 64)"
  echo
  echo 'Set GNU$Dir <Obey$Dir>'
  echo 'Set UnixEnv$swap 1'
  echo
  for n in "${progs[@]}"; do
    k=$(( ($(stat -c %s "$D/$n,ff8") + 1023) / 1024 + 192 ))
    echo "Set Alias\$$n WimpSlot -min ${k}k|MRun <GNU\$Dir>.$n %%*0"
  done
  echo 'Set Alias$awk WimpSlot -min 1200k|MRun <GNU$Dir>.gawk %%*0'
  echo 'Set Alias$egrep WimpSlot -min 800k|MRun <GNU$Dir>.grep -E %%*0'
  echo 'Set Alias$fgrep WimpSlot -min 800k|MRun <GNU$Dir>.grep -F %%*0'
  echo 'Set Alias$gunzip WimpSlot -min 700k|MRun <GNU$Dir>.gzip -d %%*0'
  [ -f "$W/more/zstd,ff8" ] && { echo 'Set Alias$unzstd WimpSlot -min 3000k|MRun <GNU$Dir>.zstd -d %%*0'; echo 'Set Alias$zstdcat WimpSlot -min 3000k|MRun <GNU$Dir>.zstd -dc %%*0'; echo 'Set Alias$unlz4 WimpSlot -min 1500k|MRun <GNU$Dir>.lz4 -d %%*0'; echo 'Set Alias$lz4cat WimpSlot -min 1500k|MRun <GNU$Dir>.lz4 -dc %%*0'; }
  echo 'Set Alias$unxz WimpSlot -min 1500k|MRun <GNU$Dir>.xz -d %%*0'
  echo 'Set Alias$xzcat WimpSlot -min 1500k|MRun <GNU$Dir>.xz -dc %%*0'
  echo 'Set Alias$zcat WimpSlot -min 700k|MRun <GNU$Dir>.gzip -dc %%*0'
} > "$D/!Boot"
printf '| !Run file for GNU tools\n\n/<Obey$Dir>.!Boot\n' > "$D/!Run"
cat > "$D/!Help" <<E
GNU and other tools for RISC OS 64 (AArch64), release $V
===========================================================

sh        a POSIX shell for a system with no fork (ports/sh of the riscos-gcc16 project); runs scripts, pipelines (stage after stage), redirections; needs !CoreUtils for most utilities
sed       GNU sed 4.9        grep  GNU grep 3.11 (egrep, fgrep as aliases)        gawk  GNU awk 5.3.1 (awk as an alias)
less lessecho (pager over the built-in termcap entry for the VDU drivers)  zstd lz4 tree unzip zip jq pcre2grep  (when built by build-more.sh: Zstandard 1.5.6, LZ4 1.10.0, tree 2.1.3, UnZip 6.00, Zip 3.0, jq 1.7.1, PCRE2 10.44; aliases unzstd zstdcat unlz4 lz4cat)
which     GNU which 2.21        bc dc  GNU bc/dc 1.07.1        xz  XZ Utils 5.4.7 (unxz, xzcat as aliases)   xxd  hex dump (from Vim 9.1.0000; Vim licence / GPL-2)
m4        GNU m4 1.4.19 (esyscmd and syscmd run the command with popen / system)
make      GNU make 4.4.1 (use  SHELL = ./sh  or the full name of sh; the jobserver is off)
diff cmp diff3 sdiff (diffutils 3.10)   find xargs (findutils 4.10.0)   patch 2.7.6   tar 1.35   gzip 1.13 (gunzip, zcat as aliases)
lua       Lua 5.4.7          bzip2  bzip2 1.0.8          sqlite3  SQLite 3.47.2 (no WAL, no extensions)

Documentation: docs/USERLAND64.md in https://github.com/robheaton/riscos-gcc16 (file names, the shell, limits, porting).

Run !GNU (or its !Boot) once: it sets GNU\$Dir and an alias for each program. The shell finds the programs through GNU\$Dir and CoreUtils\$Dir.
Built with the GCC 16.2.0 tool chain for RISC OS 64 and UnixLib 64: https://github.com/robheaton/riscos-gcc16 (ports/, riscos64/). The programs are the unmodified releases except for the patches in ports/patches
(make: \$(shell) uses popen; gawk: pipes and system () use popen / system; coreutils is in its own package). Licences: Licences/ (sed, gawk, make GPL-3+; grep GPL-3+; Lua MIT; bzip2 BSD-style; SQLite public domain; sh BSD-style).
Tested on RISC OS Pyromaniac, not yet on a real RISC OS 64 machine. Not possible: a pipeline whose first stage never ends, ln -s.
E
cat > "$O/RiscPkg/Control" <<E
Package: GNU
Version: $V
Section: Utilities
Priority: Optional
Environment: aarch64
Licence: GPL-3+
Maintainer: RISC OS 64 GCC port <https://github.com/robheaton/riscos-gcc16>
Standards-Version: 0.4.0
Components: Apps.Utilities.!GNU (Movable LookAt)
Description: sh, sed, grep, gawk, make, diff, find, patch, tar, gzip, Lua, bzip2 and SQLite for RISC OS 64
 Programs built with GCC 16 and UnixLib 64: ${progs[*]}
Homepage: https://github.com/robheaton/riscos-gcc16
E
printf 'GNU tools\n\nsed, grep, gawk and make: GNU General Public License version 3 or later. Lua: MIT. bzip2: BSD-style (Licences/bzip2-LICENSE). SQLite: public domain. sh: BSD-style.\nSources: the releases named in !Help plus the patches in the riscos-gcc16 repository.\n' > "$O/RiscPkg/Copyright"
( cd "$O" && rm -f "gnutools_${V}_aarch64.zip" && zip -qr "gnutools_${V}_aarch64.zip" Apps RiscPkg )
ls -l "$O/gnutools_${V}_aarch64.zip"
