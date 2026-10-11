#!/usr/bin/env python3
"""install-oslib.py - build OSLib (the RISC OS Open library of the operating system: the C headers of the SWIs and the veneers that call them) FROM ITS OWN SOURCES with the cross compiler, and OSLibSupport, and
install them:

  <tool chain>/arm-riscos-gnueabihf/include/oslib/*         the 333 headers (C: *.h, assembler: *.Hdr) made by OSLib's DefMod from its definition files (the same as GCCSDK's, but for the date they say)
  <tool chain>/arm-riscos-gnueabihf/include/*.h            the headers of OSLibSupport (Choices.h, Err.h, Event.h, Message.h, Task.h, trace.h, X.h, Wimptypes.h ...)
  <tool chain>/arm-riscos-gnueabihf/lib/libOSLib32.a      the library (4,674 veneers; the same 4,668 symbols as GCCSDK's libOSLib32.a), and libOSLibH32.a (the same file, the name that some Makefiles use)
  <tool chain>/arm-riscos-gnueabihf/lib/libOSLibSupport32.a

usage: install-oslib.py --oslib-src DIR --toolchain DIR [--jobs N] [--keep BUILDDIR]

DIR is a checkout of  https://svn.code.sf.net/p/ro-oslib/code/trunk/!OSLib  (it has Makefile, Source and Tools).  OSLib is under the GNU General Public License (version 1 or later) with an exception for linking:
this project does not ship OSLib, its headers or the library; they are built here from your copy (docs/OSLIB.md).  The build is OSLib's own (DefMod makes the headers and the assembler veneers, the cross
compiler assembles them); the four patches of recipe/gcc-16.2.0-riscos/patches-oslib are applied first (they add the EABI hard-float assembler type to DefMod: GCCSDK's patches).  Needs  patch, make, a host C
compiler, bison and flex.  The library is made without a C library of its own (the veneers only call SWIs), so the same file links into programs that use UnixLib and into modules (gcc -mmodule)."""
import argparse, glob, os, shutil, subprocess, sys, tempfile

ap = argparse.ArgumentParser()
ap.add_argument("--oslib-src", required=True)
ap.add_argument("--toolchain", required=True)
ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
ap.add_argument("--keep", help="build in this folder and keep it")
a = ap.parse_args()
SRC = os.path.abspath(a.oslib_src); TC = os.path.abspath(a.toolchain)
HERE = os.path.dirname(os.path.abspath(__file__))
PATCHES = os.path.join(HERE, "..", "patches-oslib")
if not os.path.isdir(PATCHES): PATCHES = os.path.join(HERE, "patches-oslib")
for need in ("Makefile", "Source", "Tools"):
    if not os.path.exists(os.path.join(SRC, need)): sys.exit("%s: no %s (is it a checkout of !OSLib?)" % (SRC, need))
TARGET = "arm-riscos-gnueabihf"
BIN = os.path.join(TC, "bin")
for t in ("gcc", "ar", "nm", "objcopy"):
    if not os.path.exists(os.path.join(BIN, "%s-%s" % (TARGET, t))): sys.exit("no %s-%s in %s" % (TARGET, t, BIN))
for t in ("patch", "make", "bison", "flex", "cc"):
    if not shutil.which(t): sys.exit("%s is needed to build OSLib" % t)
W = a.keep or tempfile.mkdtemp(prefix="oslib-")
B = os.path.join(W, "oslib")
if os.path.exists(B): shutil.rmtree(B)
shutil.copytree(SRC, B, ignore=shutil.ignore_patterns(".svn"), symlinks=True)

def run(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode: sys.exit("%s\n%s%s" % (" ".join(cmd), r.stdout[-3000:], r.stderr[-3000:]))
    return r

for p in sorted(glob.glob(os.path.join(PATCHES, "*.patch"))):
    r = subprocess.run(["patch", "-p0", "--no-backup-if-mismatch", "-s"], cwd=B, stdin=open(p), capture_output=True, text=True)
    if r.returncode: sys.exit("%s does not apply to %s:\n%s%s" % (os.path.basename(p), SRC, r.stdout, r.stderr))
print("patches applied: %d" % len(glob.glob(os.path.join(PATCHES, "*.patch"))))
env = dict(os.environ, GCCSDK_INSTALL_CROSSBIN=BIN, GCCSDK_INSTALL_ENV=B, ELFOBJECTTYPE="ARMEABIHF")
run(["make", "tools"], cwd=B, env=env)
libfile = "%s rs" % os.path.join(BIN, TARGET + "-ar")
for target in ("oslib", "oslibsupport"):
    run(["make", "-j%d" % a.jobs, "-C", "Source", target, "BUILDTYPE=CROSSGCC_ELF", "ELFOBJECTTYPE=ARMEABIHF", "LIBFILE=" + libfile], cwd=B, env=env)
BUILD = os.path.join(B, "Build")
lib = os.path.join(BUILD, "libOSLib32.a"); sup = os.path.join(BUILD, "OSLibSupport", "libOSLibSupport32.a")
for f in (lib, sup):
    if not os.path.exists(f): sys.exit("the build made no " + f)
nm = os.path.join(BIN, TARGET + "-nm"); ar = os.path.join(BIN, TARGET + "-ar")
members = len(run([ar, "t", lib]).stdout.split())
print("libOSLib32.a: %d members, libOSLibSupport32.a: %d bytes" % (members, os.path.getsize(sup)))

# every member of the library must link, as a program that uses UnixLib and as a module (through modreloc).  A few functions are defined by two members of OSLib's library (xos_read_var_val_size
# is in XOSReadVarValSize.o and in 0x00020x.000.o: GCCSDK's library has the same ones; a normal link takes one member per symbol), so the test allows multiple definitions and counts them.
dup = len(run(["sh", "-c", "%s -g --defined-only %s | awk '$2 ~ /^[TDBRWV]$/ {print $3}' | sort | uniq -d | wc -l" % (os.path.join(BIN, TARGET + "-nm"), lib)]).stdout.split() and run(["sh", "-c", "%s -g --defined-only %s | awk '$2 ~ /^[TDBRWV]$/ {print $3}' | sort | uniq -d" % (os.path.join(BIN, TARGET + "-nm"), lib)]).stdout.split())
print("symbols that two members define: %d (the first one is used)" % dup)
CC = os.path.join(BIN, TARGET + "-gcc")
t = os.path.join(W, "t"); os.makedirs(t, exist_ok=True)
open(t + "/m.c", "w").write("int main (void) { return 0; }\n")
r = subprocess.run([CC, "-o", t + "/prog", t + "/m.c", "-Wl,--allow-multiple-definition", "-Wl,--whole-archive", lib, sup, "-Wl,--no-whole-archive"], capture_output=True, text=True)
print("link of the whole library into a program: %s" % ("OK" if r.returncode == 0 else "FAILED\n" + r.stderr[-1500:]))
bad = r.returncode != 0
if os.path.exists(os.path.join(BIN, "cmunge")) and os.path.exists(os.path.join(BIN, TARGET + "-modreloc")):
    open(t + "/lt.cmhg", "w").write("title-string: LT\nhelp-string: LT 1.00\ninitialisation-code: lt_init\nfinalisation-code: lt_final\n")
    open(t + "/lt.c", "w").write('#include <kernel.h>\n#include "lt.h"\n_kernel_oserror *lt_init (const char *t, int p, void *pw) { return 0; }\n_kernel_oserror *lt_final (int f, int p, void *pw) { return 0; }\n')
    steps = [[os.path.join(BIN, "cmunge"), "-tgcc", "-32bit", "-p", "-d", t + "/lt.h", "-o", t + "/lth.o", t + "/lt.cmhg"],
             [CC, "-mmodule", "-O2", "-c", "-I", t, t + "/lt.c", "-o", t + "/ltc.o"],
             [CC, "-mmodule", "-o", t + "/lt.elf", t + "/ltc.o", t + "/lth.o", "-Wl,--allow-multiple-definition", "-Wl,--whole-archive", lib, "-Wl,--no-whole-archive"],
             [os.path.join(BIN, TARGET + "-modreloc"), "-q", t + "/lt.elf", t + "/lt,ffa"]]
    ok = True
    for s in steps:
        r = subprocess.run(s, capture_output=True, text=True)
        if r.returncode: ok = False; print("link of the whole library into a module: FAILED\n" + (r.stderr or r.stdout)[-1500:]); break
    if ok: print("link of the whole library into a module (modreloc): OK")
    bad = bad or not ok
if bad: sys.exit(1)

INC = os.path.join(TC, TARGET, "include"); LIB = os.path.join(TC, TARGET, "lib")
os.makedirs(os.path.join(INC, "oslib"), exist_ok=True); os.makedirs(LIB, exist_ok=True)
n = 0
for h in sorted(glob.glob(os.path.join(BUILD, "Header", "oslib", "*"))):            # the C headers (*.h) and the assembler headers (*.Hdr)
    if os.path.isfile(h): shutil.copyfile(h, os.path.join(INC, "oslib", os.path.basename(h))); n += 1
for h in sorted(glob.glob(os.path.join(BUILD, "OSLibSupport", "*.h"))):
    shutil.copyfile(h, os.path.join(INC, os.path.basename(h)))
# -mmodule searches include-modkit and not the include folder of the sysroot: a link there (as mkoslib-lib.sh makes it)
for d in glob.glob(os.path.join(TC, "lib", "gcc", TARGET, "*", "include-modkit")):
    link = os.path.join(d, "oslib")
    if os.path.lexists(link): os.unlink(link) if (os.path.islink(link) or os.path.isfile(link)) else shutil.rmtree(link)
    os.symlink(os.path.relpath(os.path.join(INC, "oslib"), d), link)
shutil.copyfile(lib, os.path.join(LIB, "libOSLib32.a")); shutil.copyfile(lib, os.path.join(LIB, "libOSLibH32.a")); shutil.copyfile(sup, os.path.join(LIB, "libOSLibSupport32.a"))
print("installed: %d header files (C and assembler) in %s, the support headers in %s, libOSLib32.a, libOSLibH32.a, libOSLibSupport32.a in %s" % (n, os.path.join(INC, "oslib"), INC, LIB))
if not a.keep: shutil.rmtree(W, ignore_errors=True)
