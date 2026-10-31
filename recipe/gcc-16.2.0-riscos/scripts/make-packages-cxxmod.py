#!/usr/bin/env python3
"""Make Gcc16_16.2.0-25_arm.zip (C++ modules with the native compiler) from the TESTED Gcc16 package of 16.2.0-24.

usage: make-packages-cxxmod.py OLD_DIR KIT OUT_DIR

OLD_DIR holds Gcc16_16.2.0-24_arm.zip (the file of the release), KIT is the module kit (modkit/ of this repository: include-cxx/ is read from it), OUT_DIR gets Gcc16_16.2.0-25_arm.zip.
The self-test package of 16.2.0-25 is made by make-selftest-package.py 25 (its sources, tests/selftest, have the new check 13 and the C++ module).

No program of the tool chain changes: the compiler, the binutils, make and the libraries are the bytes of 16.2.0-24, so the compiler was not built again.  What changes:
  * lib/gcc/arm-riscos-gnueabihf/16.2.0/include-cxx/   the four files of the kit's C++ headers (modkit/include-cxx: <cmath>, <math.h>, bits/c++config.h, bits/gthr-default.h), in the RISC OS layout
  * lib/gcc/arm-riscos-gnueabihf/16.2.0/modcxx         a spec file: g++ -mmodule -specs=modcxx makes the options that a C++ module needs (the kit's headers and libstdc++'s first, hosted, no exceptions,
                                                       RTTI or thread-safe statics, sections for the linker's garbage collection), given from the compiler's own prefix so that the package can be anywhere
  * RiscPkg/Control, RiscPkg/Copyright, !Boot, docs/ReadMe: the version, and the text of the module section (C++ modules are built here, no longer only on Linux)
"""
import os, re, struct, sys, time, zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pkgmeta

OLD, KIT, OUT = (sys.argv[1:4] + [None, None, None])[:3]
if not (OLD and KIT and OUT):
    sys.exit(__doc__)
OLDV, NEWV = "16.2.0-24", "16.2.0-25"
T, V = "arm-riscos-gnueabihf", "16.2.0"
APP = "Apps/Utilities/!GCC16/"
TST = "Apps/Utilities/!GCC16Test/"
LIBD = APP + "lib/gcc/%s/%s/" % (T, V)
NOW = time.time()


def riscos_stamp(unix):
    cs = int((unix + 2208988800) * 100)
    return (cs >> 32) & 0xFF, cs & 0xFFFFFFFF


def extra(ftype, unix, attr=0x13):   # Info-ZIP "AC" (ARC0) field: RISC OS load/exec (file type + date) and attributes, plus the Unix time field (as make-native-package.py)
    hi, lo = riscos_stamp(unix)
    load = 0xFFF00000 | (ftype << 8) | hi
    ac = b"ARC0" + struct.pack("<III", load, lo, attr) + b"\0\0\0\0"
    return struct.pack("<HH", 0x4341, len(ac)) + ac + struct.pack("<HHBI", 0x5455, 5, 1, int(unix))


def replace1(text, old, new, what):
    assert text.count(old) == 1, "%s: expected one place to change, found %d" % (what, text.count(old))
    return text.replace(old, new, 1)


def write_package(src_zip, dst, changed, added):
    """Copy every entry of src_zip (bytes and zip attributes unchanged) except those in CHANGED (new data, same attributes); then add ADDED = [(name, data, file type)]."""
    src = zipfile.ZipFile(src_zip)
    names = set(src.namelist())
    dirs = set()
    for name, data, ftype in added:
        d = os.path.dirname(name)
        while d:
            if d + "/" not in names:
                dirs.add(d + "/")
            d = os.path.dirname(d)
    with zipfile.ZipFile(dst, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for zi in src.infolist():
            data = b"" if zi.filename.endswith("/") else src.read(zi.filename)
            if zi.filename in changed:
                data = changed[zi.filename]
            n = zipfile.ZipInfo(zi.filename, date_time=zi.date_time)
            n.create_system, n.external_attr, n.compress_type, n.extra, n.comment = zi.create_system, zi.external_attr, zi.compress_type, zi.extra, zi.comment
            z.writestr(n, data)
        for d in sorted(dirs):
            zi = zipfile.ZipInfo(d, date_time=time.localtime(NOW)[:6])
            zi.create_system = 3
            zi.external_attr = (0o40775 << 16) | 0x10
            zi.extra = struct.pack("<HHBI", 0x5455, 5, 1, int(NOW))
            z.writestr(zi, b"")
        for name, data, ftype in added:
            zi = zipfile.ZipInfo(name, date_time=time.localtime(NOW)[:6])
            zi.create_system = 3
            zi.external_attr = 0o100664 << 16
            zi.compress_type = zipfile.ZIP_DEFLATED
            zi.extra = extra(ftype, NOW)
            z.writestr(zi, data)
    out = zipfile.ZipFile(dst)
    assert out.testzip() is None
    return out


# ------------------------------------------------------------------ Gcc16
GCC_OLD = os.path.join(OLD, "Gcc16_%s_arm.zip" % OLDV)
g = zipfile.ZipFile(GCC_OLD)
assert re.search(r"^Version: %s$" % OLDV, g.read("RiscPkg/Control").decode(), re.M), "not the package of %s" % OLDV
assert g.read("RiscPkg/Copyright").decode() == pkgmeta.copyright_gcc16(OLDV), "RiscPkg/Copyright of %s is not what pkgmeta.py makes: look at the differences first" % OLDV

control = re.sub(r"^Version: .*$", "Version: %s" % NEWV, g.read("RiscPkg/Control").decode(), flags=re.M)
boot = replace1(g.read(APP + "!Boot").decode(), 'Set GCC16$Version "%s"' % OLDV, 'Set GCC16$Version "%s"' % NEWV, "!Boot")
readme = g.read(APP + "docs/ReadMe").decode()
# the module section: the C++ recipe after the C one, and the 16.2.0-25 entry before the 16.2.0-24 one
anchor = "  RMLoad MyModule                                                  (save your work first: a mistake in a module can crash the machine)\n"
cxx = (anchor +
       "  C++ modules (new in 16.2.0-25): put  module-is-c-plus-plus:  in the CMHG file, compile with the C++ driver and the spec file modcxx, and link with the C driver (the C++ driver would add libstdc++ and libm):\n"
       "  g++ -mmodule -specs=modcxx -std=gnu++17 -O2 -c main.cc           modcxx (!GCC16.lib.gcc....modcxx) puts the kit's C++ headers (!GCC16.lib.gcc....include-cxx) and libstdc++'s headers first, says\n"
       "                                                                   hosted, and adds -fno-exceptions -fno-rtti -fno-threadsafe-statics -ffunction-sections -fdata-sections\n"
       "  gcc -mmodule -o MyModule main.o header.o                         (header.o from cmunge as above)\n"
       "  These are the steps of the Linux cross compiler: a module built here is byte for byte the module that it makes from the same source.\n")
readme = replace1(readme, anchor, cxx, "docs/ReadMe (module recipe)")
new25 = ("  New in 16.2.0-25: C++ modules are built here too (they were built only with the Linux cross compiler before): the package has the kit's C++ headers (lib/gcc/%s/%s/include-cxx) and the spec file\n"
         "  modcxx, and Gcc16SelfTest has a thirteenth check that builds, loads, runs and removes a module written in C++.  The compiler, the binutils, make and the libraries are the files of 16.2.0-24.\n" % (T, V))
readme = replace1(readme, "  New in 16.2.0-24: the runtime", new25 + "  New in 16.2.0-24: the runtime", "docs/ReadMe (new in)")
old17 = "no exceptions, RTTI, streams or threads; built with the cross compiler: the C++ headers are not in this\n  package), and a correction"
new17 = "no exceptions, RTTI, streams or threads; built with the cross compiler, and since 16.2.0-25 here\n  as well: see above), and a correction"
readme = replace1(readme, old17, new17, "docs/ReadMe (16.2.0-17 entry)")

kit_files = []                       # (zip name, data, file type): the kit's include-cxx in the RISC OS layout (a .h goes into the folder h; <cmath> has no extension)
KC = os.path.join(KIT, "include-cxx")
for root, ds, fs in os.walk(KC):
    for f in sorted(fs):
        rel = os.path.relpath(os.path.join(root, f), KC)
        d, leaf = os.path.split(rel)
        if leaf.endswith(".h"):
            rel = os.path.join(d, "h", leaf[:-2])
        kit_files.append((LIBD + "include-cxx/" + rel.replace(os.sep, "/"), open(os.path.join(root, f), "rb").read(), 0xFFF))
assert len(kit_files) == 4, "the kit has %d files in include-cxx (expected 4: cmath, math.h, bits/c++config.h, bits/gthr-default.h)" % len(kit_files)
SPEC = """# modcxx: the options that make  g++ -mmodule  build a C++ module (new in 16.2.0-25): add  -specs=modcxx  to the g++ command (the file is found in this folder).
# The kit's C++ headers (include-cxx) and libstdc++'s headers come first; -mmodule says freestanding but libstdc++'s headers need a hosted compiler; a module has no exceptions, RTTI or
# thread-safe statics (its one thread of control makes the guard of a local static a barrier instruction that ARMv6 lacks); sections let the module's link drop what nothing calls.
# The folders are written from the compiler's own prefix (lib/gcc/<target>/<version>/), so the package can be installed anywhere.
*cc1plus:
+ %{mmodule:-U__STDC_HOSTED__ -D__STDC_HOSTED__=1 -iwithprefixbefore include-cxx -iwithprefixbefore ../../../../include/c++/16.2.0 -iwithprefixbefore ../../../../include/c++/16.2.0/arm-riscos-gnueabihf -iwithprefixbefore ../../../../include/c++/16.2.0/backward -fno-exceptions -fno-rtti -fno-threadsafe-statics -ffunction-sections -fdata-sections}
"""
added = kit_files + [(LIBD + "modcxx", SPEC.encode(), 0xFFF)]
changed = {"RiscPkg/Control": control.encode(), "RiscPkg/Copyright": pkgmeta.copyright_gcc16(NEWV).encode(), APP + "!Boot": boot.encode(), APP + "docs/ReadMe": readme.encode()}
os.makedirs(OUT, exist_ok=True)
gout = os.path.join(OUT, "Gcc16_%s_arm.zip" % NEWV)
write_package(GCC_OLD, gout, changed, added)
print("wrote", gout, os.path.getsize(gout), "bytes:", len(added), "files added,", len(changed), "changed")
