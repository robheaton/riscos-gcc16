#!/usr/bin/env python3
"""install-flexlib.py - build FlexLib (Acorn's flex-block heap for Wimp programs, in the form of Stephen Fryatt's repository https://github.com/steve-fryatt/flexlib: one source file, flex.c) for the cross
compiler and UnixLib, and install

  <tool chain>/arm-riscos-gnueabihf/include/flex.h        the header
  <tool chain>/arm-riscos-gnueabihf/lib/libFlexLib32.a    the library for UnixLib programs (-lFlexLib32)

usage: install-flexlib.py --flexlib-src DIR --toolchain DIR [--keep BUILDDIR]

DIR is a checkout of the repository.  FlexLib is under the Apache License 2.0 (Copyright 1997 Acorn Computers Ltd): this project does not ship it; it is built here from your copy (docs/FLEXLIB.md).
The source is patched first (recipe/gcc-16.2.0-riscos/patches-flexlib/flexlib-unixlib.patch: under UnixLib, flex_set_budge does nothing, because _kernel_register_slotextend belongs to the Shared C
Library only; the old build of GCCSDK used -mlibscl).  The library is tested by linking it into a program."""
import argparse, os, shutil, subprocess, sys, tempfile

ap = argparse.ArgumentParser()
ap.add_argument("--flexlib-src", required=True)
ap.add_argument("--toolchain", required=True)
ap.add_argument("--keep", help="build in this folder and keep it")
a = ap.parse_args()
SRC = os.path.abspath(a.flexlib_src); TC = os.path.abspath(a.toolchain)
HERE = os.path.dirname(os.path.abspath(__file__))
PATCH = os.path.join(HERE, "..", "patches-flexlib", "flexlib-unixlib.patch")
if not os.path.exists(PATCH): PATCH = os.path.join(HERE, "flexlib-unixlib.patch")
if not os.path.exists(os.path.join(SRC, "src", "flex.c")): sys.exit("%s: no src/flex.c (is it a checkout of steve-fryatt/flexlib?)" % SRC)
TARGET = "arm-riscos-gnueabihf"; BIN = os.path.join(TC, "bin")
CC = os.path.join(BIN, TARGET + "-gcc"); AR = os.path.join(BIN, TARGET + "-ar")
for t in (CC, AR, "patch"):
    if not (os.path.exists(t) or shutil.which(t)): sys.exit("need " + t)
W = a.keep or tempfile.mkdtemp(prefix="flexlib-")
B = os.path.join(W, "flexlib")
if os.path.exists(B): shutil.rmtree(B)
shutil.copytree(SRC, B, ignore=shutil.ignore_patterns(".git"))
r = subprocess.run(["patch", "-p1", "--no-backup-if-mismatch", "-s"], cwd=B, stdin=open(PATCH), capture_output=True, text=True)
if r.returncode: sys.exit("the patch does not apply to %s:\n%s%s" % (SRC, r.stdout, r.stderr))
obj = os.path.join(B, "flex.o")
r = subprocess.run([CC, "-O2", "-Wall", "-fno-strict-aliasing", "-Wno-unknown-pragmas", "-c", os.path.join(B, "src", "flex.c"), "-o", obj], capture_output=True, text=True)
if r.returncode: sys.exit("flex.c does not compile:\n" + r.stderr[-3000:])
lib = os.path.join(B, "libFlexLib32.a")
subprocess.run([AR, "rcs", lib, obj], check=True)
t = os.path.join(W, "t"); os.makedirs(t, exist_ok=True)
open(t + "/t.c", "w").write('#include <flex.h>\n#include <stddef.h>\nint main (void) { void *p; flex_init ("t", 0, 0); if (flex_alloc (&p, 100)) { flex_extend (&p, 200); flex_free (&p); } return flex_size (&p) < 0 || flex_set_budge (0) > 1; }\n')
r = subprocess.run([CC, "-I", os.path.join(B, "src"), "-o", t + "/t", t + "/t.c", lib], capture_output=True, text=True)
print("link of a program that uses flex: %s" % ("OK" if r.returncode == 0 else "FAILED\n" + r.stderr[-1500:]))
if r.returncode: sys.exit(1)
INC = os.path.join(TC, TARGET, "include"); LIB = os.path.join(TC, TARGET, "lib")
shutil.copyfile(os.path.join(B, "src", "flex.h"), os.path.join(INC, "flex.h")); shutil.copyfile(lib, os.path.join(LIB, "libFlexLib32.a"))
print("installed: flex.h in %s, libFlexLib32.a in %s" % (INC, LIB))
if not a.keep: shutil.rmtree(W, ignore_errors=True)
