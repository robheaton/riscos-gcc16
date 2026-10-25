#!/usr/bin/env python3
"""package-hosted32.py TREE OUT.zip [VERSION]: packs the AArch64 cross compiler for 32 bit RISC OS (TREE = tree-ro/gcc16a64r made by stage-tree.sh) as the application !GCC64 in a zip with RISC OS file types
(Info-ZIP ARC0 extra field): executables (,e1f in the tree) &E1F, Obey files &FEB, the rest &FFF.  Adds !Boot, !Run, !Help and docs/ReadMe.  The names are already in the RISC OS layout (stdio.h is h/stdio)."""
import os, struct, sys, time, zipfile

TREE = sys.argv[1] if len(sys.argv) > 1 else sys.exit(__doc__)
OUT = sys.argv[2] if len(sys.argv) > 2 else sys.exit(__doc__)
VER = sys.argv[3] if len(sys.argv) > 3 else "0.1"
APP = "!GCC64/"
SFIX = "f:for:F:f90:F90:f95:F95:f03:F03:f08:F08:fpp:cc:cxx:cpp:c++:C:i:ii:rpo:c:m:h:hh:s:S:xrb:xrs:l:o:y:tcc:cmhg:adb:ads:ali"
NOW = time.time()

def extra(ftype, unix):
    cs = int((unix + 2208988800) * 100)
    hi, lo = (cs >> 32) & 0xFF, cs & 0xFFFFFFFF
    load = 0xFFF00000 | (ftype << 8) | hi
    ac = b"ARC0" + struct.pack("<III", load, lo, 0x13) + b"\0\0\0\0"
    return struct.pack("<HH", 0x4341, len(ac)) + ac + struct.pack("<HHBI", 0x5455, 5, 1, int(unix))

entries, tools = [], set()
for dp, dn, fn in os.walk(TREE):
    for f in sorted(fn):
        p = os.path.join(dp, f)
        rel = os.path.relpath(p, TREE)
        if rel == "Setup,feb":
            continue                                   # replaced by !Run
        ftype = 0xFFF
        if rel.endswith(",e1f"):
            rel, ftype = rel[:-4], 0xE1F
            tools.add(os.path.basename(rel))
        elif rel.endswith(",feb"):
            rel, ftype = rel[:-4], 0xFEB
        entries.append((APP + rel, open(p, "rb").read(), ftype))

boot = "| !Boot file for the AArch64 (RISC OS 64) cross compiler that runs on 32 bit RISC OS\nSet GCC64$Dir <Obey$Dir>\nSet GCC64$Version \"%s\"\nSet GCC64$Title \"GCC 64\"\n" % VER
run = ["| !Run file for the AArch64 (RISC OS 64) cross compiler that runs on 32 bit RISC OS", "Run <Obey$Dir>.!Boot", "",
       "| Search path for the programs: the drivers (aarch64-riscos-gcc, -g++) and the tools (cmunge64, mkoslib, as, ld ...)",
       'If "<GCC64bin$Path>" = "" Then Set Run$Path <Run$Path>,GCC64bin:,GCC64tbin:',
       "Set GCC64bin$Path <GCC64$Dir>.bin.", "Set GCC64tbin$Path <GCC64$Dir>.aarch64-riscos.bin.", "",
       "| The return code limit: UnixLib encodes information within this range, so a failing compile must not be reported as a RISC OS error.",
       "Set Sys$RCLimit 65536", "",
       "| Filename translation (foo.c <-> c.foo, foo.h <-> h.foo, foo.o <-> o.foo ...) for every program of the tool chain",
       'Set UnixEnv$gcc$sfix "%s"' % SFIX]
for t in sorted(tools):
    run.append("Set UnixEnv$%s$sfix <UnixEnv$gcc$sfix>" % t)
run += ["", "| SharedUnixLibrary 1.12 or later (SharedLibs-C-armeabihf: the Gcc16 package has it)",
        "RMEnsure SharedUnixLibrary 1.12 RMLoad System:Modules.SharedULib",
        "RMEnsure SharedUnixLibrary 1.12 Error The GCC 64 tool chain requires SharedUnixLibrary 1.12 or later", ""]
help_ = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "ReadMe-app.txt")).read().replace("@VERSION@", VER)
entries += [(APP + "!Boot", boot.encode(), 0xFEB), (APP + "!Run", "\n".join(run).encode(), 0xFEB), (APP + "!Help", help_.encode(), 0xFEB if False else 0xFFF),
            (APP + "docs/ReadMe", help_.encode(), 0xFFF)]
dirs = set()
for name, data, ft in entries:
    d = os.path.dirname(name)
    while d:
        dirs.add(d); d = os.path.dirname(d)
os.makedirs(os.path.dirname(os.path.abspath(OUT)), exist_ok=True)
with zipfile.ZipFile(OUT, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for d in sorted(dirs):
        zi = zipfile.ZipInfo(d + "/", date_time=time.localtime(NOW)[:6]); zi.create_system = 3
        zi.external_attr = (0o40775 << 16) | 0x10; zi.extra = struct.pack("<HHBI", 0x5455, 5, 1, int(NOW)); z.writestr(zi, b"")
    for name, data, ft in entries:
        zi = zipfile.ZipInfo(name, date_time=time.localtime(NOW)[:6]); zi.create_system = 3
        zi.external_attr = 0o100664 << 16; zi.compress_type = zipfile.ZIP_DEFLATED; zi.extra = extra(ft, NOW); z.writestr(zi, data)
assert zipfile.ZipFile(OUT).testzip() is None
print("wrote", OUT, os.path.getsize(OUT), "bytes,", len(entries), "files")
