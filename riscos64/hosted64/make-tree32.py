#!/usr/bin/env python3
"""make-tree32.py DEST --gcc GCCBUILD --binutils BINUTILSBUILD --sysroot NATIVETREE32 [--cxx]

Lays out a folder with the compiler that RUNS on RISC OS 64 and makes programs for 32 bit RISC OS (arm-riscos-gnueabihf), in the same flat layout as ports/make-native-tree.py (the programs of
build-hosted64.sh, with UnixLib 64's file names: x.h is h/x, x.o is o/x ...; programs have the type suffix ,ff8).  The headers and libraries of the 32 bit target (UnixLib 5, libgcc, libstdc++, the
start-up files) are data files that do not depend on the machine they are used on: they are taken from the tree of the 32 bit native compiler (SYSROOT = native-tree/gcc16, made by make-native-tree.sh).

  DEST/xgcc,ff8 [xg++,ff8]  bin/{cc1,[cc1plus,]collect2,as,ld,objcopy},ff8  lib/*  include/**  c/hello  [cc/hellopp]  o/ out/

finish-native-package.py (native32) adds the launcher, the scripts, zlib, the ReadMe and the licences and makes the zip."""
import argparse, os, shutil, sys

SFIX = "c cc f h i ii l o p s y".split()
ap = argparse.ArgumentParser()
ap.add_argument("dest"); ap.add_argument("--gcc", required=True); ap.add_argument("--binutils", required=True); ap.add_argument("--sysroot", required=True)
ap.add_argument("--cxx", action="store_true")
a = ap.parse_args()
D, S = a.dest, a.sysroot
T, V = "arm-riscos-gnueabihf", "16.2.0"
if os.path.exists(D): sys.exit("%s exists: remove it yourself" % D)

def put(src, dst):
    os.makedirs(os.path.dirname(os.path.join(D, dst)), exist_ok=True)
    shutil.copyfile(src, os.path.join(D, dst))

def swapped(rel):
    d, f = os.path.split(rel)
    if "." in f:
        stem, ext = f.rsplit(".", 1)
        if ext in SFIX and stem:
            return os.path.join(d, ext, stem)
    return rel

put(os.path.join(a.gcc, "gcc", "xgcc"), "xgcc,ff8")
put(os.path.join(a.gcc, "gcc", "cc1"), "bin/cc1,ff8")
put(os.path.join(a.gcc, "gcc", "collect2"), "bin/collect2,ff8")
if a.cxx:
    put(os.path.join(a.gcc, "gcc", "xg++"), "xg++,ff8")
    put(os.path.join(a.gcc, "gcc", "cc1plus"), "bin/cc1plus,ff8")
put(os.path.join(a.binutils, "gas", "as-new"), "bin/as,ff8")
put(os.path.join(a.binutils, "ld", "ld-new"), "bin/ld,ff8")
put(os.path.join(a.binutils, "binutils", "objcopy"), "bin/objcopy,ff8")
# the libraries and start-up files of the target: T/lib and lib/gcc/T/V (not Fortran)
for src in (os.path.join(S, T, "lib"), os.path.join(S, "lib", "gcc", T, V)):
    for f in sorted(os.listdir(src)):
        p = os.path.join(src, f)
        if not os.path.isfile(p) or "gfortran" in f or "caf" in f or f.endswith(",e1f"):
            continue
        put(p, os.path.join("lib", swapped(f)))
def tree(root, dstroot, skip=()):
    for dp, dn, fn in os.walk(root):
        rel = os.path.relpath(dp, root)
        for f in fn:
            if f in skip and rel == ".": continue
            r = os.path.normpath(os.path.join(rel, f))
            put(os.path.join(dp, f), os.path.join(dstroot, swapped(r)))
tree(os.path.join(S, T, "include"), "include")
# GCC's own headers (stddef.h, stdarg.h ...); its limits.h would #include_next the C library's, which a flat tree cannot do: the library's own limits.h stays
tree(os.path.join(S, "lib", "gcc", T, V, "include"), "include", skip=("limits.h",))
if a.cxx:
    tree(os.path.join(S, "include", "c++"), os.path.join("include", "c++"))
here = os.path.dirname(os.path.abspath(__file__))
# lib/specs: read by the driver (-B lib/).  A link needs -rpath-link for each -L directory (libunixlib.so refers to libgcc_s.so.1: the cross build's default search paths are not there) and the shared libgcc
open(os.path.join(D, "lib", "specs"), "w").write("*link:\n+ %{L*:-rpath-link %*}\n\n*libgcc:\n%{static|static-libgcc:-lgcc -lgcc_eh;:-lgcc_s -lgcc}\n")
put(os.path.join(here, "tests", "hello.c"), "c/hello")
if a.cxx: put(os.path.join(here, "tests", "hellopp.cc"), "cc/hellopp")
for d in ("o", "out"):
    os.makedirs(os.path.join(D, d), exist_ok=True)
    open(os.path.join(D, d, "keep"), "w").write("keep\n")
n = sum(len(f) for _, _, f in os.walk(D))
print("%s: %d files, %.1f MB" % (D, n, sum(os.path.getsize(os.path.join(r, f)) for r, _, fs in os.walk(D) for f in fs) / 1e6))
