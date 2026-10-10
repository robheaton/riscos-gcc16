"""a32.py - a small ARM (A32) interpreter for the test scripts of the upstream bundle (and of my port's tools): a static ELF image (the PT_LOAD segments are read-only), memory
outside the image is an overlay that can be written, the instructions that GCC's -O2 code of small functions uses (data processing, MOVW / MOVT, MUL / MLA, LDR / STR / LDRB /
STRB, LDM / STM, B / BL / BX, conditions), SWIs go to a hook.  It runs the code a compiler generated, not a model of it.  Python 3, no other dependency.
  Elf (path)        .syms (name -> address of the static image), .read32 (va)
  Cpu (elf, max_steps=5000)   .r[0..15], .n .z .c .v, .swi_hook (cpu, swi_number), .rd8 / .wr8 / .rd32 / .wr32, .run (start, stop): runs until pc == stop; Fault after max_steps"""
import struct

# ---------------------------------------------------------------- ELF (static image)
class Elf:
    def __init__(self, path):
        d = self.d = open(path, "rb").read()
        self.wsegs = []                                  # (va, memsz) of the writable segments (.data, .bss): stores go to the overlay
        assert d[:4] == b"\x7fELF"
        e_phoff, e_shoff = struct.unpack_from("<II", d, 28)
        e_phentsize, e_phnum, e_shentsize, e_shnum = struct.unpack_from("<HHHH", d, 42)
        self.segs = []
        for i in range(e_phnum):
            p_type, p_off, p_va, p_pa, p_fsz, p_msz, p_fl, p_al = struct.unpack_from("<IIIIIIII", d, e_phoff + i * e_phentsize)
            if p_type == 1:
                self.segs.append((p_va, p_fsz, p_msz, p_off))
                if p_fl & 2: self.wsegs.append((p_va, p_msz))
        self.syms = {}
        secs = [struct.unpack_from("<IIIIIIIIII", d, e_shoff + i * e_shentsize) for i in range(e_shnum)]
        for (name, typ, fl, addr, off, size, link, info, al, entsize) in secs:
            if typ == 2:
                stroff = secs[link][4]
                for k in range(size // 16):
                    st_name, st_value, st_size, st_info, st_other, st_shndx = struct.unpack_from("<IIIBBH", d, off + k * 16)
                    e = d.index(b"\0", stroff + st_name)
                    nm = d[stroff + st_name:e].decode()
                    if nm and nm not in self.syms: self.syms[nm] = st_value
    def writable(self, va):
        return any(v <= va < v + n for v, n in self.wsegs)
    def read32(self, va):
        for (v, fsz, msz, off) in self.segs:
            if v <= va < v + msz:
                return struct.unpack_from("<I", self.d, off + (va - v))[0] if va - v + 4 <= fsz else 0
        return None

class Fault(Exception): pass
M = 0xFFFFFFFF

class Cpu:
    def __init__(self, elf, max_steps=5000):
        self.elf = elf; self.r = [0] * 16; self.n = self.z = self.c = self.v = 0; self.mem = {}; self.swi_hook = None; self.steps = 0; self.max_steps = max_steps
        self.mode = 0x13; self.cpsr_ctl = 0x13                     # SVC32, interrupts enabled; r13 / r14 are banked per mode (USR 0x10, IRQ 0x12, SVC 0x13)
        self.bank = {0x10: [0, 0], 0x12: [0, 0], 0x13: [0, 0]}
    def set_mode(self, new):
        new &= 0x1F
        if new not in self.bank: raise Fault("processor mode %#x is not modelled" % new)
        if new != self.mode:
            self.bank[self.mode] = [self.r[13], self.r[14]]
            self.mode = new; self.r[13], self.r[14] = self.bank[new]
    def rd32(self, a):
        if a & 3: raise Fault("unaligned read %08x" % a)
        if any((a + i) in self.mem for i in range(4)):          # a stored byte (overlay) wins over the image
            return sum(self.rd8(a + i) << (8 * i) for i in range(4))
        w = self.elf.read32(a)
        if w is not None: return w
        return 0
    def wr32(self, a, val):
        if a & 3: raise Fault("unaligned write %08x" % a)
        if self.elf.read32(a) is not None and not self.elf.writable(a): raise Fault("write into the image %08x" % a)
        for i in range(4): self.mem[a + i] = (val >> (8 * i)) & 0xFF
    def rd8(self, a):
        if a in self.mem: return self.mem[a]
        w = self.elf.read32(a & ~3)
        if w is not None: return (w >> (8 * (a & 3))) & 0xFF
        return 0
    def wr8(self, a, val):
        if self.elf.read32(a & ~3) is not None and not self.elf.writable(a): raise Fault("write into the image %08x" % a)
        self.mem[a] = val & 0xFF
    def cond(self, cc):
        n, z, c, v = self.n, self.z, self.c, self.v
        return [z == 1, z == 0, c == 1, c == 0, n == 1, n == 0, v == 1, v == 0, c == 1 and z == 0, c == 0 or z == 1, n == v, n != v, z == 0 and n == v, z == 1 or n != v, True, False][cc]
    def reg(self, i): return (self.r[15] + 4) & M if i == 15 else self.r[i]
    def shift(self, val, typ, amt, carry, byreg=False):
        if byreg:                                                                  # the amount is the low byte of a register: 0 = no shift at all (the carry is kept), 32 and more are defined
            if amt == 0: return val, carry
            if typ == 0:
                if amt < 32: carry = (val >> (32 - amt)) & 1; val = (val << amt) & M
                elif amt == 32: carry = val & 1; val = 0
                else: carry = 0; val = 0
            elif typ == 1:
                if amt < 32: carry = (val >> (amt - 1)) & 1; val >>= amt
                elif amt == 32: carry = (val >> 31) & 1; val = 0
                else: carry = 0; val = 0
            elif typ == 2:
                if amt >= 32: carry = (val >> 31) & 1; val = M if carry else 0
                else:
                    s = val - (1 << 32) if val >> 31 else val
                    carry = (s >> (amt - 1)) & 1; val = (s >> amt) & M
            else:
                a = amt & 31
                if a == 0: carry = (val >> 31) & 1
                else: val = ((val >> a) | (val << (32 - a))) & M; carry = val >> 31
            return val, carry
        if typ == 0:
            if amt: carry = (val >> (32 - amt)) & 1; val = (val << amt) & M
        elif typ == 1:
            if amt == 0: carry = (val >> 31) & 1; val = 0
            else: carry = (val >> (amt - 1)) & 1; val >>= amt
        elif typ == 2:
            if amt == 0: amt = 32
            s = val - (1 << 32) if val >> 31 else val
            carry = (s >> (amt - 1)) & 1; val = (s >> min(amt, 31)) & M
        else:
            if amt == 0: carry = val & 1; val = ((self.c << 31) | (val >> 1)) & M
            else: val = ((val >> amt) | (val << (32 - amt))) & M; carry = val >> 31
        return val, carry
    def step(self):
        pc = self.r[15]
        w = self.elf.read32(pc)
        if w is None: raise Fault("fetch from %08x" % pc)
        self.r[15] = pc + 4
        self.steps += 1
        if self.steps > self.max_steps: raise Fault("too many steps")
        if (w & 0xFFF1FE20) == 0xF1000000:                                   # CPSIE / CPSID (imod 2 / 3) of the I and F bits; in USER mode they do nothing; "CPS #mode" is not modelled
            imod = (w >> 18) & 3
            if (w >> 17) & 1 or imod in (0, 1): raise Fault("CPS form not modelled: %08x at %08x" % (w, pc))
            if self.mode != 0x10:
                bits = (0x40 if (w >> 6) & 1 else 0) | (0x80 if (w >> 7) & 1 else 0)
                self.cpsr_ctl = self.cpsr_ctl & ~bits if imod == 2 else self.cpsr_ctl | bits
            return
        if w == 0xF57FF01F: self.excl = None; return                           # CLREX
        cc = w >> 28
        if not self.cond(cc): return
        top = (w >> 25) & 7
        if (w & 0x0FFFFFF0) == 0x012FFF10:                                   # BX
            self.r[15] = self.reg(w & 15) & ~1; return
        if (w & 0x0FFFFFF0) == 0x012FFF30:                                   # BLX Rm
            tgt = self.reg(w & 15) & ~1; self.r[14] = self.r[15]; self.r[15] = tgt; return
        if (w & 0x0FB0F000) == 0x0320F000 and not (w >> 22) & 1:             # MSR CPSR_<fields>, #imm  (the flags and the control field: mode and I / F bits)
            imm = w & 0xFF; rot = ((w >> 8) & 15) * 2
            val = ((imm >> rot) | (imm << (32 - rot))) & M if rot else imm
            self.msr(w, val); return
        if (w & 0x0FB0FFF0) == 0x0120F000 and not (w >> 22) & 1:             # MSR CPSR_<fields>, Rm
            self.msr(w, self.reg(w & 15)); return
        if top in (0, 1) and (w & 0x0FF00000) in (0x03000000, 0x03400000):    # MOVW / MOVT
            imm = ((w >> 4) & 0xF000) | (w & 0xFFF); rd = (w >> 12) & 15
            if (w & 0x0FF00000) == 0x03000000: self.r[rd] = imm
            else: self.r[rd] = (self.r[rd] & 0xFFFF) | (imm << 16)
            return
        if top == 0 and (w & 0x0F800FF0) == 0x01800F90 or top == 0 and (w & 0x0F800FFF) == 0x01800F9F:      # LDREX / STREX and the B, H, D forms (ARMv6, v6K): one processor, one monitor
            size = {0: 4, 1: 8, 2: 1, 3: 2}[(w >> 21) & 3]; rn = (w >> 16) & 15; rd = (w >> 12) & 15; addr = self.reg(rn)
            if addr & (size - 1 if size < 8 else 7): raise Fault("unaligned exclusive access %08x" % addr)
            if (w >> 20) & 1:                                                 # LDREX*: load and open the monitor
                if size == 4: self.r[rd] = self.rd32(addr)
                elif size == 1: self.r[rd] = self.rd8(addr)
                elif size == 2: self.r[rd] = self.rd8(addr) | (self.rd8(addr + 1) << 8)
                else: self.r[rd] = self.rd32(addr); self.r[rd + 1] = self.rd32(addr + 4)
                self.excl = (addr, size)
            else:                                                             # STREX* Rd, Rm, [Rn]: stores when the monitor is open for this address; Rd = 0 for done, 1 for not done
                rm = w & 15; ok = getattr(self, "excl", None) == (addr, size)
                if ok:
                    v = self.reg(rm)
                    if size == 4: self.wr32(addr, v)
                    elif size == 1: self.wr8(addr, v & 0xFF)
                    elif size == 2: self.wr8(addr, v & 0xFF); self.wr8(addr + 1, (v >> 8) & 0xFF)
                    else: self.wr32(addr, v); self.wr32(addr + 4, self.reg(rm + 1))
                self.r[(w >> 12) & 15] = 0 if ok else 1
                self.excl = None
            return
        if top == 0 and (w & 0x0F8000F0) == 0x00800090:                       # UMULL / UMLAL / SMULL / SMLAL
            rdhi = (w >> 16) & 15; rdlo = (w >> 12) & 15; rs = (w >> 8) & 15; rm = w & 15
            a, b = self.reg(rm), self.reg(rs)
            if (w >> 22) & 1:                                                 # signed
                a = a - (1 << 32) if a >> 31 else a; b = b - (1 << 32) if b >> 31 else b
            res = a * b
            if (w >> 21) & 1: res += (self.r[rdhi] << 32) | self.r[rdlo]
            res &= (1 << 64) - 1
            self.r[rdlo] = res & M; self.r[rdhi] = (res >> 32) & M
            if (w >> 20) & 1: self.n = res >> 63; self.z = 1 if res == 0 else 0
            return
        if top == 0 and (w & 0xF0) == 0x90:                                   # MUL / MLA
            rd = (w >> 16) & 15; rn = (w >> 12) & 15; rs = (w >> 8) & 15; rm = w & 15
            res = (self.reg(rm) * self.reg(rs) + (self.reg(rn) if (w >> 21) & 1 else 0)) & M
            self.r[rd] = res; return
        if top == 0 and (w & 0x0F900090) == 0x01000080:                       # the DSP multiplies of ARMv5E / ARMv6: SMLAxy, SMLAWy / SMULWy, SMLALxy, SMULxy
            op = (w >> 21) & 3; rd = (w >> 16) & 15; ra = (w >> 12) & 15; rm = (w >> 8) & 15; rn = w & 15; x = (w >> 5) & 1; y = (w >> 6) & 1
            def half(v, hi):
                h = (v >> 16) & 0xFFFF if hi else v & 0xFFFF
                return h - 0x10000 if h & 0x8000 else h
            def sg(v): return v - (1 << 32) if v >> 31 else v
            if op == 3: self.r[rd] = (half(self.reg(rn), x) * half(self.reg(rm), y)) & M                 # SMULxy
            elif op == 0: self.r[rd] = (half(self.reg(rn), x) * half(self.reg(rm), y) + sg(self.reg(ra))) & M     # SMLAxy
            elif op == 1:                                                                                   # SMLAWy (x = 0) / SMULWy (x = 1)
                prod = (sg(self.reg(rn)) * half(self.reg(rm), y)) >> 16
                self.r[rd] = (prod + (0 if x else sg(self.reg(ra)))) & M
            else:                                                                                           # SMLALxy
                acc = (self.r[rd] << 32) | self.r[ra]
                if acc >> 63: acc -= 1 << 64
                acc += half(self.reg(rn), x) * half(self.reg(rm), y); acc &= (1 << 64) - 1
                self.r[ra] = acc & M; self.r[rd] = (acc >> 32) & M
            return
        if top == 0 and (w & 0x90) == 0x90 and (w & 0x60) != 0:                # halfword / signed byte / doubleword transfers: LDRH STRH LDRSB LDRSH LDRD STRD (ARMv4, ARMv5TE)
            p = (w >> 24) & 1; u = (w >> 23) & 1; imm = (w >> 22) & 1; wb = (w >> 21) & 1; ld = (w >> 20) & 1; rn = (w >> 16) & 15; rd = (w >> 12) & 15; sh = (w >> 5) & 3
            off = (((w >> 8) & 15) << 4 | (w & 15)) if imm else self.reg(w & 15)
            base = self.reg(rn); naddr = (base + off if u else base - off) & M; addr = naddr if p else base
            if ld or sh == 1:                                                  # LDRH / STRH / LDRSB / LDRSH (ld = 1); STRH (ld = 0, sh = 1)
                if sh in (1, 3) and addr & 1: raise Fault("unaligned halfword access %08x" % addr)
                if ld:
                    if sh == 1: val = self.rd8(addr) | (self.rd8(addr + 1) << 8)
                    elif sh == 3:
                        val = self.rd8(addr) | (self.rd8(addr + 1) << 8)
                        if val & 0x8000: val |= 0xFFFF0000
                    else:
                        val = self.rd8(addr)
                        if val & 0x80: val |= 0xFFFFFF00
                    if rd == 15: self.r[15] = val
                    else: self.r[rd] = val
                else:
                    v = self.reg(rd); self.wr8(addr, v & 0xFF); self.wr8(addr + 1, (v >> 8) & 0xFF)
            else:                                                              # LDRD (sh = 2) / STRD (sh = 3) with ld = 0
                if addr & 3: raise Fault("unaligned doubleword access %08x" % addr)
                if sh == 2: self.r[rd] = self.rd32(addr); self.r[rd + 1] = self.rd32(addr + 4)
                else: self.wr32(addr, self.reg(rd)); self.wr32(addr + 4, self.reg(rd + 1))
            if not p: self.r[rn] = naddr
            elif wb: self.r[rn] = naddr
            return
        if (w & 0x0FFF0FF0) == 0x016F0F10:                                    # CLZ
            v = self.reg(w & 15); self.r[(w >> 12) & 15] = 32 - v.bit_length(); return
        if (w & 0x0F8000F0) == 0x06800070 and ((w >> 20) & 7) in (2, 3, 6, 7):  # the ARMv6 extends: SXTB / SXTH / UXTB / UXTH and the accumulating SXTAB / SXTAH / UXTAB / UXTAH
            op = (w >> 20) & 7; rn = (w >> 16) & 15; rd = (w >> 12) & 15; rot = ((w >> 10) & 3) * 8; v = self.reg(w & 15)
            if rot: v = ((v >> rot) | (v << (32 - rot))) & M
            if op in (2, 6): v &= 0xFF
            else: v &= 0xFFFF
            if op in (2, 3):                                                  # signed
                bits = 8 if op == 2 else 16
                if v >> (bits - 1): v -= 1 << bits
            self.r[rd] = (v + (self.reg(rn) if rn != 15 else 0)) & M; return
        if (w & 0x0FFF0FF0) in (0x06BF0F30, 0x06BF0FB0, 0x06FF0FB0):          # REV / REV16 / REVSH
            v = self.reg(w & 15); rd = (w >> 12) & 15
            if (w & 0x0FFF0FF0) == 0x06BF0F30: self.r[rd] = int.from_bytes(v.to_bytes(4, "little"), "big")
            elif (w & 0x0FFF0FF0) == 0x06BF0FB0: self.r[rd] = (((v & 0x00FF00FF) << 8) | ((v >> 8) & 0x00FF00FF)) & M
            else:
                h = ((v & 0xFF) << 8) | ((v >> 8) & 0xFF)
                self.r[rd] = (h - 0x10000 if h & 0x8000 else h) & M
            return
        if top == 0 and (w & 0x0FBF0FFF) == 0x010F0000:                       # MRS Rd, CPSR (the mode is always SVC32: the module runs there)
            self.r[(w >> 12) & 15] = (self.n << 31) | (self.z << 30) | (self.c << 29) | (self.v << 28) | (self.cpsr_ctl & ~0x1F) | self.mode
            return
        if top in (0, 1):
            op = (w >> 21) & 15; s = (w >> 20) & 1; rn = (w >> 16) & 15; rd = (w >> 12) & 15
            if top == 1:
                imm = w & 0xFF; rot = ((w >> 8) & 15) * 2
                op2 = ((imm >> rot) | (imm << (32 - rot))) & M if rot else imm
                sc = (op2 >> 31) & 1 if rot else self.c
            else:
                if w & 0x10: op2, sc = self.shift(self.reg(w & 15), (w >> 5) & 3, self.reg((w >> 8) & 15) & 0xFF, self.c, True)
                else: op2, sc = self.shift(self.reg(w & 15), (w >> 5) & 3, (w >> 7) & 31, self.c)
            a = self.reg(rn)
            def add(x, y, cin):
                t = x + y + cin; res = t & M
                return res, 1 if t > M else 0, 1 if (~(x ^ y) & (x ^ res)) >> 31 else 0
            logical = op in (0, 1, 8, 9, 12, 13, 14, 15)
            cflag, vflag = sc, self.v
            if   op == 0:  res = a & op2
            elif op == 1:  res = a ^ op2
            elif op == 2:  res, cflag, vflag = add(a, ~op2 & M, 1)
            elif op == 3:  res, cflag, vflag = add(op2, ~a & M, 1)
            elif op == 4:  res, cflag, vflag = add(a, op2, 0)
            elif op == 5:  res, cflag, vflag = add(a, op2, self.c)
            elif op == 6:  res, cflag, vflag = add(a, ~op2 & M, self.c)
            elif op == 7:  res, cflag, vflag = add(op2, ~a & M, self.c)
            elif op == 8:  res = a & op2
            elif op == 9:  res = a ^ op2
            elif op == 10: res, cflag, vflag = add(a, ~op2 & M, 1)
            elif op == 11: res, cflag, vflag = add(a, op2, 0)
            elif op == 12: res = a | op2
            elif op == 13: res = op2
            elif op == 14: res = a & ~op2 & M
            else:          res = ~op2 & M
            if s:
                self.n = res >> 31; self.z = 1 if res == 0 else 0; self.c = cflag
                if not logical: self.v = vflag
            if op not in (8, 9, 10, 11):
                if rd == 15: self.r[15] = res
                else: self.r[rd] = res
        elif top in (2, 3):
            if top == 3 and (w & 0x10): raise Fault("media instruction %08x" % w)
            p = (w >> 24) & 1; u = (w >> 23) & 1; b = (w >> 22) & 1; wb = (w >> 21) & 1; ld = (w >> 20) & 1
            rn = (w >> 16) & 15; rd = (w >> 12) & 15
            if top == 2: off = w & 0xFFF
            else: off, _ = self.shift(self.reg(w & 15), (w >> 5) & 3, (w >> 7) & 31, self.c)
            base = self.reg(rn)
            addr = (base + off if u else base - off) & M if p else base
            if ld:
                val = self.rd8(addr) if b else self.rd32(addr)
                if rd == 15: self.r[15] = val
                else: self.r[rd] = val
            else:
                if b: self.wr8(addr, self.reg(rd))
                else: self.wr32(addr, self.reg(rd))
            if not p: self.r[rn] = (base + off if u else base - off) & M
            elif wb: self.r[rn] = addr
        elif top == 4:
            p = (w >> 24) & 1; u = (w >> 23) & 1; wb = (w >> 21) & 1; ld = (w >> 20) & 1
            rn = (w >> 16) & 15; rl = [i for i in range(16) if (w >> i) & 1]
            n = len(rl); base = self.reg(rn)
            start = (base + (4 if p else 0)) if u else (base - 4 * n + (0 if p else 4))
            new = base + 4 * n if u else base - 4 * n
            for k, i in enumerate(rl):
                a = (start + 4 * k) & M
                if ld:
                    val = self.rd32(a)
                    if i == 15: self.r[15] = val
                    else: self.r[i] = val
                else: self.wr32(a, self.reg(i) if i != 15 else (pc + 8) & M)
            if wb: self.r[rn] = new & M
        elif top == 5:
            off = w & 0xFFFFFF
            if off & 0x800000: off -= 1 << 24
            if (w >> 24) & 1: self.r[14] = self.r[15]
            self.r[15] = (pc + 8 + (off << 2)) & M
        elif (w & 0x0FFF0FFF) in (0x0E070F9A, 0x0E070FBA, 0x0E070F94):      # the ARMv6 barriers: MCR p15, 0, Rt, c7, c10, 4 (DSB) and c10, 5 (DMB), c7, c5, 4 (the prefetch flush, ISB): one CPU, nothing to wait for
            pass
        elif top == 7 and (w >> 24) & 1:
            self.swi_hook(self, w & 0xFFFFFF)
        else:
            raise Fault("instruction not modelled: %08x at %08x" % (w, pc))
    def msr(self, w, val):
        if (w >> 19) & 1: self.n = (val >> 31) & 1; self.z = (val >> 30) & 1; self.c = (val >> 29) & 1; self.v = (val >> 28) & 1
        if (w >> 16) & 1 and self.mode != 0x10:                              # the control field: the mode (and with it the banked r13 / r14) and the I / F bits; USER mode cannot write it (the write is ignored)
            self.set_mode(val & 0x1F); self.cpsr_ctl = val & 0xFF
    def run(self, start, stop):
        """run from START until the pc is STOP (an address, or a collection of addresses)"""
        stops = stop if isinstance(stop, (set, frozenset, tuple, list)) else (stop,)
        self.r[15] = start
        while self.r[15] not in stops: self.step()



class FlatImage:
    """a raw memory image loaded at BASE (a module in the RMA): everything in it is readable and writable (stores go to the overlay of the Cpu), nothing is a symbol table"""
    def __init__(self, data, base, syms=None):
        self.d = bytes(data); self.base = base; self.syms = dict(syms or {}); self.segs = [(base, len(data), len(data), 0)]; self.wsegs = [(base, len(data))]
    def writable(self, va):
        return self.base <= va < self.base + len(self.d)
    def read32(self, va):
        if self.base <= va and va + 4 <= self.base + len(self.d):
            return struct.unpack_from("<I", self.d, va - self.base)[0]
        if self.base <= va < self.base + len(self.d):                       # a word that straddles the end: pad with zeros
            return struct.unpack("<I", (self.d[va - self.base:] + b"\0\0\0\0")[:4])[0]
        return None
