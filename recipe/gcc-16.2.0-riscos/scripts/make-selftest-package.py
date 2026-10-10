#!/usr/bin/env python3
"""Build the PackMan package Gcc16SelfTest_16.2.0-<REL>_arm.zip: the self-test of tests/selftest as the application !GCC16Test, with RISC OS file types.

usage: make-selftest-package.py [REL]        (default 19)      output directory: $PKG_OUT, default ../../../release
Files get RISC OS types through the Info-ZIP "ARC0" extra field (Obey &FEB for !Boot, !Run and RunSelfTest, Text &FFF for the rest); see make-native-package.py.
"""
import os, struct, sys, time, zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pkgmeta

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.normpath(os.path.join(HERE, "..", "tests", "selftest"))           # the author's work area
if not os.path.isdir(SRC):
    SRC = os.path.normpath(os.path.join(HERE, "..", "..", "..", "tests", "selftest"))      # this repository: the tests of the recipe are published in tests/
REL = sys.argv[1] if len(sys.argv) > 1 else "19"
VER = "16.2.0-" + REL
OUT = os.environ.get("PKG_OUT") or os.path.normpath(os.path.join(HERE, "..", "..", "..", "release"))
os.makedirs(OUT, exist_ok=True)
PATH = os.path.join(OUT, "Gcc16SelfTest_%s_arm.zip" % VER)
APP = "Apps/Utilities/!GCC16Test/"

def riscos_stamp(unix):
    cs = int((unix + 2208988800) * 100)
    return (cs >> 32) & 0xFF, cs & 0xFFFFFFFF

def extra(ftype, unix, attr=0x13):
    hi, lo = riscos_stamp(unix)
    load = 0xFFF00000 | (ftype << 8) | hi
    ac = b"ARC0" + struct.pack("<III", load, lo, attr) + b"\0\0\0\0"
    return struct.pack("<HH", 0x4341, len(ac)) + ac + struct.pack("<HHBI", 0x5455, 5, 1, int(unix))

NOW = time.time()
BOOT = """| !Boot file of the self-test for the native GCC 16 tool chain
Set GCC16Test$Dir <Obey$Dir>
Set GCC16Test$Help <GCC16Test$Dir>.ReadMe
Set GCC16Test$Version "%s"
""" % VER
RUN = """| !Run file of the self-test: it only sets up GCC16Test$Dir.  The test is run in a Task window:   Obey <GCC16Test$Dir>.RunSelfTest
Run <Obey$Dir>.!Boot
Filer_Run <GCC16Test$Help>
"""
HELP = """| !Help file of the self-test: it shows the ReadMe
Run <Obey$Dir>.!Boot
Filer_Run <GCC16Test$Help>
"""
README = """Self-test for the native GCC 16 tool chain (Gcc16 %(v)s)
=====================================================
It compiles and runs small C, C++ and Fortran programs, a two-file project, a make build, two -flto builds, a coverage run (--coverage and gcov), a profile-guided build, a gprof run (-pg) and, from 16.2.0-14, a relocatable module (cmunge, -mmodule, load, run, remove), and checks that a compile error is reported.
Every program checks itself: a PASS means the compiler made a program that ran correctly.  About half a minute.

  1. Install the packages with PackMan (SharedLibs-C-armeabihf, then Gcc16), REBOOT, and double-click !GCC16 once.
  2. Open the folder that contains !GCC16Test once (the Filer sets GCC16Test$Dir).
  3. Open a Task window (Ctrl-F12) and type:      Obey <GCC16Test$Dir>.RunSelfTest

The work is done on the local disc (<Wimp$ScrapDir>.GCC16Test); the output is also written to Results here.  The last line says
  SELFTEST: ALL CHECKS PASSED
or lists the failed checks.  If something fails send the Results file with a bug report: https://github.com/robheaton/riscos-gcc16/issues
""" % {"v": VER}

entries = []   # (zip name, data, ftype, mode)
def add(name, data, ftype, mode=0o100664):
    entries.append((name, data, ftype, mode))
add(APP + "!Boot", BOOT.encode(), 0xFEB)
add(APP + "!Run", RUN.encode(), 0xFEB)
add(APP + "!Help", HELP.encode(), 0xFEB)
add(APP + "ReadMe", README.encode(), 0xFFF)
add(APP + "RunSelfTest", open(os.path.join(SRC, "RunSelfTest,feb"), "rb").read(), 0xFEB)
add(APP + "Makefile", open(os.path.join(SRC, "Makefile"), "rb").read(), 0xFFF)
for d in ("c", "cc", "cmhg", "f90", "h"):
    for f in sorted(os.listdir(os.path.join(SRC, d))):
        add(APP + d + "/" + f, open(os.path.join(SRC, d, f), "rb").read(), 0xFFF)
add("RiscPkg/Control", pkgmeta.control("Gcc16SelfTest", VER, "GPL",
    "Self-test for the native GCC 16 tool chain: compiles and runs small C, C++ and Fortran programs, a make build, -flto builds, a coverage run with gcov, a profile-guided build, a gprof run and a module that is built, loaded and run",
    depends="Gcc16 (>= %s)" % VER, components="Apps.Utilities.!GCC16Test (Movable LookAt)").encode(), 0xFFF)
add("RiscPkg/Copyright", ("The self-test of the native GCC 16 tool chain (Gcc16SelfTest %s): small test programs and an Obey file, GNU General Public License, version 3 or later.\n"
                          "Source and documentation: %s (tests/selftest).\n%s\n" % (VER, pkgmeta.REPO, pkgmeta.REPORT)).encode(), 0xFFF)

dirs = set()
for name, _, _, _ in entries:
    d = os.path.dirname(name)
    while d:
        dirs.add(d)
        d = os.path.dirname(d)
with zipfile.ZipFile(PATH, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for d in sorted(dirs):
        zi = zipfile.ZipInfo(d + "/", date_time=time.localtime(NOW)[:6])
        zi.create_system = 3
        zi.external_attr = (0o40775 << 16) | 0x10
        zi.extra = struct.pack("<HHBI", 0x5455, 5, 1, int(NOW))
        z.writestr(zi, b"")
    for name, data, ftype, mode in entries:
        zi = zipfile.ZipInfo(name, date_time=time.localtime(NOW)[:6])
        zi.create_system = 3
        zi.external_attr = mode << 16
        zi.compress_type = zipfile.ZIP_DEFLATED
        zi.extra = extra(ftype, NOW)
        z.writestr(zi, data)
chk = zipfile.ZipFile(PATH)
assert chk.testzip() is None
print("wrote %s: %d files, %d bytes" % (PATH, len(entries), os.path.getsize(PATH)))
