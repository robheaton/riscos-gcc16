#!/usr/bin/env python3
"""install-rufl.py - build RUfl (the RISC OS Unicode font library of NetSurf: text in any Unicode character with the fonts that the Font Manager has, with font substitution; git://git.netsurf-browser.org/librufl.git)
for the cross compiler and UnixLib, and install

  <tool chain>/arm-riscos-gnueabihf/include/rufl.h     the header
  <tool chain>/arm-riscos-gnueabihf/lib/librufl.a      the library (-lrufl -lOSLib32)

usage: install-rufl.py --rufl-src DIR --toolchain DIR [--keep BUILDDIR]

DIR is a checkout of the repository.  RUfl is under the MIT licence: this project does not ship it (as with the other libraries: docs/RUFL.md); it is built here from your copy.  It needs OSLib in the same tool chain
(install-oslib.py: the sources include <oslib/font.h> and others) and Perl (the table of glyph names is made from src/Glyphs by tools/makeglyphs).  NetSurf's build system is not used: the script compiles the sources
that src/Makefile lists with the flags of the project's Makefile (-std=c99 -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=500).  One patch (patches-rufl/rufl-log-off.patch) turns off the library's debug log, which is hard-wired on and prints to stderr; -DRUFL_LOG turns it on.  The library is tested by linking every member into a program."""
import argparse, os, re, shutil, subprocess, sys, tempfile

ap = argparse.ArgumentParser()
ap.add_argument("--rufl-src", required=True)
ap.add_argument("--toolchain", required=True)
ap.add_argument("--keep", help="build in this folder and keep it")
a = ap.parse_args()
SRC = os.path.abspath(a.rufl_src); TC = os.path.abspath(a.toolchain)
if not os.path.exists(os.path.join(SRC, "src", "rufl_init.c")) or not os.path.exists(os.path.join(SRC, "tools", "makeglyphs")): sys.exit("%s: no src/rufl_init.c or tools/makeglyphs (is it a checkout of librufl?)" % SRC)
TARGET = "arm-riscos-gnueabihf"; BIN = os.path.join(TC, "bin")
CC = os.path.join(BIN, TARGET + "-gcc"); AR = os.path.join(BIN, TARGET + "-ar")
INC = os.path.join(TC, TARGET, "include"); LIB = os.path.join(TC, TARGET, "lib")
for t in (CC, AR, os.path.join(INC, "oslib", "font.h"), os.path.join(LIB, "libOSLib32.a")):
    if not os.path.exists(t): sys.exit("%s is missing: run install-oslib.py first" % t)
perl = shutil.which("perl") or sys.exit("perl is needed (tools/makeglyphs)")
HERE = os.path.dirname(os.path.abspath(__file__))
PATCH = os.path.join(HERE, "..", "patches-rufl", "rufl-log-off.patch")
if not os.path.exists(PATCH): PATCH = os.path.join(HERE, "rufl-log-off.patch")
mk = open(os.path.join(SRC, "src", "Makefile")).read()
m = re.search(r"^DIR_SOURCES\s*:=\s*((?:.*\\\n)*.*)$", mk, re.M)
if not m: sys.exit("no DIR_SOURCES list in src/Makefile")
files = [f for f in m.group(1).replace("\\", " ").split() if f.endswith(".c")]
W = a.keep or tempfile.mkdtemp(prefix="rufl-")
os.makedirs(W, exist_ok=True)
B = os.path.join(W, "librufl")
if os.path.exists(B): shutil.rmtree(B)
shutil.copytree(SRC, B, ignore=shutil.ignore_patterns(".git"))
SRC = B
r = subprocess.run(["patch", "-p1", "--no-backup-if-mismatch", "-s"], cwd=B, stdin=open(PATCH), capture_output=True, text=True)
if r.returncode: sys.exit("the patch does not apply:\n%s%s" % (r.stdout, r.stderr))
gm = os.path.join(W, "rufl_glyph_map.c")
with open(os.path.join(SRC, "src", "Glyphs")) as fi, open(gm, "w") as fo:
    r = subprocess.run([perl, os.path.join(SRC, "tools", "makeglyphs")], stdin=fi, stdout=fo, stderr=subprocess.PIPE, text=True)
if r.returncode: sys.exit("makeglyphs failed: " + r.stderr[-1000:])
flags = ["-std=c99", "-O2", "-Wall", "-Wno-unused-parameter", "-D_DEFAULT_SOURCE", "-D_XOPEN_SOURCE=500", "-I", os.path.join(SRC, "include"), "-I", os.path.join(SRC, "src")]
objs = []; bad = False
for f in [os.path.join(SRC, "src", x) for x in files] + [gm]:
    o = os.path.join(W, os.path.basename(f)[:-2] + ".o")
    r = subprocess.run([CC] + flags + ["-c", f, "-o", o], capture_output=True, text=True)
    if r.returncode: bad = True; print("%s does not compile:\n%s" % (os.path.basename(f), r.stderr[-2000:]))
    objs.append(o)
if bad: sys.exit(1)
lib = os.path.join(W, "librufl.a")
if os.path.exists(lib): os.unlink(lib)
subprocess.run([AR, "rcs", lib] + objs, check=True)
open(W + "/m.c", "w").write("int main (void) { return 0; }\n")
r = subprocess.run([CC, "-o", W + "/prog", W + "/m.c", "-Wl,--whole-archive", lib, "-Wl,--no-whole-archive", os.path.join(LIB, "libOSLib32.a")], capture_output=True, text=True)
print("link of every member into a program (with OSLib): %s" % ("OK" if r.returncode == 0 else "FAILED\n" + r.stderr[-2500:]))
if r.returncode: sys.exit(1)
shutil.copyfile(os.path.join(SRC, "include", "rufl.h"), os.path.join(INC, "rufl.h")); shutil.copyfile(lib, os.path.join(LIB, "librufl.a"))
print("installed: %d members in librufl.a (%s), rufl.h in %s" % (len(objs), LIB, INC))
if not a.keep: shutil.rmtree(W, ignore_errors=True)
