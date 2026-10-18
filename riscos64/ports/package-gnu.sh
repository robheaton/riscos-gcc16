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
add "$W/lua-5.4.7/lua,ff8" lua
add "$W/bz-o/bzip2,ff8" bzip2
add "$W/sqlite3,ff8" sqlite3
cp "$W/sed-4.9/COPYING" "$D/Licences/sed-GPL3"; cp "$W/grep-3.11/COPYING" "$D/Licences/grep-GPL3"; cp "$W/gawk-5.3.1/COPYING" "$D/Licences/gawk-GPL3"; cp "$W/make-4.4.1/COPYING" "$D/Licences/make-GPL3" 2>/dev/null || true
cp "$W/lua-5.4.7/doc/readme.html" "$D/Licences/lua-readme" 2>/dev/null || true; cp "$W/bzip2-1.0.8/LICENSE" "$D/Licences/bzip2-LICENSE"
{
  echo "| !Boot file for GNU tools (RISC OS 64)"
  echo
  echo 'Set GNU$Dir <Obey$Dir>'
  echo
  for n in "${progs[@]}"; do
    k=$(( ($(stat -c %s "$D/$n,ff8") + 1023) / 1024 + 192 ))
    echo "Set Alias\$$n WimpSlot -min ${k}k|MRun <GNU\$Dir>.$n %%*0"
  done
  echo 'Set Alias$awk WimpSlot -min 1200k|MRun <GNU$Dir>.gawk %%*0'
  echo 'Set Alias$egrep WimpSlot -min 800k|MRun <GNU$Dir>.grep -E %%*0'
  echo 'Set Alias$fgrep WimpSlot -min 800k|MRun <GNU$Dir>.grep -F %%*0'
} > "$D/!Boot"
printf '| !Run file for GNU tools\n\n/<Obey$Dir>.!Boot\n' > "$D/!Run"
cat > "$D/!Help" <<E
GNU and other tools for RISC OS 64 (AArch64), release $V
===========================================================

sh        a POSIX shell for a system with no fork (ports/sh of the riscos-gcc16 project); runs scripts, pipelines (stage after stage), redirections; needs !CoreUtils for most utilities
sed       GNU sed 4.9        grep  GNU grep 3.11 (egrep, fgrep as aliases)        gawk  GNU awk 5.3.1 (awk as an alias)
make      GNU make 4.4.1 (use  SHELL = ./sh  or the full name of sh; the jobserver is off)
lua       Lua 5.4.7          bzip2  bzip2 1.0.8          sqlite3  SQLite 3.47.2 (no WAL, no extensions)

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
Description: sh, sed, grep, gawk, make, Lua, bzip2 and SQLite for RISC OS 64
 Programs built with GCC 16 and UnixLib 64: ${progs[*]}
Homepage: https://github.com/robheaton/riscos-gcc16
E
printf 'GNU tools\n\nsed, grep, gawk and make: GNU General Public License version 3 or later. Lua: MIT. bzip2: BSD-style (Licences/bzip2-LICENSE). SQLite: public domain. sh: BSD-style.\nSources: the releases named in !Help plus the patches in the riscos-gcc16 repository.\n' > "$O/RiscPkg/Copyright"
( cd "$O" && rm -f "gnutools_${V}_aarch64.zip" && zip -qr "gnutools_${V}_aarch64.zip" Apps RiscPkg )
ls -l "$O/gnutools_${V}_aarch64.zip"
