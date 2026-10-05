#!/usr/bin/env python3
"""rochecks.py FILE...  -- independent structural checks of RISC OS EABI ELF files produced by the linker.

Checks (each is derived from the RISC OS dynamic-linking design, !SharedLibs.docs.Technical, not from another linker):
  hdr   : e_ehsize == 72 and program headers start at 72 (the 5-word runtime workspace after the ELF header)
  seg   : exactly one R(+X) PT_LOAD and at most one RW PT_LOAD, both starting where the file offset says
          (vaddr - offset constant), RW loaded after RO; PT_INTERP only in executables
  abi   : DT_RISCOS_ABI_VERSION points (file-offset-wise) at the NUL terminated string "armeabihf"
  pltgot: DT_PLTGOT present and inside the RW segment
  pic   : every PIC-register load sequence  mov rN,#0x8000 ; ldr rN,[rN,#0x38] ; ldr rM,[rN,#0]  has an entry
          (bit 31 set) in .riscos.pic, every entry points at such a sequence, header count is exact
  plt   : every .plt stub addresses the GOT slot of exactly one R_ARM_JUMP_SLOT relocation (same symbol), and the
          slot initially holds the address of PLT0 (lazy binding)
"""
import struct
import sys

R_ARM_JUMP_SLOT = 22
PT_LOAD, PT_DYNAMIC, PT_INTERP = 1, 2, 3
DT = {"NULL": 0, "NEEDED": 1, "PLTRELSZ": 2, "PLTGOT": 3, "HASH": 4, "STRTAB": 5, "SYMTAB": 6, "JMPREL": 23,
      "RISCOS_PIC": 0x6000000d, "RISCOS_ABI": 0x6000000e}


class Elf:
    def __init__(self, path):
        self.path = path
        self.d = open(path, "rb").read()
        d = self.d
        (self.e_type, self.e_machine, _, self.e_entry, self.e_phoff, self.e_shoff, self.e_flags, self.e_ehsize,
         self.e_phentsize, self.e_phnum, self.e_shentsize, self.e_shnum, self.e_shstrndx) = struct.unpack_from("<HHIIIIIHHHHHH", d, 16)
        assert d[:4] == b"\x7fELF" and d[4] == 1 and d[5] == 1, "not ELF32 LE"
        self.ph = []
        for i in range(self.e_phnum):
            self.ph.append(struct.unpack_from("<IIIIIIII", d, self.e_phoff + i * self.e_phentsize))
        self.sh = []
        for i in range(self.e_shnum):
            self.sh.append(struct.unpack_from("<IIIIIIIIII", d, self.e_shoff + i * self.e_shentsize))
        strtab = self.sh[self.e_shstrndx]
        self.names = []
        for s in self.sh:
            o = strtab[4] + s[0]
            self.names.append(d[o:d.index(b"\0", o)].decode())
        self.byname = {n: s for n, s in zip(self.names, self.sh)}

    def sec(self, name):
        s = self.byname.get(name)
        if not s:
            return None
        return self.d[s[4]:s[4] + s[5]] if s[1] != 8 else b""

    def vaddr_to_off(self, va):
        for p in self.ph:   # (type, offset, vaddr, paddr, filesz, memsz, flags, align)
            if p[0] == PT_LOAD and p[2] <= va < p[2] + p[4]:
                return p[1] + (va - p[2])
        return None

    def dynamic(self):
        s = self.sec(".dynamic")
        out = []
        if s:
            for i in range(0, len(s), 8):
                t, v = struct.unpack_from("<II", s, i)
                out.append((t, v))
                if t == 0:
                    break
        return out

    def dynsym_name(self, idx):
        sy = self.byname[".dynsym"]
        st = self.sh[sy[6]]
        o = sy[4] + idx * 16
        nm = struct.unpack_from("<I", self.d, o)[0]
        s = st[4] + nm
        return self.d[s:self.d.index(b"\0", s)].decode()


def check(path):
    e = Elf(path)
    errs = []
    notes = []
    isexe = e.e_type == 2
    dyn = dict((t, v) for t, v in e.dynamic())

    # hdr
    if e.e_ehsize != 72 or e.e_phoff != 72:
        errs.append(f"hdr: e_ehsize={e.e_ehsize} e_phoff={e.e_phoff} (want 72/72)")

    # seg
    loads = [p for p in e.ph if p[0] == PT_LOAD]
    ro = [p for p in loads if (p[6] & 6) == 4]
    rw = [p for p in loads if (p[6] & 7) in (6,)]
    other = [p for p in loads if p not in ro and p not in rw]
    if len(ro) != 1 or len(rw) > 1 or other:
        errs.append(f"seg: LOAD segments ro={len(ro)} rw={len(rw)} other={len(other)} (flags {[hex(p[6]) for p in loads]})")
    base = 0x8000 if isexe else 0
    for p in loads:
        if p[2] - p[1] != base:
            errs.append(f"seg: LOAD vaddr-offset {p[2]-p[1]:#x} != {base:#x}")
    if ro and rw and not (rw[0][2] >= ro[0][2] + ro[0][5]):
        errs.append("seg: RW segment overlaps RO segment")
    if isexe != any(p[0] == PT_INTERP for p in e.ph):
        errs.append("seg: PT_INTERP presence mismatch for e_type")
    if sum(1 for p in e.ph if p[0] == PT_DYNAMIC) != 1:
        errs.append("seg: want exactly one PT_DYNAMIC")

    # abi
    if DT["RISCOS_ABI"] in dyn:
        off = e.vaddr_to_off(dyn[DT["RISCOS_ABI"]])
        s = e.d[off:e.d.index(b"\0", off)] if off is not None else None
        if s != b"armeabihf":
            errs.append(f"abi: version string {s!r}")
    else:
        errs.append("abi: no DT_RISCOS_ABI_VERSION")

    # pltgot
    pg = dyn.get(DT["PLTGOT"])
    if pg is None:
        errs.append("pltgot: missing")
    elif rw and not (rw[0][2] <= pg < rw[0][2] + rw[0][5]):
        errs.append(f"pltgot: {pg:#x} outside RW segment")

    # pic
    pic = e.sec(".riscos.pic")
    entries = []
    if pic:
        ver, cnt = struct.unpack_from("<II", pic, 0)
        if ver != 1:
            errs.append(f"pic: version {ver}")
        if 8 + 4 * cnt > len(pic):
            errs.append(f"pic: header count {cnt} exceeds section")
            cnt = (len(pic) - 8) // 4
        entries = [struct.unpack_from("<I", pic, 8 + 4 * i)[0] for i in range(cnt)]
        if DT["RISCOS_PIC"] not in dyn:
            errs.append("pic: .riscos.pic without DT_RISCOS_PIC")
        elif e.vaddr_to_off(dyn[DT["RISCOS_PIC"]]) != e.byname[".riscos.pic"][4]:
            errs.append("pic: DT_RISCOS_PIC does not point at .riscos.pic")
    sites = set()
    for n, s_ in zip(e.names, e.sh):
        if s_[1] == 1 and (s_[2] & 4) and s_[5] >= 12:  # PROGBITS + EXECINSTR
            data = e.d[s_[4]:s_[4] + s_[5]]
            words = struct.unpack("<%dI" % (len(data) // 4), data[:len(data) // 4 * 4])
            for i, w0 in enumerate(words):
                if (w0 & 0xffff0fff) != 0xe3a00902:      # mov rM,#0x8000
                    continue
                m = (w0 >> 12) & 15
                for j in range(i + 1, min(i + 10, len(words))):
                    w1 = words[j]
                    if (w1 & 0xfff00fff) == 0xe5900038 and ((w1 >> 16) & 15) == m:   # ldr rN,[rM,#0x38]
                        nreg = (w1 >> 12) & 15
                        for k in range(j + 1, min(j + 9, len(words))):
                            w2 = words[k]
                            if (w2 & 0xfff00fff) == 0xe5900000 and ((w2 >> 16) & 15) == nreg:  # ldr rX,[rN,#0]
                                sites.add(s_[3] + k * 4)
                                break
                        break
    idx_entries = set(x & 0x3fffffff for x in entries if (x >> 31) & 1)
    # every entry must point at "ldr rD,[rN,#0]" (the loader ORs the GOTT index into the 12-bit offset)
    bogus = []
    for a in sorted(idx_entries):
        off = e.vaddr_to_off(a)
        w = struct.unpack_from("<I", e.d, off)[0] if off is not None else 0
        if (w & 0xfff00fff) != 0xe5900000:
            bogus.append(a)
    if bogus:
        errs.append(f"pic: {len(bogus)} entries do not point at 'ldr rD,[rN]': {[hex(x) for x in bogus[:5]]}")
    if pic or not isexe:
        miss = sorted(sites - idx_entries)
        if miss:
            errs.append(f"pic: {len(miss)} PIC-register load sites have no .riscos.pic entry: {[hex(x) for x in miss[:5]]}")
        if len([x for x in entries if (x >> 30) & 3 == 0]):
            notes.append("pic: has __GOTT_BASE__ style entries")
    notes.append(f"pic sites(found)={len(sites)} entries={len(entries)}")

    # plt
    plt = e.byname.get(".plt")
    jmprel = e.byname.get(".rel.plt")
    if plt and plt[5]:
        hdr = 20
        esz = 12 if isexe else 20
        n = (plt[5] - hdr) // esz
        slots = {}
        for i in range(n):
            a = plt[3] + hdr + i * esz
            o = plt[4] + hdr + i * esz
            w = struct.unpack_from("<%dI" % (esz // 4), e.d, o)
            if isexe:
                if (w[0] & 0xfffff000) != 0xe28fc000 or (w[1] & 0xfffff000) != 0xe28cc000 or (w[2] & 0xfffff000) != 0xe5bcf000:
                    errs.append(f"plt: entry {i} not an exec PLT stub: {[hex(x) for x in w]}")
                    continue
                def rot(insn):
                    v = insn & 0xff
                    r = ((insn >> 8) & 15) * 2
                    return ((v >> r) | (v << (32 - r))) & 0xffffffff if r else v
                slot = a + 8 + rot(w[0]) + rot(w[1]) + (w[2] & 0xfff)
            else:
                if w[0] != 0xe3a0c902 or w[1] != 0xe59cc038 or (w[2] & 0xfffff000) != 0xe59cc000 or (w[3] & 0xfffff000) != 0xe28cc000 or (w[4] & 0xfffff000) != 0xe5bcf000:
                    errs.append(f"plt: entry {i} not a shared PLT stub: {[hex(x) for x in w]}")
                    continue
                def rot(insn):
                    v = insn & 0xff
                    r = ((insn >> 8) & 15) * 2
                    return ((v >> r) | (v << (32 - r))) & 0xffffffff if r else v
                slot = (pg or 0) + rot(w[3]) + (w[4] & 0xfff)
            slots[slot] = i
        rels = {}
        if jmprel:
            for o in range(jmprel[4], jmprel[4] + jmprel[5], 8):
                r_off, r_info = struct.unpack_from("<II", e.d, o)
                if (r_info & 0xff) == R_ARM_JUMP_SLOT:
                    rels[r_off] = e.dynsym_name(r_info >> 8)
        if set(slots) != set(rels):
            errs.append(f"plt: stub slots != JUMP_SLOT relocs (stubs={len(slots)} relocs={len(rels)}; "
                        f"only-stub={[hex(x) for x in sorted(set(slots)-set(rels))[:4]]} only-rel={[hex(x) for x in sorted(set(rels)-set(slots))[:4]]})")
        # lazy-binding initial GOT contents: PLT0 address
        for slot in slots:
            off = e.vaddr_to_off(slot)
            if off is None:
                errs.append(f"plt: slot {slot:#x} outside file image")
                break
            if struct.unpack_from("<I", e.d, off)[0] != plt[3]:
                errs.append(f"plt: GOT slot {slot:#x} initial value != PLT0 {plt[3]:#x}")
                break
        notes.append(f"plt entries={n}")
    return errs, notes


def main():
    rc = 0
    for f in sys.argv[1:]:
        try:
            errs, notes = check(f)
        except Exception as ex:  # noqa
            errs, notes = [f"exception {ex!r}"], []
        print(("FAIL " if errs else "ok   ") + f + "  [" + "; ".join(notes) + "]")
        for x in errs:
            print("     " + x)
        rc |= bool(errs)
    sys.exit(rc)


if __name__ == "__main__":
    main()
