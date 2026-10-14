#!/usr/bin/env python3
"""modinfo.py FILE... - what is in a module image that gcc -mmodule made, and is its relocation table right (the checks of modcheck, for any title)"""
import struct, sys

def cstr(img, off):
    return img[off:img.index(0, off)].decode("latin-1") if off else None

def check(path):
    img = open(path, "rb").read()
    n = len(img)
    out = []
    if n < 56 or n % 4:
        return False, ["not a module image (%d bytes)" % n]
    if img[:4] == b"\x7fELF":
        return False, ["still an ELF file"]
    w = lambda off: struct.unpack_from("<I", img, off)[0]
    h = [w(4 * i) for i in range(13)]
    ok = True
    for i, name in enumerate(["start", "init", "final", "service", "title", "help", "keywords", "swi chunk", "swi handler", "swi table", "swi decoder", "messages", "flags"]):
        if name in ("swi chunk", "start"):
            continue
        if h[i] >= n or h[i] % 4 and name not in ("title", "help", "messages", "swi table"):
            out.append("header word %s = %d points outside the image or is not aligned" % (name, h[i])); ok = False
    if h[0] and (h[0] >= n or h[0] % 4):
        out.append("bad start entry %d" % h[0]); ok = False
    flags = h[12]
    if flags + 16 > n or flags % 4:
        out.append("flags word at %d is outside the image" % flags); return False, out
    if w(flags) != 1:
        out.append("flags word is %d, not 1" % w(flags)); ok = False
    if w(flags + 4) != 0:
        out.append("linked address %d, not 0" % w(flags + 4)); ok = False
    tab, count = w(flags + 8), w(flags + 12)
    if tab % 4 or tab > n or tab + 4 * count != n or count == 0:
        out.append("relocation table (%d, %d entries) does not end the file" % (tab, count)); return False, out
    bad = 0
    for i in range(count):
        off = w(tab + 4 * i)
        if off % 4 or off + 4 > tab:
            bad += 1; continue
        if w(off) > tab:
            bad += 1
    if bad:
        out.append("%d bad relocation entries" % bad); ok = False
    title, helps = cstr(img, h[4]), cstr(img, h[5])
    names = []
    if h[9]:
        k = h[9]; k += len(cstr(img, k)) + 1
        while img[k]:
            nm = cstr(img, k); names.append(nm); k += len(nm) + 1
    cmds = []
    if h[6]:
        k = h[6]
        while img[k]:
            nm = cstr(img, k); k += (len(nm) + 1 + 3) & ~3
            cmds.append(nm); k += 16
    info = "%s: %d bytes, %d relocs; title %r, help %r%s; %d commands%s; SWI chunk &%X (%d names%s)%s%s%s" % (
        path.split("/")[-1], n, count, title, helps, "", len(cmds), " (" + ", ".join(cmds[:5]) + (", ..." if len(cmds) > 5 else "") + ")" if cmds else "",
        h[7], len(names), ": " + ", ".join(names[:4]) + (", ..." if len(names) > 4 else "") if names else "",
        "; runnable" if h[0] else "", "; service" if h[3] else "", "; messages %r" % cstr(img, h[11]) if h[11] else "")
    out.append(info)
    return ok, out

rc = 0
for p in sys.argv[1:]:
    ok, lines = check(p)
    print(("OK    " if ok else "BAD   ") + "\n      ".join(lines))
    rc |= 0 if ok else 1
sys.exit(rc)
