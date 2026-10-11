#!/usr/bin/env python3
"""build-examples.py - build the example programs of DeskLib (the Examples folder of https://github.com/riscos-dot-info/desklib) with the cross compiler and libDesk.a (install-desklib.py), as RISC OS
applications: for every example folder with C files, a copy of the folder without the sources, with  !RunImage,e1f  built from all its C files, and the !Run file's WimpSlot raised to 2 MB (UnixLib programs
need more than the 64k to 320k of the Norcroft originals).

usage: build-examples.py --examples DIR --toolchain DIR --out DIR
Prints one line per example (built, or the first error)."""
import argparse, glob, os, re, shutil, subprocess, sys, tempfile
ap = argparse.ArgumentParser()
ap.add_argument("--examples", required=True); ap.add_argument("--toolchain", required=True); ap.add_argument("--out", required=True)
a = ap.parse_args()
TC = os.path.abspath(a.toolchain); EX = os.path.abspath(a.examples); OUT = os.path.abspath(a.out)
CC = os.path.join(TC, "bin", "arm-riscos-gnueabihf-gcc")
INC = os.path.join(TC, "arm-riscos-gnueabihf", "include", "DeskLib")
W = tempfile.mkdtemp(prefix="dlex-")
env = dict(os.environ, DESKLIB_PATH=INC)
open(W + "/p.c", "w").write('#include "Desklib:Core.h"\nint x;\n')
flags = ["-O2", "-std=gnu99", "-fcommon", "-w", "-fpermissive"]                # (the examples are written for old compilers: GCC 14 and later make an int from a pointer, an implicit declaration and the like errors; -fpermissive makes them warnings)
if subprocess.run([CC, "-c", W + "/p.c", "-o", W + "/p.o"], env=env, capture_output=True).returncode:       # an older cross compiler: a folder of links named DeskLib:Core.h
    shim = W + "/shim"; os.makedirs(shim)
    for h in glob.glob(INC + "/*.h"):
        b = os.path.basename(h)
        for pre in ("DeskLib", "Desklib", "DESKLIB"):
            for name in {b, b.lower(), b.capitalize()}:                 # (an older cross compiler does not match the case of the file name: Colourtran.h is ColourTran.h)
                link = "%s/%s:%s" % (shim, pre, name)
                if not os.path.lexists(link): os.symlink(h, link)
    flags += ["-I", shim]
os.makedirs(OUT, exist_ok=True)
# Fixups of the examples, applied to a copy of the sources (the upstream files are not changed).  !Dialog2EG: Event_Initialise expresses an interest in NO messages (DeskLib 2.41 and later, Wimp 3.10),
# but the Info entry of its menu is a submenu that is opened by Message_MenuWarning: the task must ask for that message.
FIXUPS = {"!Dialog2EG": [('Event_Initialise( "Dialog2 example");', '{ static int wanted[] = { 0x400C0, 0 };    /* Message_MenuWarning */\n  Event_Initialise3( "Dialog2 example", 310, wanted); }')]}
built, failed = [], []
for d in sorted(os.listdir(EX)):
    src = os.path.join(EX, d)
    if not os.path.isdir(src) or not d.startswith("!"): continue
    cfiles = sorted(glob.glob(src + "/*.c"))
    if not cfiles: continue
    dst = os.path.join(OUT, d)
    if os.path.isdir(dst): shutil.rmtree(dst)
    os.makedirs(dst)
    for f in os.listdir(src):                                       # the resources (templates, sprites, messages, !Run ...): everything but the sources
        p = os.path.join(src, f)
        if os.path.isfile(p) and not (f.endswith(".c") or f.endswith(".h") or f.lower().startswith("makefile") or f == "!Debug" or f == "!RunDebug,feb"): shutil.copy(p, os.path.join(dst, f))
    run = os.path.join(dst, "!Run,feb")
    if os.path.exists(run):
        t = open(run, "rb").read().decode("latin-1").replace("\r", "")
        t = re.sub(r"(?im)^WimpSlot .*$", "WimpSlot -min 2M -max 2M", t)
        open(run, "w", newline="\n").write(t)
    objs = []
    err = ""
    for i, c in enumerate(cfiles):
        o = os.path.join(W, "%s_%d.o" % (d.strip("!"), i))
        if d in FIXUPS:
            text = open(c, "rb").read().decode("latin-1")
            for old, new in FIXUPS[d]:
                if old not in text: sys.exit("%s: the line to fix is not there: %s" % (c, old))
                text = text.replace(old, new)
            fixed = os.path.join(W, "fixed_%d_%s" % (i, os.path.basename(c)))
            open(fixed, "wb").write(text.encode("latin-1"))
            c = fixed
        r = subprocess.run([CC, "-c"] + flags + ["-I", src, "-I", EX + "/OtherSrc", c, "-o", o], env=env, capture_output=True, text=True)
        if r.returncode: err = (r.stderr.strip().split("\n") or [""])[0][:200]; break
        objs.append(o)
    if not err:
        r = subprocess.run([CC, "-o", os.path.join(dst, "!RunImage,e1f")] + objs + ["-lDesk"], env=env, capture_output=True, text=True)
        if r.returncode: err = "link: " + " ".join((r.stderr.strip().split("\n") or [""])[:2])[:300]
    (failed if err else built).append(d)
    print("%-14s %s" % (d, ("FAILED: " + err) if err else "built (%d C files, %d bytes)" % (len(cfiles), os.path.getsize(os.path.join(dst, "!RunImage,e1f")))))
shutil.rmtree(W, ignore_errors=True)
print("%d built, %d failed" % (len(built), len(failed)))
sys.exit(1 if failed else 0)
