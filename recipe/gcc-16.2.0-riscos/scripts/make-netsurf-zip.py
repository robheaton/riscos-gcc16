#!/usr/bin/env python3
"""make-netsurf-zip.py OUTDIR ZIP [README] - pack the application folder that build-netsurf.py made (OUTDIR/!NetSurf and OUTDIR/BUILD-INFO.txt) into a zip with RISC OS file types.

A file called  name,xxx  (the Linux convention for a file type) is stored as  name  with the type &XXX in the Info-ZIP "ARC0" extra field, the way the packages of this project are; a file without a type is Text (&FFF).
The zip has one folder, NetSurf, with !NetSurf, the ReadMe and BUILD-INFO."""
import os, re, struct, sys, time, zipfile
if len(sys.argv) < 3: sys.exit(__doc__)
out, path = sys.argv[1], sys.argv[2]
readme = open(sys.argv[3]).read() if len(sys.argv) > 3 else None
NOW = time.time()
def riscos_stamp(u): cs = int((u + 2208988800) * 100); return (cs >> 32) & 0xFF, cs & 0xFFFFFFFF
def extra(ftype, u, attr=0x13):
    hi, lo = riscos_stamp(u); load = 0xFFF00000 | (ftype << 8) | hi
    ac = b"ARC0" + struct.pack("<III", load, lo, attr) + b"\0\0\0\0"
    return struct.pack("<HH", 0x4341, len(ac)) + ac + struct.pack("<HHBI", 0x5455, 5, 1, int(u))
entries = []; dirs = set()
def add(zipname, data, ftype):
    entries.append((zipname, data, ftype))
    d = os.path.dirname(zipname)
    while d: dirs.add(d); d = os.path.dirname(d)
top = os.path.join(out, "!NetSurf")
for dp, dn, fn in os.walk(top):
    for f in sorted(fn):
        p = os.path.join(dp, f); rel = os.path.relpath(p, out)
        m = re.search(r",([0-9a-fA-F]{3})$", rel)
        ftype = int(m.group(1), 16) if m else 0xFFF
        if m: rel = rel[:m.start()]
        add("NetSurf/" + rel, open(p, "rb").read(), ftype)
if readme: add("NetSurf/ReadMe", readme.encode("latin-1", "replace"), 0xFFF)
add("NetSurf/BUILD-INFO", open(os.path.join(out, "BUILD-INFO.txt"), "rb").read(), 0xFFF)
with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for d in sorted(dirs | {"NetSurf"}):
        zi = zipfile.ZipInfo(d + "/", date_time=time.localtime(NOW)[:6]); zi.create_system = 3
        zi.external_attr = (0o40775 << 16) | 0x10; zi.extra = struct.pack("<HHBI", 0x5455, 5, 1, int(NOW)); z.writestr(zi, b"")
    for name, data, ftype in entries:
        zi = zipfile.ZipInfo(name, date_time=time.localtime(NOW)[:6]); zi.create_system = 3
        zi.external_attr = 0o100755 << 16 if ftype in (0xE1F, 0xFF8, 0xFFA) else 0o100644 << 16
        zi.compress_type = zipfile.ZIP_DEFLATED; zi.extra = extra(ftype, NOW); z.writestr(zi, data)
assert zipfile.ZipFile(path).testzip() is None
print("wrote %s: %d files, %d bytes" % (path, len(entries), os.path.getsize(path)))
