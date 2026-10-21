#!/bin/bash
# package-coreutils.sh BUILDDIR SRCDIR OUTDIR [VERSION]: makes the RISC OS application !CoreUtils from a build of GNU coreutils (ports/build-gnu.sh style, no --enable-single-binary) and a zip of it
# the way the 32-bit package (coreutils_8.30-2_arm.zip) is made: Apps/Utilities/!CoreUtils with one program per utility, a !Boot that sets CoreUtils$Dir and one  Alias$NAME  per program (so that "sort" works on
# the command line, in Obey files and for system () and posix_spawn), !Run (runs !Boot), !Help, the licence files, and RiscPkg/Control.  The files have ,ff8 on the end (type &FF8, AIF64), as in a zip made on Linux.
set -e
B=$(cd "${1:?build directory of coreutils}" && pwd); S=$(cd "${2:?coreutils source directory}" && pwd); O=${3:?output directory}; V=${4:-9.5-1}
mkdir -p "$O"; O=$(cd "$O" && pwd)
D=$O/Apps/Utilities/!CoreUtils
rm -rf "$O/Apps" "$O/RiscPkg"; mkdir -p "$D" "$O/RiscPkg"
progs=()
for f in "$B"/src/*; do
  n=$(basename "$f")
  [ -f "$f" ] && [ -x "$f" ] || continue
  case "$n" in *.so|*.a|*.o|*.sh|*.c|*.h|*.in|*.log|stdbuf|chroot|make-prime-list.dSYM) continue;; esac
  [ "$(stat -c %s "$f")" -gt 100000 ] || continue
  cp "$f" "$D/$n,ff8"
  progs+=("$n")
done
IFS=$'\n' progs=($(sort <<<"${progs[*]}")); unset IFS
{
  echo "| !Boot file for CoreUtils (GNU coreutils $V for RISC OS 64)"
  echo
  echo 'Set CoreUtils$Dir <Obey$Dir>'
  echo 'Set UnixEnv$swap 1'
  echo 'IfThere <CoreUtils$Dir>.!Sprites then Iconsprites <CoreUtils$Dir>.!Sprites'
  echo
  for n in "${progs[@]}"; do
    k=$(( ($(stat -c %s "$D/$n,ff8") + 1023) / 1024 + 192 ))
    echo "Set Alias\$$n WimpSlot -min ${k}k|MRun <CoreUtils\$Dir>.$n %%*0"
  done
} > "$D/!Boot"
printf '| !Run file for CoreUtils\n\n/<Obey$Dir>.!Boot\n' > "$D/!Run"
{
  echo "GNU coreutils $V for RISC OS 64 (AArch64)"
  echo
  echo "Run !CoreUtils (or its !Boot) once: it sets CoreUtils\$Dir and an alias for each program, so that the programs can be used by name from the command line, from Obey files, and by programs"
  echo "that use system () or posix_spawn. The shell of this port (sh) also finds them through CoreUtils\$Dir. 'install' is called 'ginstall', as in the 32-bit package."
  echo
  echo "Documentation: docs/USERLAND64.md in https://github.com/robheaton/riscos-gcc16 (file names, the shell, limits, porting)."
  echo
  echo "The programs are built with the GCC 16 tool chain for RISC OS 64 (UnixLib 64): see https://github.com/robheaton/riscos-gcc16 . What does not work on RISC OS: ln -s (no links), chown/chgrp (one user), df, who, users"
  echo "and uptime (no list of mounted file systems or logins), stty/tty (only the keyboard and screen)."
  echo
  cat "$S/README" 2>/dev/null
} > "$D/!Help"
for f in COPYING AUTHORS THANKS NEWS; do [ -f "$S/$f" ] && cp "$S/$f" "$D/$f"; done
cat > "$O/RiscPkg/Control" <<E
Package: CoreUtils
Version: $V
Section: Utilities
Priority: Optional
Environment: aarch64
Licence: GPL-3+
Maintainer: RISC OS 64 GCC port <https://github.com/robheaton/riscos-gcc16>
Standards-Version: 0.4.0
Components: Apps.Utilities.!CoreUtils (Movable LookAt)
Description: GNU core utilities
 The basic file, shell and text manipulation utilities that are expected to exist on every operating system,
 built for RISC OS 64: ${progs[*]}
Homepage: http://gnu.org/software/coreutils
E
printf 'coreutils\n\nGNU coreutils is copyrighted under the GNU General Public License version 3 or later (see COPYING in the application).\nBuilt from the unmodified source of coreutils 9.5 with the patches of the riscos-gcc16 project (ports/patches/coreutils-9.5-riscos64.patch).\n' > "$O/RiscPkg/Copyright"
( cd "$O" && rm -f "coreutils_${V}_aarch64.zip" && zip -qr "coreutils_${V}_aarch64.zip" Apps RiscPkg )
ls -l "$O/coreutils_${V}_aarch64.zip"; echo "${#progs[@]} programs"
