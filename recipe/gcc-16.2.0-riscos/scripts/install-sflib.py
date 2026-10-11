#!/usr/bin/env python3
"""install-sflib.py - build SFLib (Stephen Fryatt's C libraries for RISC OS Wimp programs: windows, menus, icons, events, saving, templates, messages, configuration; https://github.com/steve-fryatt/sflib)
for the cross compiler and UnixLib, and install

  <tool chain>/arm-riscos-gnueabihf/include/sflib/*.h     the headers (#include "sflib/event.h")
  <tool chain>/arm-riscos-gnueabihf/lib/libSFLib32.a      the library (-lSFLib32 -lFlexLib32 -lOSLib32)

usage: install-sflib.py --sflib-src DIR --toolchain DIR [--jobs N] [--keep BUILDDIR]

DIR is a checkout of the repository.  SFLib is under the European Union Public Licence 1.2 (compatible with the GPL v2): this project does not ship it; it is built here from your copy
(docs/FLEXLIB.md).  It needs OSLib (install-oslib.py: the sources include <oslib/wimp.h> and others) and FlexLib (install-flexlib.py: <flex.h>) in the same tool chain.  The sources are not patched; the
author's makefile (-mlibscl) is replaced by this script, with the same list of objects and the flags of the cross compiler for UnixLib.  strdup.c is for the Norcroft compiler only and is left out
as it is in the author's makefile.  The library is tested by linking every member into a program."""
import argparse, concurrent.futures as cf, glob, os, re, shutil, subprocess, sys, tempfile

ap = argparse.ArgumentParser()
ap.add_argument("--sflib-src", required=True)
ap.add_argument("--toolchain", required=True)
ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
ap.add_argument("--keep", help="build in this folder and keep it")
a = ap.parse_args()
SRC = os.path.abspath(a.sflib_src); TC = os.path.abspath(a.toolchain)
if not os.path.exists(os.path.join(SRC, "src", "event.c")) or not os.path.exists(os.path.join(SRC, "Makefile")): sys.exit("%s: no src/event.c or Makefile (is it a checkout of steve-fryatt/sflib?)" % SRC)
TARGET = "arm-riscos-gnueabihf"; BIN = os.path.join(TC, "bin")
CC = os.path.join(BIN, TARGET + "-gcc"); AR = os.path.join(BIN, TARGET + "-ar")
INC = os.path.join(TC, TARGET, "include"); LIB = os.path.join(TC, TARGET, "lib")
for t in (CC, AR, os.path.join(INC, "oslib", "wimp.h"), os.path.join(INC, "flex.h"), os.path.join(LIB, "libOSLib32.a"), os.path.join(LIB, "libFlexLib32.a")):
    if not os.path.exists(t): sys.exit("%s is missing: run install-oslib.py and install-flexlib.py first" % t)
mk = open(os.path.join(SRC, "Makefile")).read()
m = re.search(r"^OBJS\s*:=\s*((?:.*\\\n)*.*)$", mk, re.M)
if not m: sys.exit("no OBJS list in the Makefile of SFLib")
objs = [o[:-2] for o in m.group(1).replace("\\", " ").split() if o.endswith(".o")]
W = a.keep or tempfile.mkdtemp(prefix="sflib-")
os.makedirs(W, exist_ok=True)
S = os.path.join(SRC, "src")
def cc(n):
    r = subprocess.run([CC, "-std=gnu99", "-O2", "-Wall", "-fno-strict-aliasing", "-iquote", S, "-c", os.path.join(S, n + ".c"), "-o", os.path.join(W, n + ".o")], capture_output=True, text=True)
    return n, r
bad = False
with cf.ThreadPoolExecutor(a.jobs) as ex:
    for n, r in ex.map(cc, objs):
        if r.returncode: bad = True; print("%s.c does not compile:\n%s" % (n, r.stderr[-2000:]))
if bad: sys.exit(1)
lib = os.path.join(W, "libSFLib32.a")
if os.path.exists(lib): os.unlink(lib)
subprocess.run([AR, "rcs", lib] + [os.path.join(W, n + ".o") for n in objs], check=True)
open(W + "/m.c", "w").write("int main (void) { return 0; }\n")
r = subprocess.run([CC, "-o", W + "/prog", W + "/m.c", "-Wl,--allow-multiple-definition", "-Wl,--whole-archive", lib, "-Wl,--no-whole-archive", os.path.join(LIB, "libFlexLib32.a"), os.path.join(LIB, "libOSLib32.a")], capture_output=True, text=True)
print("link of every member into a program (with FlexLib and OSLib): %s" % ("OK" if r.returncode == 0 else "FAILED\n" + r.stderr[-2500:]))
if r.returncode: sys.exit(1)
d = os.path.join(INC, "sflib")
if os.path.isdir(d): shutil.rmtree(d)
os.makedirs(d)
for h in sorted(glob.glob(os.path.join(S, "*.h"))): shutil.copyfile(h, os.path.join(d, os.path.basename(h)))
shutil.copyfile(lib, os.path.join(LIB, "libSFLib32.a"))
print("installed: %d members in libSFLib32.a (%s), %d headers in %s" % (len(objs), LIB, len(os.listdir(d)), d))
if not a.keep: shutil.rmtree(W, ignore_errors=True)
