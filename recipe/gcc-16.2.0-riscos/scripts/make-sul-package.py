#!/usr/bin/env python3
"""Build the PackMan package SharedULibFix_1.16-vforkfix3_arm.zip: the fixed SharedUnixLibrary module with the tools that install it, undo it and check it, as the application !SULFix.

usage: make-sul-package.py [SULBUILD]       SULBUILD = the output directory of build-sul.sh (default ~/gccsdk-next/sul-build); the module is SharedULib-116fix3,ffa there
       output directory: $PKG_OUT, default ../../../release.     the C programs of recipe/gcc-16.2.0-riscos/sulfix are compiled with the cross compiler of $GCCNEXT/env-f (default ~/gccsdk-next).
The package installs nothing into the system by itself: it puts the folder !SULFix in Apps.Utilities, and the system module is replaced only when the user runs Install in a Task window.
Files get RISC OS types through the Info-ZIP "ARC0" extra field (Obey &FEB, module &FFA, ELF &E1F, Text &FFF); see make-selftest-package.py."""
import os, struct, subprocess, sys, tempfile, time, zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pkgmeta
from pkgmeta import REPO, REPORT

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.normpath(os.path.join(HERE, "..", "sulfix"))
VER = "1.16-vforkfix3"
MODULE = "SharedULib-116fix3,ffa"
MOD_SIZE, MOD_FNV = 3228, 0xefc7dfea                      # the module that passed RunSul8 on the machine (must be the one in the table of sulfile.c)
SULB = os.path.expanduser(sys.argv[1]) if len(sys.argv) > 1 else os.path.expanduser("~/gccsdk-next/sul-build")
OUT = os.environ.get("PKG_OUT") or os.path.normpath(os.path.join(HERE, "..", "..", "..", "release"))
GN = os.environ.get("GCCNEXT") or os.path.expanduser("~/gccsdk-next")
CCDIR = os.path.join(GN, "env-f", "bin")
os.makedirs(OUT, exist_ok=True)
PATH = os.path.join(OUT, "SharedULibFix_%s_arm.zip" % VER)
APP = "Apps/Utilities/!SULFix/"
NOW = float(os.environ["SOURCE_DATE_EPOCH"]) if os.environ.get("SOURCE_DATE_EPOCH") else time.time()


def fnv1a(b):
    h = 0x811c9dc5
    for c in b:
        h = ((h ^ c) * 0x01000193) & 0xFFFFFFFF
    return h


def riscos_stamp(unix):
    cs = int((unix + 2208988800) * 100)
    return (cs >> 32) & 0xFF, cs & 0xFFFFFFFF


def extra(ftype, unix, attr=0x13):
    hi, lo = riscos_stamp(unix)
    load = 0xFFF00000 | (ftype << 8) | hi
    ac = b"ARC0" + struct.pack("<III", load, lo, attr) + b"\0\0\0\0"
    return struct.pack("<HH", 0x4341, len(ac)) + ac + struct.pack("<HHBI", 0x5455, 5, 1, int(unix))


def copyright_sulfix(version):
    """RiscPkg/Copyright of SharedULibFix (here and not in pkgmeta.py: this package is released separately from the compilers)."""
    return """SharedULibFix %s - the fixed SharedUnixLibrary module (a replacement for the system module SharedULib 1.16 of 3 Apr 2020), with the tools that install it, undo it and check it

The module (SharedULib) is built from module/sul.s of UnixLib (GCCSDK svn trunk r7800, svn://svn.riscos.info/gccsdk/trunk, gcc4/recipe/files/gcc/libunixlib) with the three patches
patches-unixlib/unixlib-sul-vfork-child-stack.patch, -slot.patch and -execed.patch of %s, by recipe/gcc-16.2.0-riscos/scripts/build-sul.sh (docs/BUILDING.md).
SharedUnixLibrary is part of UnixLib: Copyright (c) 2002-2020 UnixLib Developers, the revised BSD licence for most files; some files carry other BSD-style notices or the GNU Library General
Public Licence (see doc/UnixLib/COPYING in the UnixLib sources).  The patches are under the licence of the file they change.
The programs (sulfile, modver, fixlevel, vforkbare, vforkfail) and the Obey files (Install, Restore, Check) are under the GNU General Public License, version 3 or later; their source is in
recipe/gcc-16.2.0-riscos/sulfix of %s.
This replaces a system module: it was tested on one machine (Raspberry Pi Compute Module 4, RISC OS 5.30) and comes with no warranty.
%s
""" % (version, REPO, REPO, REPORT)


# the module: it must be exactly the one that the tool in the package calls "the FIXED module" (and that was tested on the machine)
mod = open(os.path.join(SULB, MODULE), "rb").read()
if len(mod) != MOD_SIZE or fnv1a(mod) != MOD_FNV:
    sys.exit("ERROR: %s is %d bytes, FNV-1a %08x: not the module that passed the hardware tests (%d bytes, %08x): run build-sul.sh" % (os.path.join(SULB, MODULE), len(mod), fnv1a(mod), MOD_SIZE, MOD_FNV))
sulfile_c = open(os.path.join(SRC, "sulfile.c")).read()
if "%d, 0x%08xu" % (MOD_SIZE, MOD_FNV) not in sulfile_c:
    sys.exit("ERROR: the table of sulfile.c does not have the module %d bytes, %08x" % (MOD_SIZE, MOD_FNV))

# the programs, built with the cross compiler like the test programs that ran on the machine (static libgcc: nothing but the runtime package is needed)
CC = os.path.join(CCDIR, "arm-riscos-gnueabihf-gcc")
STRIP = os.path.join(CCDIR, "arm-riscos-gnueabihf-strip")
tmp = tempfile.mkdtemp(prefix="sulfix-")
progs = {}
for name in ("sulfile", "modver", "fixlevel", "vforkbare", "vforkfail"):
    exe = os.path.join(tmp, name)
    subprocess.check_call([CC, "-std=gnu11", "-O2", "-g0", "-Wall", "-Wextra", "-Wno-unused-parameter", os.path.join(SRC, name + ".c"), "-static-libgcc", "-Wl,--allow-shlib-undefined", "-lm", "-o", exe])
    subprocess.check_call([STRIP, "--strip-all", exe])
    progs[name] = open(exe, "rb").read()

BOOT = """| !Boot file of SharedULibFix: it only sets variables
Set SULFix$Dir <Obey$Dir>
Set SULFix$Help <SULFix$Dir>.ReadMe
Set SULFix$Version "%s"
""" % VER
RUN = """| !Run file of SharedULibFix: it only sets up SULFix$Dir and shows the ReadMe.  The fixed module is installed by Install, in a Task window:   Obey <SULFix$Dir>.Install
Run <Obey$Dir>.!Boot
Filer_Run <SULFix$Help>
"""
HELP = """| !Help file of SharedULibFix: it shows the ReadMe
Run <Obey$Dir>.!Boot
Filer_Run <SULFix$Help>
"""
README = """SharedULibFix %(v)s - the fixed SharedUnixLibrary module
=============================================================
SharedUnixLibrary 1.16 (3 Apr 2020) is the module that every UnixLib program shares.  It has three bugs that show when a program starts a child with vfork () and the child ends
WITHOUT calling exec: the usual reaction to an exec that failed,   if (vfork () == 0) { execv (...); _exit (127); }   as a shell, GNU make or any program that runs commands does:
  1. the child frees the main stack of its PARENT: the parent dies ("Internal error: abort on data transfer");
  2. the child makes its parent's Wimp slot as big as it can be (96 MB became 512 MB);
  3. when the parent was itself started by exec (everything under make or a shell) the child deregisters the parent's Shared Object Manager client, which froze the machine in a loop test.
Ordinary compiles and make runs are not affected: only programs that fork children that fail to exec.
This package replaces that one system module by 1.16-vforkfix3, which has the three fixes (SharedULib 1.16-vforkfix3 (4 Oct 2026): the module file is the one in this folder).

IT REPLACES A SYSTEM MODULE: NOTHING IS CHANGED UNTIL YOU RUN Install.  Read this first.
  * It needs the runtime SharedLibs-C-armeabihf 16.2.0-13 (UnixLib fix level 10 or later: the fixed module needs the library's own fix for the same family of bugs; with an older runtime a loop of
    such children can corrupt memory and freeze the machine).  Install refuses an older runtime.
  * Install replaces the file only if System:Modules.SharedULib is the STOCK 1.16 (it compares the whole file, size and hash, with the module it was made for): the fixed module already
    installed, another version of SharedUnixLibrary or any other file: it stops and changes nothing.  It backs the stock module up twice (SharedULib-stock in this folder and SharedULib-stock next
    to the module), checks every copy, and puts the stock module back by itself if a check fails.
  * A module that is loaded stays as it is until the next boot, so you REBOOT after the install.
  * It was tested on one machine (Raspberry Pi Compute Module 4, RISC OS 5.30).  A system module that is wrong can freeze the machine: have your work saved.

HOW TO
  1. Install the package with PackMan, open the folder that contains !SULFix (Apps.Utilities) once.
  2. Open a Task window (Ctrl-F12) and type:           Obey <SULFix$Dir>.Install         (about 10 seconds)
  3. REBOOT.
  4. As the FIRST thing after the boot: open the folder that contains !SULFix (Apps.Utilities) once (the Filer sets SULFix$Dir again at every boot; that starts no UnixLib program), then in a Task window:
       Obey <SULFix$Dir>.Check        (is the module that is loaded the fixed one, and a vfork child that ends without exec: the parent carries on)
     Install printed the full name of Check at its end: you can type that instead of opening the folder.
TO GO BACK:   Obey <SULFix$Dir>.Restore   and REBOOT.      (If the folder is gone: copy SharedULib-stock, next to the module, over SharedULib, and reboot.)
A PackMan update of a package that ships the stock module puts the stock module back: run Install again then.  Removing this package removes only this folder: the module that is installed stays
(run Restore first if you want the stock module back).

THE FILES   Install, Restore, Check (Obey files: read them)   SharedULib (the module, file type &FFA)   sulfile (says which SharedUnixLibrary a file is, and where the file that System:Modules.SharedULib
finds really is)   modver (the module that is LOADED)   fixlevel (the runtime)   vforkbare, vforkfail (the tests of Check).
Source of the module (UnixLib's module/sul.s with three patches), of the programs and of the Obey files, and the reports: https://github.com/robheaton/riscos-gcc16 (docs/SHAREDULIB-FIX.md).
Problems: https://github.com/robheaton/riscos-gcc16/issues
""" % {"v": VER}

entries = []   # (zip name, data, ftype, mode)
def add(name, data, ftype, mode=0o100664):
    entries.append((name, data, ftype, mode))
add(APP + "!Boot", BOOT.encode(), 0xFEB)
add(APP + "!Run", RUN.encode(), 0xFEB)
add(APP + "!Help", HELP.encode(), 0xFEB)
add(APP + "ReadMe", README.encode(), 0xFFF)
for f in ("Install", "Restore", "Check"):
    add(APP + f, open(os.path.join(SRC, f + ",feb"), "rb").read(), 0xFEB)
add(APP + "SharedULib", mod, 0xFFA)
for name, data in sorted(progs.items()):
    add(APP + name, data, 0xE1F, 0o100775)
add("RiscPkg/Control", pkgmeta.control("SharedULibFix", VER, "Free",
    "The fixed SharedUnixLibrary module 1.16-vforkfix3 (a vfork child that ends without exec no longer harms its parent), with an installer that checks everything, a restore script and a check (experimental)",
    depends="SharedLibs-C-armeabihf (>= 16.2.0-13)", components="Apps.Utilities.!SULFix (Movable LookAt)").encode(), 0xFFF)
add("RiscPkg/Copyright", copyright_sulfix(VER).encode(), 0xFFF)

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
