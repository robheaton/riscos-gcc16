#!/usr/bin/env python3
"""modreloc64.py [-q] ELF OUT   |   modreloc64.py --info ELF

Makes the flat image of a 64-bit module (an AArch64 module that relocates itself, "the published way") from the ELF file that the linker wrote with --emit-relocs and the script module64.ld (or mod64d.ld)
(everything the module has in ONE loadable output section, .image, linked at address 4: the module is loaded at a page boundary plus 4).

 * the code reaches code and data PC-relative (ADRP/ADD, ADR, BL), which stays right when the whole image moves by a multiple of 4 KB plus nothing: the module is loaded at page+4 and linked at 4;
 * the only things that are wrong after a move are the 64-bit words that hold an address (R_AARCH64_ABS64: tables of pointers, a pointer to data, link_addr): this tool lists them (their offsets in the
   image) in the table that the module header reserves (reloc_table, reloc_count, with room for reloc_cap entries), and the module's own relocator (mod_reloc in modcore64.S (kit) or mod64d_hdr.S) adds the difference
   to each of them when the module starts;
 * a module that needs anything else is refused, with the reason: a GOT entry (position independent code that reaches an extern symbol through the GOT), a MOVZ/MOVK address, a 32-bit address word,
   a TLS relocation.  An absolute symbol (SHN_ABS) is a number, not an address: a word that holds one is left alone (the size of the zero-initialised area in the header is one).
The output file is the image: the part of the module that is in the file.  The zero-initialised area (.zi, NOLOAD) is not in it: the kernel allocates it after the module, as the header says.

  ELF OUT     write the image to OUT
  --info ELF  print what the tool found (sections, the relocations, the symbols) and write nothing
  -q          no message
Stdlib only (a small ELF64 reader)."""
import struct, sys

SHF_ALLOC = 2
SHN_ABS = 0xFFF1
R_NAMES = {0: "R_AARCH64_NONE", 257: "R_AARCH64_ABS64", 258: "R_AARCH64_ABS32", 259: "R_AARCH64_ABS16", 260: "R_AARCH64_PREL64", 261: "R_AARCH64_PREL32", 262: "R_AARCH64_PREL16",
           263: "R_AARCH64_MOVW_UABS_G0", 264: "R_AARCH64_MOVW_UABS_G0_NC", 265: "R_AARCH64_MOVW_UABS_G1", 266: "R_AARCH64_MOVW_UABS_G1_NC", 267: "R_AARCH64_MOVW_UABS_G2",
           268: "R_AARCH64_MOVW_UABS_G2_NC", 269: "R_AARCH64_MOVW_UABS_G3", 270: "R_AARCH64_MOVW_SABS_G0", 271: "R_AARCH64_MOVW_SABS_G1", 272: "R_AARCH64_MOVW_SABS_G2",
           273: "R_AARCH64_LD_PREL_LO19", 274: "R_AARCH64_ADR_PREL_LO21", 275: "R_AARCH64_ADR_PREL_PG_HI21", 276: "R_AARCH64_ADR_PREL_PG_HI21_NC", 277: "R_AARCH64_ADD_ABS_LO12_NC",
           278: "R_AARCH64_LDST8_ABS_LO12_NC", 279: "R_AARCH64_TSTBR14", 280: "R_AARCH64_CONDBR19", 282: "R_AARCH64_JUMP26", 283: "R_AARCH64_CALL26", 284: "R_AARCH64_LDST16_ABS_LO12_NC",
           285: "R_AARCH64_LDST32_ABS_LO12_NC", 286: "R_AARCH64_LDST64_ABS_LO12_NC", 299: "R_AARCH64_LDST128_ABS_LO12_NC", 311: "R_AARCH64_ADR_GOT_PAGE",
           312: "R_AARCH64_LD64_GOT_LO12_NC", 1025: "R_AARCH64_GLOB_DAT", 1026: "R_AARCH64_JUMP_SLOT", 1027: "R_AARCH64_RELATIVE"}
PCREL = {260, 261, 262, 273, 274, 275, 276, 277, 278, 279, 280, 282, 283, 284, 285, 286, 299}   # resolved by the linker, right wherever the image is, as long as it moves as a whole at page + 4
ABS64 = 257
ABSNARROW = {258, 259}                                                      # fine only against an absolute symbol

class Elf:
    def __init__(self, path):
        d = self.d = open(path, "rb").read()
        if d[:4] != b"\x7fELF" or d[4] != 2 or d[5] != 1: raise SystemExit("modreloc64: %s is not a little-endian ELF64 file" % path)
        (self.type, self.machine) = struct.unpack_from("<HH", d, 16)
        if self.machine != 183: raise SystemExit("modreloc64: %s is not an AArch64 file (machine %d)" % (path, self.machine))
        if self.type != 2: raise SystemExit("modreloc64: %s is not a linked file (a partial link, -r?): link it with module64.ld (or mod64d.ld) first" % path)
        shoff, = struct.unpack_from("<Q", d, 40)
        shentsize, shnum, shstrndx = struct.unpack_from("<HHH", d, 58)
        self.secs = []
        for i in range(shnum):
            name, typ, flags, addr, off, size, link, info, align, entsize = struct.unpack_from("<IIQQQQIIQQ", d, shoff + i * shentsize)
            self.secs.append(dict(i=i, nameoff=name, type=typ, flags=flags, addr=addr, off=off, size=size, link=link, info=info, entsize=entsize))
        sh = self.secs[shstrndx]
        for s in self.secs: s["name"] = self.cstr(sh["off"], s["nameoff"])
        self.syms = {}
        self.symlist = []
        for s in self.secs:
            if s["type"] == 2:                                   # SHT_SYMTAB
                strs = self.secs[s["link"]]
                for k in range(s["size"] // 24):
                    nm, info, other, shndx, value, size = struct.unpack_from("<IBBHQQ", d, s["off"] + k * 24)
                    name = self.cstr(strs["off"], nm)
                    self.symlist.append((name, value, shndx, info))
                    if name and name not in self.syms: self.syms[name] = (value, shndx)
    def cstr(self, base, off):
        e = self.d.index(b"\0", base + off)
        return self.d[base + off:e].decode("latin-1")
    def sec(self, name):
        for s in self.secs:
            if s["name"] == name: return s
        return None

def analyse(path):
    """-> (elf, image section, image bytes (bytearray), sorted list of the offsets of the words to relocate, the symbols)"""
    e = Elf(path)
    img = e.sec(".image")
    if img is None or img["type"] != 1: raise SystemExit("modreloc64: %s has no .image section: it was not linked with module64.ld (or mod64d.ld)" % path)
    for s in e.secs:
        if s["name"] in (".got", ".got.plt") and s["size"]:
            raise SystemExit("modreloc64: %s has a GOT (%s, %d bytes): the code was compiled position independent and reaches a symbol through it. Compile with -fPIC -fvisibility=hidden, and declare "
                             "extern symbols hidden" % (path, s["name"], s["size"]))
    base, size = img["addr"], img["size"]
    if base & 0xFFF != 4: raise SystemExit("modreloc64: the image is linked at &%X: it must be at a page boundary plus 4 (the module is loaded there)" % base)
    if (base + size) % 16: raise SystemExit("modreloc64: the image ends at &%X: that must be a multiple of 16, because the kernel puts the zero-initialised area right after the file (base + size) and the code "
                                           "expects it at the next 16-byte boundary of the link addresses (the modules of riscos64-clib are all 12 bytes past a multiple of 16 for that reason)" % (base + size))
    data = bytearray(e.d[img["off"]:img["off"] + size])
    abssyms = {name for (name, value, shndx, info) in e.symlist if shndx == SHN_ABS}
    offs = []
    bad = []
    for s in e.secs:
        if s["type"] != 4: continue                              # SHT_RELA
        tgt = e.secs[s["info"]]
        if not (tgt["flags"] & SHF_ALLOC) or tgt["addr"] < base or tgt["addr"] >= base + size: continue    # debug sections and the like
        symsec = e.secs[s["link"]]
        for k in range(s["size"] // 24):
            roff, rinfo, addend = struct.unpack_from("<QQq", e.d, s["off"] + k * 24)
            typ, symi = rinfo & 0xFFFFFFFF, rinfo >> 32
            if symi:
                nm, info, other, shndx, value, ssize = struct.unpack_from("<IBBHQQ", e.d, symsec["off"] + symi * 24)
                name, isabs, undef = e.cstr(e.secs[symsec["link"]]["off"], nm), shndx == SHN_ABS, shndx == 0
            else:
                name, value, isabs, undef = "", 0, False, False
            if typ == 0: continue
            if typ in PCREL:
                if symi and (isabs or undef):                    # a PC-relative reference to a number or to nothing is right only at the address it was linked at: it breaks when the image moves
                    bad.append((roff, R_NAMES[typ], "against %s, which is %s: a PC-relative reference to it is wrong after the image is loaded somewhere else" % (name, "an absolute symbol" if isabs else "undefined")))
                continue
            if typ == ABS64:
                if isabs: continue                               # a number, not an address
                if not (base <= roff < base + size) or roff % 8:
                    bad.append((roff, R_NAMES[typ], "the word is not inside the image or not 8-byte aligned")); continue
                held = struct.unpack_from("<Q", data, roff - base)[0]
                if held != (value + addend) & 0xFFFFFFFFFFFFFFFF:
                    bad.append((roff, R_NAMES[typ], "the word holds &%X, the relocation says &%X (%s%+d)" % (held, value + addend, name, addend))); continue
                offs.append(roff - base)
            elif typ in ABSNARROW and isabs: continue
            else:
                bad.append((roff, R_NAMES.get(typ, "type %d" % typ), "against %s: not allowed in a module image (%s)" % (name or "a section", reason(typ))))
    if bad:
        raise SystemExit("modreloc64: %s: relocations that the module's loader cannot apply:\n  %s" % (path, "\n  ".join("&%X %s %s" % b for b in bad[:20])))
    return e, img, data, sorted(set(offs))

def reason(typ):
    if typ in (311, 312, 1025, 1026, 1027): return "a GOT or dynamic relocation: position independent code that reaches an extern symbol through the GOT"
    if 263 <= typ <= 272: return "an address built with MOVZ/MOVK is absolute: use ADRP/ADD (the compiler's small code model does)"
    if typ in (258, 259): return "a 32-bit or 16-bit word that holds an address cannot be relocated"
    if typ >= 512: return "thread-local storage"
    return "no loader support"

def check_header(e, img, data, nrel):
    """a structural check of the module header, so that a bad module is caught before it is loaded: returns a list of problems"""
    errs = []
    w = lambda o: struct.unpack_from("<I", data, o)[0]
    n = len(data)
    if n < 52: return ["the image is %d bytes: no module header" % n]
    h = dict(zip("start init final service title help cmds swibase swih swinames swicode msgs flags".split(), struct.unpack_from("<13I", data, 0)))
    if not h["init"] & 0x40000000: errs.append("bit 30 of the initialisation offset is not set (the entry is AArch64 code)")
    init = h["init"] & 0x3FFFFFFF
    for k, v in (("initialisation", init), ("finalisation", h["final"]), ("service", h["service"]), ("title", h["title"]), ("help", h["help"]), ("SWI handler", h["swih"]), ("flags", h["flags"])):
        if v and not 0 < v < n: errs.append("the %s offset &%X is outside the image" % (k, v))
    for k in ("cmds", "swinames"):
        if h[k] and not 0 < h[k] < n: errs.append("the %s offset &%X is outside the image" % (k, h[k]))
    for k, nm in (("swicode", "SWI decoding code"), ("msgs", "messages file")):
        if h[k] and not 0 < h[k] < n: errs.append("the %s offset &%X is outside the image" % (nm, h[k]))
    if 0 < h["flags"] < n - 8:
        fl, zi = struct.unpack_from("<II", data, h["flags"])
        if fl != 0x14: errs.append("the flags word is &%X, not &14 (bit 0 clear, bit 2: zero-initialised size follows, architecture 1)" % fl)
        want = e.syms["__ZIEnd"][0] - e.syms["__ZIStart"][0]
        if zi != want: errs.append("the zero-initialised size in the header is %d, the linker's is %d" % (zi, want))
        if e.syms["__ZIStart"][0] != img["addr"] + n: errs.append("the zero-initialised area is not right after the image")
    else: errs.append("no flags word")
    if 0 < h["service"] < n:                                   # a service table: the word before the entry is the offset of the table; the entry is RET
        tab = w(h["service"] - 4)
        if w(h["service"]) != 0xD65F03C0: errs.append("the service entry is not a RET: the kernel would call it for every service")
        elif not (0 < tab < h["service"] and w(tab) == 1 and w(h["service"] - 8) == 0): errs.append("the service table is malformed")
    o, nc = h["cmds"], 0
    while h["cmds"] and 0 < o < n - 20 and data[o] != 0 or (h["cmds"] and nc == 0 and data[o] != 0):
        e0 = data.index(b"\0", o); o = (e0 + 1 + 3) & ~3
        code, info, syn, hlp = struct.unpack_from("<4I", data, o)
        for nm, v in (("code", code), ("syntax", syn), ("help", hlp)):
            if not 0 < v < n: errs.append("command %d: the %s offset &%X is outside the image" % (nc, nm, v))
        o += 16; nc += 1
        if o >= n - 4 or w(o) == 0: break
    if nrel > 0 and "link_addr" in e.syms and e.syms["link_addr"][0] - img["addr"] >= n: errs.append("link_addr is outside the image")
    return errs

def main(argv):
    quiet, info, pos = False, False, []
    for a in argv:
        if a == "-q": quiet = True
        elif a == "--info": info = True
        elif a in ("-h", "--help"): print(__doc__); return 0
        else: pos.append(a)
    if (info and len(pos) != 1) or (not info and len(pos) != 2): sys.exit("usage: modreloc64.py [-q] ELF OUT   |   modreloc64.py --info ELF   (modreloc64.py -h)")
    e, img, data, offs = analyse(pos[0])
    for need in ("reloc_count", "reloc_cap", "reloc_table", "_mod_start", "link_addr", "__image_end", "__ZIStart", "__ZIEnd"):
        if need not in e.syms: sys.exit("modreloc64: %s has no %s: it was not linked with the module header of mod64d_hdr.S" % (pos[0], need))
    base = img["addr"]
    rc, rcap, rtab = (e.syms[n][0] - base for n in ("reloc_count", "reloc_cap", "reloc_table"))
    cap = struct.unpack_from("<I", data, rcap)[0]
    zi = e.syms["__ZIEnd"][0] - e.syms["__ZIStart"][0]
    if info:
        print("image &%X, %d bytes, ZI %d bytes at &%X, %d relocations (room for %d), link_addr at +&%X" % (base, img["size"], zi, e.syms["__ZIStart"][0], len(offs), cap, e.syms["link_addr"][0] - base))
        for o in offs: print("  +&%04X  (word at &%X holds &%X)" % (o, base + o, struct.unpack_from("<Q", data, o)[0]))
        return 0
    problems = check_header(e, img, data, len(offs))
    if problems: sys.exit("modreloc64: %s: the module header is wrong:\n  %s" % (pos[0], "\n  ".join(problems)))
    if len(offs) > cap: sys.exit("modreloc64: %d words to relocate and room for %d: raise RELOC_CAP in mod64d_hdr.S" % (len(offs), cap))
    if e.syms["link_addr"][0] - base not in offs: sys.exit("modreloc64: link_addr is not in the list of relocations: the header was not linked as expected")
    struct.pack_into("<I", data, rc, len(offs))
    for k, o in enumerate(offs): struct.pack_into("<I", data, rtab + 4 * k, o)
    open(pos[1], "wb").write(data)
    if not quiet: print("%s: %d bytes (image at &%X, %d relocations, zero-initialised area %d bytes)" % (pos[1], len(data), base, len(offs), zi))
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
