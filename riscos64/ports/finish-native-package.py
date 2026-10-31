#!/usr/bin/env python3
"""finish-native-package.py KIND TREE OUTDIR --version V --launcher ulrun,ff8 --ar ar,ff8 --zlib ZLIB-1.3.1-DIR [--repo REPO]

Makes the folder and the zip of the two compiler packages that run ON RISC OS 64:
  KIND native    riscos64-native-V    the compiler for RISC OS 64 (AArch64): C and C++        (TREE from ports/make-native-tree.py --cxx)
  KIND native32  riscos64-native32-V  the compiler that makes programs for 32 bit RISC OS     (TREE from hosted64/make-tree32.py --cxx)
TREE is a folder with the programs, libraries and headers in the layout of those two scripts (xgcc,ff8 xg++,ff8 bin/ lib/ include/ c/hello cc/hellopp o/ out/); it is copied, not changed.  This script adds
  bin/ar,ff8                   the archiver (the zlib example puts its objects into an archive first, so that the link has few files to open)
  zl/                          zlib 1.3.1 (ZLIB is the unpacked source) in the flat layout: c/x for x.c (test/example.c is c/example), h/x for x.h, o/ and out/ empty, link a response file; zconf.h as configure makes it
  scripts/                     Clean, the sample scripts of the package (the static ones are ports/native/scripts and hosted64/scripts) and the zlib script, which is written here from the file list
  Run*,ff8                     a copy of the launcher (ports/native/ulrun.c) for each sample script: it runs scripts.<its own name>
  ReadMe, Licences/            ports/native/ReadMe-<kind>.txt with @VERSION@ filled in, and the licence texts of REPO (licenses/*.txt, LICENSES.md) and zlib's
and writes OUTDIR/riscos64-<kind>-V.zip (the RISC OS file types are in the zip: tools/rozip.py runs on it; programs are 0755) and SHA256SUMS.  An existing OUTDIR/<package> is not touched: remove it yourself."""
import argparse, hashlib, os, shutil, sys, zipfile

ap = argparse.ArgumentParser()
ap.add_argument("kind", choices=("native", "native32"))
ap.add_argument("tree"); ap.add_argument("outdir")
ap.add_argument("--version", required=True)
ap.add_argument("--launcher", required=True); ap.add_argument("--ar", required=True); ap.add_argument("--zlib", required=True)
ap.add_argument("--repo", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
a = ap.parse_args()
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(a.repo)
NAME = "riscos64-%s-%s" % (a.kind, a.version)
D = os.path.join(a.outdir, NAME)
if os.path.exists(D):
    sys.exit("%s exists: remove it yourself" % D)

def put_text(rel, text):
    p = os.path.join(D, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    open(p, "w", newline="\n").write(text)

def put_file(src, rel):
    p = os.path.join(D, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    shutil.copyfile(src, p)

shutil.copytree(a.tree, D, symlinks=False)
put_file(a.ar, "bin/ar,ff8")

# ---- zlib
Z = a.zlib
CSRC = "adler32 compress crc32 deflate gzclose gzlib gzread gzwrite infback inffast inflate inftrees trees uncompr zutil".split()
HDR = "crc32 deflate gzguts inffast inffixed inflate inftrees trees zconf zlib zutil".split()
for n in CSRC:
    put_file(os.path.join(Z, n + ".c"), "zl/c/" + n)
put_file(os.path.join(Z, "test", "example.c"), "zl/c/example")
for n in HDR:
    t = open(os.path.join(Z, n + ".h"), newline="").read()
    if n == "zconf":                     # what ./configure does to the two tests: unistd.h and stdarg.h are there
        for sym in ("HAVE_UNISTD_H", "HAVE_STDARG_H"):
            old = "#ifdef %s    /* may be set to #if 1 by ./configure */" % sym
            assert t.count(old) == 1, "zconf.h: " + sym
            t = t.replace(old, "#if 1    /* was set to #if 1 by ./configure */")
    p = os.path.join(D, "zl", "h", n)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    open(p, "w", newline="").write(t)
for d in ("o", "out"):
    put_text("zl/%s/keep" % d, "keep\n")
ALL = ["example"] + CSRC                 # the order of the response file: the object with main first
if a.kind == "native":
    put_text("zl/link,fff", "-munixlib -o zl/out/zexample " + " ".join("zl/o/" + n for n in ALL) + "\n")
else:
    put_text("zl/link,fff", " ".join("zl/o/" + n for n in ALL) + " -o zl/out/zexample32\n")

# ---- scripts
for f in ("Clean,feb",):
    put_file(os.path.join(HERE, "native", "scripts", f), "scripts/" + f)
lines = []
if a.kind == "native":
    put_file(os.path.join(HERE, "native", "scripts", "RunNatCxx,feb"), "scripts/RunNatCxx,feb")
    launchers = ["RunNatCxx", "Run64Zlib"]
    lines.append("| Run64Zlib - zlib 1.3.1 built on RISC OS 64 with the native C compiler and run.  double-click Run64Zlib   (about 20 seconds)")
    lines.append("| If a script stops with an error, send the text on the screen.")
else:
    put_file(os.path.join(HERE, "..", "hosted64", "scripts", "RunTo32,feb"), "scripts/RunTo32,feb")
    launchers = ["RunTo32", "RunZlib32"]
    lines.append("| RunZlib32 - zlib 1.3.1 compiled for 32 bit RISC OS on RISC OS 64; the program is zl.out.zexample32 (run it on a 32 bit machine).  double-click RunZlib32")
if a.kind == "native":
    lines.append("| the native compiler needs a big application slot")
lines.append("WimpSlot -min 128M -max 128M")
if a.kind == "native":
    lines.append("Echo ===== compile (one native compiler run for each source file)")
for n in sorted(ALL):
    lines.append("Obey Clean")
    lines.append("<T64$Dir>.xgcc -B bin/ -B lib/ -L lib -I zl -isystem include -O2 -x c -c zl/c/%s -o zl/o/%s" % (n, n))
lines.append("Echo ===== link (the objects go into an archive first)")
lib = CSRC
lines.append("<T64$Dir>.bin.ar rcs zl/out/libz.a " + " ".join("zl/o/" + n for n in lib[:8]))
lines.append("<T64$Dir>.bin.ar rcs zl/out/libz.a " + " ".join("zl/o/" + n for n in lib[8:]))
lines.append("Obey Clean")
if a.kind == "native":
    lines.append("<T64$Dir>.xgcc -B bin/ -B lib/ -L lib -munixlib zl/o/example zl/out/libz.a -o zl/out/zexample")
    lines.append("SetType <T64$Dir>.zl.out.zexample &FF8")
    lines.append("Echo ===== run the zlib test program")
    lines.append("Echo (the last line should be: inflate with dictionary: hello, hello!)")
    lines.append("<T64$Dir>.zl.out.zexample")
else:
    lines.append("<T64$Dir>.xgcc -B bin/ -B lib/ -L lib zl/o/example zl/out/libz.a -o zl/out/zexample32")
    lines.append("Echo ===== now run zl.out.zexample32 on a 32 bit machine")
    lines.append("Echo (the last line should be: inflate with dictionary: hello, hello!)")
lines.append("Echo Finished.")
put_text("scripts/" + launchers[1] + ",feb", "\n".join(lines) + "\n")
for l in launchers:
    put_file(a.launcher, l + ",ff8")

# ---- ReadMe and licences
rm = open(os.path.join(HERE, "native", "ReadMe-%s.txt" % a.kind)).read().replace("@VERSION@", a.version)
put_text("ReadMe", rm)
LIC = os.path.join(REPO, "licenses")
for f in ("GPL-3.0", "LGPL-2.0", "Apache-2.0", "UnixLib-COPYING", "GCC-Runtime-Library-Exception-3.1"):
    put_file(os.path.join(LIC, f + ".txt"), "Licences/" + f)
put_file(os.path.join(REPO, "LICENSES.md"), "Licences/LICENSES-notes")
put_file(os.path.join(Z, "LICENSE"), "Licences/zlib-LICENSE")

# ---- the zip
zp = os.path.join(a.outdir, NAME + ".zip")
if os.path.exists(zp):
    os.remove(zp)
n = 0
with zipfile.ZipFile(zp, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for root, dirs, files in os.walk(D):
        dirs.sort()
        for f in sorted(files):
            p = os.path.join(root, f)
            zi = zipfile.ZipInfo.from_file(p, NAME + "/" + os.path.relpath(p, D).replace(os.sep, "/"))
            zi.compress_type = zipfile.ZIP_DEFLATED
            zi.external_attr = (0o100755 if f.endswith(",ff8") else 0o100644) << 16
            z.writestr(zi, open(p, "rb").read())
            n += 1
import subprocess
subprocess.check_call([sys.executable, os.path.join(HERE, "..", "..", "tools", "rozip.py"), "--in-place", zp, "--default-type", "fff"])   # the ,xxx suffixes become the file type of each entry
sha = hashlib.sha256(open(zp, "rb").read()).hexdigest()
open(os.path.join(a.outdir, NAME + ".sha256"), "w").write("%s  %s.zip\n" % (sha, NAME))
print("%s: %d files, %.1f MB, sha256 %s" % (zp, n, os.path.getsize(zp) / 1e6, sha))
