#!/bin/bash
# build-gnu.sh PREFIX WORKDIR: GNU sed 4.9, grep 3.11, gawk 5.3.1 and coreutils 9.5 for RISC OS 64 (UnixLib 64) with the aarch64-riscos tool chain in PREFIX: WORKDIR/sed,ff8 grep,ff8 gawk,ff8, and WORKDIR/coreutils-pkg/coreutils_9.5-1_aarch64.zip (the application !CoreUtils: one program per utility and a !Boot that sets an Alias$ for each, like the 32-bit package coreutils_8.30-2_arm.zip; see package-coreutils.sh).
# Tested on RISC OS Pyromaniac on 18 October 2026 (jobs port_sed, port_grep, port_gawk of tools/run_on_service.py; the programs need the shell, ports/sh, for the pipes and system () of awk).
#   sed: s, y, addresses, -n, -E, -f, -i (in place; this needed inode numbers from stat), l, =, N/P/D, labels and branches.   grep: the usual options (-E -F -i -v -c -n -o -w -x -l -L -H -f -A -q, back references).
#   gawk: fields, arrays, functions, printf, sub/gsub/match/split, getline from files and pipes, output to files and pipes, system (), strftime.   No extensions (shared libraries), no readline, no MPFR.
# gawk is built with PIPES_SIMULATED and patched (patches/gawk-5.3.1-riscos64.patch) so that its pipes and system () use popen and system () of the C library, which run  sh -c  when there is an sh.
set -e
P=$(cd "${1:?PREFIX}" && pwd); W=${2:?WORKDIR}
mkdir -p "$W"; W=$(cd "$W" && pwd); cd "$W"
HERE=$(dirname "$(readlink -f "$0")")
CC="$P/bin/aarch64-riscos-gcc -munixlib"
BUILD="$(uname -m)-pc-linux-gnu"
fetch () { [ -f "$2" ] || curl -sSLO "https://ftp.gnu.org/gnu/$1/$2"; echo "$3  $2" | sha256sum -c -; }
fetch sed sed-4.9.tar.xz 6e226b732e1cd739464ad6862bd1a1aba42d7982922da7a53519631d24975181
fetch grep grep-3.11.tar.xz 1db2aedde89d0dea42b16d9528f894c8d15dae4e190b59aecc78f5a951276eab
fetch gawk gawk-5.3.1.tar.xz 694db764812a6236423d4ff40ceb7b6c4c441301b72ad502bb5c27e00cd56f78
for t in sed-4.9 grep-3.11 gawk-5.3.1; do [ -d $t ] || tar xf $t.tar.xz; done
[ -f .gawk-patched ] || { patch -p0 < "$HERE/patches/gawk-5.3.1-riscos64.patch" && touch .gawk-patched; }
mkdir -p sed-build grep-build gawk-build
(cd sed-build && ../sed-4.9/configure --host=aarch64-riscos --build=$BUILD CC="$CC" CFLAGS=-O2 --disable-nls --without-selinux --disable-acl --disable-i18n > conf.log 2>&1 && make -j"$(nproc)" -C . > make.log 2>&1 || true)	# (the tests of gnulib do not build; the programs do)
cp sed-build/sed/sed "sed,ff8"
(cd grep-build && ../grep-3.11/configure --host=aarch64-riscos --build=$BUILD CC="$CC" CFLAGS=-O2 --disable-nls --disable-perl-regexp > conf.log 2>&1 && make -j"$(nproc)" > make.log 2>&1 || true)
cp grep-build/src/grep "grep,ff8"
(cd gawk-build && ../gawk-5.3.1/configure --host=aarch64-riscos --build=$BUILD CC="$CC" CFLAGS=-O2 --disable-nls --disable-extensions --without-readline --without-mpfr --disable-pma > conf.log 2>&1 \
  && make -j"$(nproc)" CFLAGS="-O2 -std=gnu17 -include stdbool.h -DPIPES_SIMULATED" > make.log 2>&1)	# (gawk's own getopt.c is K&R; C23 refuses it; dfa.h needs bool)
cp gawk-build/gawk "gawk,ff8"
# diffutils, findutils, patch, tar, gzip
for pkg in diffutils-3.10:diffutils findutils-4.10.0:findutils patch-2.7.6:patch tar-1.35:tar gzip-1.13:gzip; do t=${pkg%%:*}; n=${pkg##*:}
  case $n in diffutils) h=90e5e93cc724e4ebe12ede80df1634063c7a855692685919bfe60b556c9bd09e;; findutils) h=1387e0b67ff247d2abde998f90dfbf70c1491391a59ddfecb8ae698789f0a4f5;; patch) h=ac610bda97abe0d9f6b7c963255a11dcb196c25e337c61f94e4778d632f1d8fd;; tar) h=4d62ff37342ec7aed748535323930c7cf94acf71c3591882b26a7ea50f3edc16;; gzip) h=7454eb6935db17c6655576c2e1b0fabefd38b4d0936e0f87f48cd062ce91a057;; esac
  fetch $n $t.tar.xz $h
  [ -d $t ] || tar xf $t.tar.xz
  python3 "$HERE/patches/gnulib-riscos64.py" $t > /dev/null	# (freadahead and friends: picolibc's FILE)
  mkdir -p $n-build
  extra=-O2; [ $n = findutils ] && extra="-O2 -Dfork=vfork" && { [ -f .findutils-patched ] || { patch -p0 < "$HERE/patches/findutils-4.10.0-riscos64.patch" && touch .findutils-patched; }; }	# (find and xargs: fork is vfork; xargs's child must not change the table of its parent)
  (cd $n-build && ../$t/configure --host=aarch64-riscos --build=$BUILD CC="$CC" CFLAGS="$extra" --disable-nls --without-selinux --disable-acl --disable-xattr > conf.log 2>&1 && make -k -j"$(nproc)" > make.log 2>&1 || true)
done

# coreutils: one program per utility (no --enable-single-binary), then the application
fetch coreutils coreutils-9.5.tar.xz cd328edeac92f6a665de9f323c93b712af1858bc2e0d88f3f7100469470a1b8a
[ -d coreutils-9.5 ] || tar xf coreutils-9.5.tar.xz
[ -f .coreutils-patched ] || { patch -p0 < "$HERE/patches/coreutils-9.5-riscos64.patch" && touch .coreutils-patched; }
mkdir -p cu-build
(cd cu-build && ../coreutils-9.5/configure --host=aarch64-riscos --build=$BUILD CC="$CC" CFLAGS=-O2 --disable-nls --disable-acl --disable-xattr --without-selinux --without-openssl > conf.log 2>&1 && make -k -j"$(nproc)" > make.log 2>&1 || true)	# (stdbuf's shared library and the tests of gnulib do not build; the programs do)
"$HERE/package-coreutils.sh" cu-build coreutils-9.5 coreutils-pkg 9.5-1
ls -l "$W"/*,ff8
