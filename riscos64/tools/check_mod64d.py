#!/usr/bin/env python3
"""check_mod64d.py ELF IMAGE: checks a 64-bit module made by modreloc64.py (Mod64D of module test 0.4) before it is loaded: the header words, bit 30 of the
initialisation offset, the flags word (&14) and the zero-initialisation size, the strings, the SWI and command tables, the service table, the entry code, the relocation table against the relocations of the ELF
file (every word that holds an address is listed, nothing else is, each one is 8-byte aligned and holds an address inside the module).  (That the code uses neither floating point nor vector registers nor x18
is checked on the object files by build.sh: this file's image has data in its executable section.)  Exits with 1 if anything is wrong."""
import os, re, struct, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import modreloc64

if len(sys.argv) != 3: sys.exit(__doc__)
elf_path, img_path = sys.argv[1], sys.argv[2]
e, img, data0, offs = modreloc64.analyse(elf_path)
d = open(img_path, "rb").read()
base = img["addr"]
errs = []
def need(c, msg):
    if not c: errs.append(msg)
w = lambda o: struct.unpack_from("<I", d, o)[0]
def cstr(o): return d[o:d.index(b"\0", o)]

need((base + len(d)) % 16 == 0, "the image does not end at a multiple of 16 (link address): the zero-initialised area would not be where the code expects it")
h = dict(zip("start init final service title help cmds swibase swih swinames swicode msgs flags".split(), struct.unpack_from("<13I", d, 0)))
need(h["start"] == 0, "start entry is not 0")
need(h["init"] & 0x40000000, "bit 30 of the initialisation offset is not set")
init = h["init"] & 0x3FFFFFFF
for k in ("final", "service", "title", "help", "cmds", "swih", "swinames", "flags"): need(0 < h[k] < len(d), "%s offset &%X is outside the image" % (k, h[k]))
need(0 < init < len(d), "initialisation offset &%X is outside the image" % init)
need(h["swibase"] == 0x59A40, "SWI chunk is &%X, not &59A40" % h["swibase"])
need(h["swicode"] == 0 and h["msgs"] == 0, "SWI decoding code and the message file must be 0")
need(cstr(h["title"]) == b"Mod64D", "title")
need(b"Mod64D" in cstr(h["help"]) and b"0.04" in cstr(h["help"]), "help string")
want = b"Mod64D\0Add\0Add32\0Setup\0Info\0Reloc\0Count\0Zero\0Stack\0FailCopy\0FailRMA\0Svc\0Args\0Heap\0Dirty\0\0"
need(d[h["swinames"]:h["swinames"] + len(want)] == want, "SWI name table")

# flags word and zero-initialisation size
fl, zi = struct.unpack_from("<II", d, h["flags"])
need(fl == 0x14, "flags word is &%X, not &14 (bit 0 clear, bit 2 set, architecture 1)" % fl)
need(zi == e.syms["__ZIEnd"][0] - e.syms["__ZIStart"][0] and zi >= 6000 and zi % 16 == 0, "zero-initialisation size %d does not match the linker's (%d)" % (zi, e.syms["__ZIEnd"][0] - e.syms["__ZIStart"][0]))
need(e.syms["__ZIStart"][0] == base + len(d), "the zero-initialised area is at &%X, not right after the image (&%X)" % (e.syms["__ZIStart"][0], base + len(d)))

# the command table: name, padded to a word, code offset, information word, syntax, help; ended by a 0 word
o, ncmd = h["cmds"], 0
while w(o) != 0 or ncmd == 0:
    name = cstr(o)
    if not name.startswith(b"Mod64D_"): errs.append("command %d name %r" % (ncmd, name)); break
    o = (o + len(name) + 1 + 3) & ~3
    code, info, syn, hlp = struct.unpack_from("<4I", d, o)
    need(0 < code < len(d) and syn < len(d) and hlp < len(d) and info == (0x30000 if ncmd == 3 else 0), "command %s: table entry" % name.decode())
    need(w(code) == 0xD2800002 + 0x20 * ncmd, "command %s: the entry does not start with MOV x2,#%d" % (name.decode(), ncmd))
    o += 16; ncmd += 1
need(ncmd == 4, "%d commands, expected 4" % ncmd)

# the service table: flags, the fast entry, the numbers and a 0, the offset of the table in the word before the entry
tab = w(h["service"] - 4)
need(0 < tab < h["service"], "the word before the service entry (&%X) is not the offset of the service table" % tab)
need(w(tab) == 1 and 0 < w(tab + 4) < len(d) and w(tab + 8) == 0x59A41 and w(tab + 12) == 0 and w(tab + 16) == tab, "service table")

# the entry code
need(w(init) == 0xA9BE7BFD, "the initialisation entry does not start with STP x29,x30,[sp,#-32]!")
need(w(h["final"]) == 0xA9BF7BFD and w(h["swih"]) == 0xA9BF7BFD, "finalisation and SWI entries do not start with STP x29,x30,[sp,#-16]!")

# the relocation table against the relocations of the ELF file
rc, rcap, rtab = (e.syms[n][0] - base for n in ("reloc_count", "reloc_cap", "reloc_table"))
n = w(rc)
cap = w(rcap)
need(n == len(offs), "reloc_count is %d, the ELF file has %d words that hold an address" % (n, len(offs)))
need(n <= cap, "reloc_count %d is more than the room (%d)" % (n, cap))
table = [w(rtab + 4 * k) for k in range(min(n, cap))]
need(table == offs, "the relocation table is not the list of the ELF file's relocations")
for o in table:
    v = struct.unpack_from("<Q", d, o)[0]
    need(o % 8 == (8 - base % 8) % 8 or (base + o) % 8 == 0, "relocation +&%X: the word is not 8-byte aligned" % o)
    need(base <= v < base + len(d) + zi, "relocation +&%X: the word holds &%X, outside the module" % (o, v))
need(struct.unpack_from("<Q", d, e.syms["link_addr"][0] - base)[0] == base, "link_addr does not hold the link address (&%X)" % base)
need(all(w(rtab + 4 * k) == 0 for k in range(n, cap)), "unused entries of the relocation table are not 0")

print("%s: %s (%d bytes, %d relocations, %d commands, zero-initialised area %d)" % (img_path, "OK" if not errs else "BAD", len(d), len(offs), ncmd, zi))
for m in errs: print("   " + m)
sys.exit(1 if errs else 0)
