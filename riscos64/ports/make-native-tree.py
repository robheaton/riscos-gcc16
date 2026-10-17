#!/usr/bin/env python3
"""make-native-tree.py DEST --prefix PREFIX --gcc GCCBUILD --binutils BINUTILSBUILD [--asm FILE]

Lays out a folder with the native tool chain for RISC OS 64 (the programs of build-native-gcc.sh and build-native-binutils.sh, the SDK start-up files and libraries, the C headers, a test program), with the file
names the way UnixLib 64 looks for them: a file called  x.h  is  h/x  on RISC OS (the suffixes c, cc, f, h, i, ii, l, o, p, s, y are swapped into directories).  Programs get the type suffix ,ff8.  The tree is meant to be
zipped and run on RISC OS 64 (the build service for Pyromaniac takes such a zip as it is).

  DEST/xgcc,ff8  bin/{cc1,as,collect2,ld,objcopy},ff8  lib/{o/*, libc.a, libm.a, libgcc.a, libriscosstub.a, aif64.ld}  include/**  c/hello  s/hello  o/ out/"""
import argparse, os, shutil, sys

SFIX = "c cc f h i ii l o p s y".split()
ap = argparse.ArgumentParser()
ap.add_argument("dest")
ap.add_argument("--prefix", required=True, help="the installed aarch64-riscos tool chain (target6-inst)")
ap.add_argument("--gcc", required=True, help="the build directory of build-native-gcc.sh (natgcc/build)")
ap.add_argument("--binutils", required=True, help="the build directory of build-native-binutils.sh (nat2/build)")
ap.add_argument("--hello", help="a C file for c/hello")
ap.add_argument("--asm", help="its assembly (s/hello), from the cross compiler")
a = ap.parse_args()
D = a.dest
if os.path.exists(D):
    sys.exit("%s exists: remove it yourself" % D)
T = os.path.join(a.prefix, "aarch64-riscos")
G = os.path.join(a.prefix, "lib", "gcc", "aarch64-riscos", "16.2.0")

def put(src, dst):
    os.makedirs(os.path.dirname(os.path.join(D, dst)), exist_ok=True)
    shutil.copyfile(src, os.path.join(D, dst))

def swapped(rel):
    """a relative Unix name -> the layout on disc: dir/x.h -> dir/h/x"""
    d, f = os.path.split(rel)
    if "." in f:
        stem, ext = f.rsplit(".", 1)
        if ext in SFIX and stem:
            return os.path.join(d, ext, stem)
    return rel

put(os.path.join(a.gcc, "gcc", "xgcc"), "xgcc,ff8")
put(os.path.join(a.gcc, "gcc", "cc1"), "bin/cc1,ff8")
put(os.path.join(a.gcc, "gcc", "collect2"), "bin/collect2,ff8")
put(os.path.join(a.binutils, "gas", "as-new"), "bin/as,ff8")
put(os.path.join(a.binutils, "ld", "ld-new"), "bin/ld,ff8")
put(os.path.join(a.binutils, "binutils", "objcopy"), "bin/objcopy,ff8")
for f in ("aifhdr", "crt0", "riscos_start", "riscos_syscalls", "swi"):
    put(os.path.join(T, "lib", f + ".o"), "lib/o/" + f)
for f in ("crti", "crtbegin", "crtend", "crtn"):
    put(os.path.join(G, f + ".o"), "lib/o/" + f)
for f, src in (("libc.a", T), ("libm.a", T), ("libriscosstub.a", T), ("aif64.ld", T), ("libgcc.a", G)):
    put(os.path.join(src, "lib" if src == T else "", f), "lib/" + f)
# the C headers: picolibc's and the SDK's (no C++), and GCC's own (stddef.h, stdarg.h ...)
for root, dirs, files in os.walk(os.path.join(T, "include")):
    rel = os.path.relpath(root, os.path.join(T, "include"))
    if rel == "c++" or rel.startswith("c++" + os.sep):
        dirs[:] = []
        continue
    for f in files:
        r = os.path.normpath(os.path.join(rel, f))
        put(os.path.join(root, f), os.path.join("include", swapped(r)))
for f in sorted(os.listdir(os.path.join(G, "include"))):
    if os.path.isfile(os.path.join(G, "include", f)):
        put(os.path.join(G, "include", f), os.path.join("include", swapped(f)))
if a.hello:
    put(a.hello, "c/hello")
if a.asm:
    put(a.asm, "s/hello")
for d in ("o", "out"):
    os.makedirs(os.path.join(D, d), exist_ok=True)
    open(os.path.join(D, d, "keep"), "w").write("keep\n")
n = sum(len(f) for _, _, f in os.walk(D))
print("%s: %d files, %.1f MB" % (D, n, sum(os.path.getsize(os.path.join(r, f)) for r, _, fs in os.walk(D) for f in fs) / 1e6))
