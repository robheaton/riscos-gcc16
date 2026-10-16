#!/usr/bin/env python3
"""check_mod64.py FILE...: check a 64-bit module image against the module format of the README of riscos64-clib: the header words, bit 30 of the initialisation offset, the flags word
(architecture AArch64 in bits 4-7, bit 2 and a zero-initialisation size after it), the offsets inside the file, the strings, the command table and the SWI name table, and that the code has no
absolute addresses (the module has no relocations: only ADR/ADRP style references: checked in the ELF by the build with readelf -r)."""
import struct, sys
args = sys.argv[1:]
name, swis, chunk, nocmds = "Mod64", "Add,Add32,Fail,Carry,FailRMA", None, False       # what the module of tests 0.1-0.3 has; --name N --swis A,B --chunk &X --nocmds for another one
while args and args[0].startswith("--"):
    o = args.pop(0)
    if o == "--name": name = args.pop(0)
    elif o == "--swis": swis = args.pop(0)
    elif o == "--chunk": chunk = int(args.pop(0), 0)
    elif o == "--nocmds": nocmds = True
    else: sys.exit("check_mod64.py: unknown option " + o)
bad = 0
for f in args:
    d = open(f, "rb").read(); errs = []
    def need(c, m):
        if not c: errs.append(m)
    w = struct.unpack("<13I", d[:52])
    names = "start init final service title help cmds swibase swih swinames swicode msgs flags".split()
    h = dict(zip(names, w))
    need(h["start"] == 0, "start entry")
    need(h["init"] & 0x40000000, "bit 30 of the initialisation offset is not set")
    init = h["init"] & ~0x40000000
    for k in ("init", "final", "title", "help", "cmds", "swih", "swinames", "flags"):
        if k == "cmds" and nocmds: need(h[k] == 0, "the command table offset is not 0"); continue
        o = h[k] & ~0x40000000
        need(0 < o < len(d), "%s offset &%X outside the file" % (k, o))
    def cstr(o): return d[o:d.index(b"\0", o)]
    need(cstr(h["title"]) == name.encode(), "title")
    need(name.encode() in cstr(h["help"]), "help string")
    if chunk is not None: need(h["swibase"] == chunk, "SWI chunk is &%X, not &%X" % (h["swibase"], chunk))
    fl = struct.unpack("<I", d[h["flags"]:h["flags"] + 4])[0]
    need((fl >> 4) & 15 == 1, "architecture in the flags is %d, not 1 (AArch64)" % ((fl >> 4) & 15))
    need(fl & 4, "bit 2 of the flags (zero-init size follows) is not set")
    need(not fl & 1, "bit 0 of the flags (32-bit compatible) is set: it must be clear in a 64-bit module")
    zi = struct.unpack("<I", d[h["flags"] + 4:h["flags"] + 8])[0]
    # command table: name, padded to a word, then four words
    if not nocmds:
        o = h["cmds"]; n = cstr(o); o = (o + len(n) + 1 + 3) & ~3
        code, info, syn, hlp = struct.unpack("<4I", d[o:o + 16])
        need(n == b"Mod64_Hello" and 0 < code < len(d) and 0 < syn < len(d) and 0 < hlp < len(d), "command table")
        need(d[o + 16:o + 20] == b"\0\0\0\0", "command table is not ended by a zero word")
    # SWI names
    want = (name + "\0" + "\0".join(swis.split(",")) + "\0\0").encode()
    s = d[h["swinames"]:h["swinames"] + len(want)]
    need(s == want, "SWI name table")
    need(len(d) % 4 == 0, "size not a multiple of 4")
    print("%s: %s" % (f, "OK (%d bytes, init &%X final &%X, SWI chunk &%X, flags &%X, zero-init %d)" % (len(d), init, h["final"], h["swibase"], fl, zi) if not errs else "BAD"))
    for e in errs: print("   " + e)
    bad += bool(errs)
sys.exit(1 if bad else 0)
