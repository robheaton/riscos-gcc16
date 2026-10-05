#!/usr/bin/env python3
"""Build SharedLibs-Fortran-armeabihf_16.2.0-<REL>_arm.zip: the GCC 16.2 libgfortran.so.5.0.0 built/linked with binutils 2.45.1
(input: the in-tree build, stripped here with the new strip).  Same RiscPkg layout as make-cxx-package.py.
usage: make-fortran-package.py [REL] [SRC]   (defaults: 1, build-f/arm-riscos-gnueabihf/libgfortran/.libs/libgfortran.so.5.0.0)"""
import os, struct, subprocess, sys, time, zipfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pkgmeta
HOME = os.path.expanduser("~")
OUT = os.environ.get("PKG_OUT", HOME + "/gccsdk-next/release").rstrip("/") + "/"
os.makedirs(OUT, exist_ok=True)
STRIP = HOME + "/gccsdk-next/binutils-2.45.1-install/bin/arm-riscos-gnueabihf-strip"
REL = sys.argv[1] if len(sys.argv) > 1 else "1"
SRC = sys.argv[2] if len(sys.argv) > 2 else HOME + "/gccsdk-next/build-f/arm-riscos-gnueabihf/libgfortran/.libs/libgfortran.so.5.0.0"

def riscos_stamp(unix):
    cs = int((unix + 2208988800) * 100)
    return (cs >> 32) & 0xFF, cs & 0xFFFFFFFF
def extra(ftype, unix, attr=0x13):
    hi, lo = riscos_stamp(unix)
    load = 0xFFF00000 | (ftype << 8) | hi
    ac = b"ARC0" + struct.pack("<III", load, lo, attr) + b"\0\0\0\0"
    return struct.pack("<HH", 0x4341, len(ac)) + ac + struct.pack("<HHBI", 0x5455, 5, 1, int(unix))
def zinfo(name, ftype, unix, mode, directory=False):
    t = time.gmtime(unix)
    zi = zipfile.ZipInfo(name + ("/" if directory else ""), date_time=t[:6])
    zi.create_system = 3
    zi.external_attr = (mode << 16) | (0x10 if directory else 0)
    zi.compress_type = zipfile.ZIP_STORED if directory else zipfile.ZIP_DEFLATED
    if not directory: zi.extra = extra(ftype, unix)
    return zi
def link(t): return b"LINK" + struct.pack("<I", len(t.replace(".", "/"))) + t.replace(".", "/").encode()
LICENCE = "GNU General Public License version 3 or later with the GCC Runtime Library Exception"
SOURCE = "GCC 16.2.0 (libgfortran) from the GNU project, https://ftp.gnu.org/gnu/gcc/gcc-16.2.0/, with the patches of the recipe (patches-fortran)"
NOTE = {"2": ("\n\nlibgfortran.so.5.0.0 changes since 16.2.0-1: built with -fstack-clash-protection (the new default of the compiler: every function with a stack frame touches the "
              "pages of its frame, so no page of the lazily mapped EABI stack is first touched by RISC OS or by a 64-byte store); the code is otherwise the same.")}.get(REL, "")
tmp = OUT + "libgfortran.so.5.0.0"
subprocess.run(["cp", SRC, tmp], check=True)
subprocess.run([STRIP, "--strip-unneeded", tmp], check=True)
base = "Resources/!SharedLibs/lib/armeabihf/"
entries = [("f", base + "libgfortran.so", link("libgfortran.so.5.0.0"), 0x1C8, 0o100664),
           ("f", base + "libgfortran.so.5", link("libgfortran.so.5.0.0"), 0x1C8, 0o100664),
           ("f", base + "libgfortran.so.5.0.0", open(tmp, "rb").read(), 0xE1F, 0o100755),
           ("f", "RiscPkg/Control", pkgmeta.control("SharedLibs-Fortran-armeabihf", "16.2.0-" + REL, "Free", pkgmeta.DESCRIPTION["SharedLibs-Fortran-armeabihf"]).encode(), 0xFFF, 0o100664),
           ("f", "RiscPkg/Copyright", pkgmeta.copyright_runtime("SharedLibs-Fortran-armeabihf", "16.2.0-" + REL, "libgfortran 5.0.0 for EABI (arm-riscos-gnueabihf) programs", LICENCE, SOURCE, NOTE).encode(), 0xFFF, 0o100664)]
path = OUT + "SharedLibs-Fortran-armeabihf_16.2.0-" + REL + "_arm.zip"
now = time.time()
with zipfile.ZipFile(path, "w") as z:
    seen = set()
    for kind, name, data, ftype, mode in entries:
        parts = name.split("/")
        for i in range(1, len(parts)):
            d = "/".join(parts[:i])
            if d not in seen:
                z.writestr(zinfo(d, 0xFFD, now, 0o40775, True), b""); seen.add(d)
        z.writestr(zinfo(name, ftype, now, mode), data)
print("wrote", path, os.path.getsize(path), "bytes")
z = zipfile.ZipFile(path); assert z.testzip() is None
for zi in z.infolist():
    if zi.filename.endswith("/"): continue
    ex = zi.extra; ft = "-"
    if ex[:4] == b"AC\x14\x00":
        load = struct.unpack("<I", ex[8:12])[0]; ft = "%03X" % ((load >> 8) & 0xFFF)
    print("  %-60s type=%-4s %9d bytes" % (zi.filename.replace("Resources/!SharedLibs/lib/", ""), ft, zi.file_size))
