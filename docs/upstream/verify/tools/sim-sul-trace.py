#!/usr/bin/env python3
"""Check the TRACE code of a SULTRACE build of the SharedUnixLibrary module (raw module file + its .o for the symbols) on the small ARM (A32) interpreter of sim-startup-loops.py, before it goes to a
machine that it could freeze:
  sul_trace      called with r0 = a tag, lr = a return address, trace_val = a value: must append exactly
                     "<tag> v=<8 hex> sp=<the caller's sp> ctx=<VFPSupport_ActiveContext, ffffffff when the SWI fails> t=<OS_ReadMonotonicTime> p=<[0x8038]> g=<[p], ffffffff when OS_ValidateAddress says
                      no or fails> c=<[0x8040]> h=<sum of the 64 words at 0x8000>\\n"; a checksum of a region replaces v when the statics trace_sum_a / trace_sum_n ask for one (not when the base is 0)
                 to the file with OS_Find (0xC0, name) / OS_Args 2 / OS_Args 1 / OS_GBPB 2 / OS_Find 0 and nothing else, make every SWI call with sp = the PRIVATE stack (never the caller's), and give back
                 r1-r12, sp and lr unchanged (r0 and the flags are the macro's business); when OS_Find fails (V set) or gives the handle 0 it must stop quietly with the same registers.
  a TRACE site   (the macro, at the first site: the start of sul_fork): every register r0-r12, sp and lr is unchanged after it, whatever sp is (also an invalid one), and the code goes on at the instruction
                 after the inline text.
  every site     of the module: the instruction before it does not set flags that the instruction after it uses (the macro corrupts the flags), and the registers the surrounding code needs are not the ones the
                 macro corrupts (r0 and lr are restored; the value register is only read).
usage: sim-sul-trace.py MODULE.bin MODULE.o        (exit status 0 = every scenario right)"""
import importlib.util, os, struct, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("simstart", os.path.join(here, "sim-startup-loops.py"))
simstart = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(simstart)
Cpu, Fault = simstart.Cpu, simstart.Fault
M = 0xFFFFFFFF

class Raw:                                   # the raw module as an "ELF" for the interpreter: one image at address 0
    def __init__(self, path):
        self.d = open(path, "rb").read(); self.segs = [(0, len(self.d), len(self.d), 0)]
    def read32(self, va):
        return struct.unpack_from("<I", self.d, va)[0] if 0 <= va and va + 4 <= len(self.d) else None

class TSim(Cpu):
    """what the trace code needs on top of the base interpreter: ROR/RRX, byte loads and stores, MOV pc, lr, and writes anywhere (also into the image: the module's own static data) through an overlay"""
    def __init__(self, raw):
        super().__init__(raw); self.over = {}
    def img8(self, a):
        w = self.elf.read32(a & ~3)
        return None if w is None else (w >> (8 * (a & 3))) & 0xFF
    def rd8(self, a):
        if a in self.over: return self.over[a]
        b = self.img8(a)
        if b is None: raise Fault("read of unmapped byte %08x" % a)
        return b
    def wr8(self, a, v): self.over[a] = v & 0xFF
    def rd32(self, a):
        if a & 3: raise Fault("unaligned read %08x" % a)
        return sum(self.rd8(a + i) << (8 * i) for i in range(4))
    def wr32(self, a, val):
        if a & 3: raise Fault("unaligned write %08x" % a)
        for i in range(4): self.over[a + i] = (val >> (8 * i)) & 0xFF
    def shift_imm(self, rm, typ, amt):
        if typ == 3:
            val = self.reg(rm)
            if amt == 0: carry = val & 1; val = (val >> 1) | (self.c << 31)                       # RRX
            else: val = ((val >> amt) | (val << (32 - amt))) & M; carry = (val >> 31) & 1         # ROR
            return val, carry
        return super().shift_imm(rm, typ, amt)
    def step(self):
        pc = self.r[15]; w = self.elf.read32(pc)
        if w is not None:
            if (w & 0x0FFFFFF0) == 0x01A0F000 and (w & 15) != 15:                                    # MOV pc, Rm  (MOV pc, lr; MOV pc, ip)
                self.steps += 1; self.r[15] = (self.r[w & 15] & ~3) & M if self.cond(w >> 28) else pc + 4; return
            if ((w >> 25) & 7) == 2 and (w >> 22) & 1:                                              # LDRB / STRB with an immediate offset
                self.steps += 1; self.r[15] = pc + 4
                if not self.cond(w >> 28): return
                p, u, wb, ld = (w >> 24) & 1, (w >> 23) & 1, (w >> 21) & 1, (w >> 20) & 1
                rn, rd, off = (w >> 16) & 15, (w >> 12) & 15, w & 0xFFF
                base = (pc + 8) & M if rn == 15 else self.r[rn]
                addr = ((base + off) if u else (base - off)) & M if p else base
                self.check_sp_access(rn, addr)
                if ld: self.set_reg(rd, self.rd8(addr))
                else: self.wr8(addr, self.r[rd] & 0xFF)
                if not p: self.set_reg(rn, ((base + off) if u else (base - off)) & M)
                elif wb: self.set_reg(rn, addr)
                return
        super().step()

def symbols(o):
    out = subprocess.run([os.environ.get("NM", "nm"), o], capture_output=True, text=True).stdout
    d = {}
    for ln in out.splitlines():
        f = ln.split()
        if len(f) == 3: d[f[2]] = int(f[0], 16)
    return d

SENT = 0xFFFF0000
TAGADDR = 0x50000000
XOS_Find, XOS_Args, XOS_GBPB, XOS_RMT, XVFP_Active, XOS_Valid = 0x2000D, 0x20009, 0x2000C, 0x20042, 0x78EC6, 0x2003A

def cstr(cpu, a):
    s = bytearray()
    while True:
        b = cpu.rd8(a + len(s))
        if b == 0: return bytes(s)
        s.append(b)

P_VAL, G_VAL, C_VAL = 0x50002000, 0x6706CAFE, 0x20F16000
def page_words():                                   # the memory at 0x8000: 64 words, [0x8038] = P_VAL, [0x8040] = C_VAL (word 16 = 0x8040 is inside the 0x100 bytes: offset 0x40)
    w = {0x8000 + 4 * i: (0x11110000 + i * 0x101) & M for i in range(64)}
    w[0x8038] = P_VAL; w[0x8040] = C_VAL
    return w
def want_page_fields(valid=True):
    w = page_words(); h = sum(w.values()) & M
    return P_VAL, (G_VAL if valid else M), C_VAL, h

def run_trace(raw, syms, tag, val, find="ok", sp=0x7F000000 - 64, rmt=0x1234ABCD, ctx=0x6706AEE8, valid="ok", sum_a=0, sum_n=0, region=None):
    cpu = TSim(raw)
    for a, v in page_words().items():
        for i in range(4): cpu.over[a + i] = (v >> (8 * i)) & 0xFF
    for i in range(4): cpu.over[P_VAL + i] = (G_VAL >> (8 * i)) & 0xFF
    if region:
        for a, v in region.items():
            for i in range(4): cpu.over[a + i] = (v >> (8 * i)) & 0xFF
    cpu.wr32(syms["trace_sum_a"], sum_a); cpu.wr32(syms["trace_sum_n"], sum_n)
    cpu.stack_ok = (0x7E000000, 0x7F000000)
    for i, ch in enumerate(tag.encode() + b"\0"): cpu.over[TAGADDR + i] = ch
    cpu.r[0] = TAGADDR; cpu.r[14] = SENT; cpu.r[13] = sp
    for i in range(1, 13): cpu.r[i] = 0xC0DE0000 + i
    cpu.wr32(syms["trace_val"], val)
    log = []; written = bytearray(); state = {"sp_bad": False, "name": None}
    def swi(c, n):
        if c.r[13] != syms["trace_stack_top"]: state["sp_bad"] = True              # every SWI must be made with the PRIVATE stack
        log.append((n, c.r[0], c.r[1], c.r[2], c.r[3]))
        c.v = 0
        if n == XOS_RMT: c.r[0] = rmt
        elif n == XOS_Valid:
            if valid == "err": c.v = 1; c.r[0] = 0x1234
            else: c.c = 0 if valid == "ok" else 1
        elif n == XVFP_Active:
            if ctx is None: c.v = 1; c.r[0] = 0x1234              # VFPSupport is not there: the SWI fails
            else: c.r[0] = ctx
        elif n == XOS_Find and c.r[0] == 0xC0:
            state["name"] = cstr(c, c.r[1])
            if find == "err": c.v = 1; c.r[0] = 0
            elif find == "zero": c.r[0] = 0
            else: c.r[0] = 77
        elif n == XOS_Args and c.r[0] == 2: c.r[2] = 100
        elif n == XOS_GBPB:
            for i in range(c.r[3]): written.append(c.rd8(c.r[2] + i))
        else: pass
    cpu.swi_hook = swi
    cpu.run(syms["sul_trace"], {SENT})
    state["sum_n_after"] = cpu.rd32(syms["trace_sum_n"])
    kept = all(cpu.r[i] == 0xC0DE0000 + i for i in range(1, 13)) and cpu.r[13] == sp and cpu.r[15] == SENT and cpu.r[14] == SENT
    return bytes(written), log, kept, state["sp_bad"], state["name"], state["sum_n_after"]

def main():
    if len(sys.argv) != 3: sys.exit(__doc__)
    raw = Raw(sys.argv[1]); syms = symbols(sys.argv[2])
    for n in ("sul_trace", "trace_val", "trace_stack_top", "sul_fork", "trace_name", "trace_sum_a", "trace_sum_n"):
        if n not in syms: sys.exit("symbol %s not found in %s" % (n, sys.argv[2]))
    ok_all = True
    def rec(ok, msg):
        nonlocal ok_all
        ok_all &= ok; print(("  ok   " if ok else "  FAIL ") + msg)
    want_name = b"LanMan98::MyShare.$.SulLog"
    # -- the routine
    for tag, val in (("X1 sul_exit enter", 0x20F18414), ("F2 sul_fork returns as the child", 0), ("a" * 60, 0xFFFFFFFF), ("", 1), ("E4 before OS_CLI", 0x80000000), ("x", 0x0A1B2C3D)):
        w, log, kept, spbad, name, sumn = run_trace(raw, syms, tag, val)
        pf = want_page_fields()
        want = ("%s v=%08X sp=%08X ctx=%08X t=%08X p=%08X g=%08X c=%08X h=%08X\n" % ((tag, val, 0x7F000000 - 64, 0x6706AEE8, 0x1234ABCD) + pf)).encode()
        calls = [(n, r0) for (n, r0, r1, r2, r3) in log]
        seq = [(XOS_Valid, None), (XVFP_Active, None), (XOS_RMT, None), (XOS_Find, 0xC0), (XOS_Args, 2), (XOS_Args, 1), (XOS_GBPB, 2), (XOS_Find, 0)]
        seq_ok = len(calls) == 8 and all(c[0] == s[0] and (s[1] is None or c[1] == s[1]) for c, s in zip(calls, seq))
        # the arguments (log entries are (swi, r0, r1, r2, r3)): the handle in r1 everywhere, OS_Args 1 sets the pointer (r2) to the extent that OS_Args 2 gave (100), the GBPB writes the whole line, the file is closed
        args_ok = seq_ok and log[0][1] == P_VAL and log[0][2] == P_VAL + 4 and log[4][2] == 77 and log[5][2] == 77 and log[5][3] == 100 and log[6][2] == 77 and log[6][4] == len(want) and log[7][2] == 77
        good = w == want and seq_ok and args_ok and kept and not spbad and name == want_name
        rec(good, "sul_trace %r v=%08x: line %s, SWIs %s, arguments %s, file name %s, registers %s, private stack %s" % (
            tag[:20], val, "right" if w == want else "WRONG %r (expected %r)" % (w, want), "right" if seq_ok else "WRONG %s" % calls, "right" if args_ok else "WRONG %s" % log,
            "right" if name == want_name else "WRONG %r" % name, "kept" if kept else "DAMAGED", "used for every SWI" if not spbad else "NOT USED"))
    for mode in ("err", "zero"):
        w, log, kept, spbad, name, sumn = run_trace(raw, syms, "X1 sul_exit enter", 5, find=mode)
        good = w == b"" and [n for (n, *_) in log] == [XOS_Valid, XVFP_Active, XOS_RMT, XOS_Find] and kept and not spbad
        rec(good, "sul_trace when OS_Find %s: nothing written, no further SWI, registers kept" % ("fails" if mode == "err" else "gives the handle 0"))
    # an invalid caller sp must not matter: the routine never touches it
    for spv in (0x10, 0xFFFFFFF0, 0x7F000000 - 64):
        w, log, kept, spbad, name, sumn = run_trace(raw, syms, "X4 after stack free", 7, sp=spv)
        want = ("X4 after stack free v=00000007 sp=%08X ctx=6706AEE8 t=1234ABCD p=%08X g=%08X c=%08X h=%08X\n" % ((spv,) + want_page_fields())).encode()
        rec(w == want and kept and not spbad, "sul_trace with the caller's sp = %08x: works, logs that sp, sp unchanged afterwards" % spv)
    # VFPSupport absent (the SWI fails): ctx = ffffffff, the line is written all the same
    w, log, kept, spbad, name, sumn = run_trace(raw, syms, "X6 after copy_down_parent", 0x20F18414, ctx=None)
    rec(w == ("X6 after copy_down_parent v=20F18414 sp=7EFFFFC0 ctx=FFFFFFFF t=1234ABCD p=%08X g=%08X c=%08X h=%08X\n" % want_page_fields()).encode() and kept and not spbad, "sul_trace with VFPSupport absent: ctx=ffffffff, line written")
    # OS_ValidateAddress says "not valid" (C set) or fails (V set): g = ffffffff, the line is written all the same
    for mode in ("no", "err"):
        w, log, kept, spbad, name, sumn = run_trace(raw, syms, "G", 3, valid=mode)
        want = ("G v=00000003 sp=7EFFFFC0 ctx=6706AEE8 t=1234ABCD p=%08X g=%08X c=%08X h=%08X\n" % want_page_fields(valid=False)).encode()
        rec(w == want and kept and not spbad, "sul_trace when OS_ValidateAddress %s: g = ffffffff, the line is written" % ("says no" if mode == "no" else "fails"))
    # a checksum of a region replaces the value (and the request is cleared); a zero base or size asks for nothing
    region = {0x50010000 + 4 * i: (i * 0x01010101 + 7) & M for i in range(16)}
    want_sum = sum(region.values()) & M
    w, log, kept, spbad, name, sumn = run_trace(raw, syms, "S", 0x99, sum_a=0x50010000, sum_n=64, region=region)
    want = ("S v=%08X sp=7EFFFFC0 ctx=6706AEE8 t=1234ABCD p=%08X g=%08X c=%08X h=%08X\n" % ((want_sum,) + want_page_fields())).encode()
    rec(w == want and kept and not spbad and sumn == 0, "sul_trace with a region to sum (64 bytes): v = the sum %08x, the request is cleared" % want_sum)
    for sa, sn, what in ((0, 64, "base 0"), (0x50010000, 0, "size 0")):
        w, log, kept, spbad, name, sumn = run_trace(raw, syms, "S", 0x99, sum_a=sa, sum_n=sn, region=region)
        want = ("S v=00000099 sp=7EFFFFC0 ctx=6706AEE8 t=1234ABCD p=%08X g=%08X c=%08X h=%08X\n" % want_page_fields()).encode()
        rec(w == want and kept and not spbad and sumn == 0, "sul_trace with %s: nothing is summed, v is the given value" % what)
    # -- the macro at the first site (the start of sul_fork)
    start = syms["sul_fork"]
    a = start
    while raw.read32(a) != 0xE1A0C00D and a < start + 200: a += 4              # the first instruction of sul_fork itself:  MOV ip, sp
    for spv in (0x7F000000 - 64, 0x20, 0xFFFFFFF0):
        cpu = TSim(raw); cpu.stack_ok = (0x7E000000, 0x7F000000)
        for aa, v in page_words().items():
            for i in range(4): cpu.over[aa + i] = (v >> (8 * i)) & 0xFF
        for i in range(4): cpu.over[P_VAL + i] = (G_VAL >> (8 * i)) & 0xFF
        for i in range(0, 13): cpu.r[i] = 0xAB000000 + i
        cpu.r[13] = spv; cpu.r[14] = 0xCD000004
        def swi(c, n):
            c.v = 0
            if n == XOS_Valid: c.c = 0
            elif n == XOS_RMT: c.r[0] = 5
            elif n == XOS_Find and c.r[0] == 0xC0: c.r[0] = 3
            elif n == XOS_Args and c.r[0] == 2: c.r[2] = 0
        cpu.swi_hook = swi
        try:
            cpu.run(start, {a})
            same = all(cpu.r[i] == 0xAB000000 + i for i in range(0, 13)) and cpu.r[13] == spv and cpu.r[14] == 0xCD000004
            rec(same and cpu.r[15] == a, "TRACE site at sul_fork (caller sp %08x): r0-r12, sp and lr unchanged, execution goes on at the first real instruction (%x)" % (spv, a))
        except Fault as e:
            rec(False, "TRACE site at sul_fork (caller sp %08x): fault %s" % (spv, e))
    # -- every site of the module
    n = 0; bad = []
    sites = []
    for off in range(0, len(raw.d) - 4, 4):
        w = raw.read32(off)
        if (w >> 24) == 0xEB and ((off + 8 + (((w & 0xFFFFFF) ^ 0x800000) - 0x800000) * 4) & M) == syms["sul_trace"]: sites.append(off)
    def pcrel(addr, w):                                                           # the target of a  LDR/STR Rd, [pc, #+-imm]  at ADDR (None when it is not one)
        if (w & 0x0F7F0000) != 0x051F0000 and (w & 0x0F7F0000) != 0x050F0000: return None
        if (w & 0x0E000000) != 0x04000000 or not (w & (1 << 24)) or (w & (1 << 22)) or (w & (1 << 21)): return None
        if ((w >> 16) & 15) != 15: return None
        imm = w & 0xFFF
        return (addr + 8 + imm if (w >> 23) & 1 else addr + 8 - imm) & M
    def sets_flags(w):
        top = (w >> 25) & 7
        if top in (0, 1):
            if (w & 0x0FFFFFF0) == 0x012FFF10: return False                           # BX
            if top == 0 and (w & 0x90) == 0x90: return bool(w & (1 << 20))             # multiply / halfword transfer: the S bit is bit 20 for the multiplies
            op = (w >> 21) & 15
            return bool((w >> 20) & 1) or op in (8, 9, 10, 11)
        return False
    want_str = ((None, 0), ("trace_r0", 0), ("trace_lr", 14))                          # (target symbol, Rd) of the three STRs: the value register is free
    for off in sites:
        n += 1
        first = off - 16                                                              # the macro: STR val / STR r0 / STR lr / ADR r0,text (one instruction: 4 bytes, the strings are short) / BL / B / text / LDR r0 / LDR lr
        ins = [raw.read32(first + 4 * k) for k in range(3)]
        for k, (sym, rd) in enumerate(want_str):
            tg = pcrel(first + 4 * k, ins[k])
            if tg is None or (ins[k] >> 20) & 1: bad.append((first + 4 * k, "STR %d is not a pc-relative store" % (k + 1))); continue
            if (ins[k] >> 12 & 15) != rd and sym is not None: bad.append((first + 4 * k, "STR %d stores r%d, expected r%d" % (k + 1, (ins[k] >> 12) & 15, rd)))
            if tg != syms[sym or "trace_val"]: bad.append((first + 4 * k, "STR %d stores to %x, not to %s" % (k + 1, tg, sym or "trace_val")))
        bl_w = raw.read32(off); b = raw.read32(off + 4)
        if (b >> 24) != 0xEA: bad.append((off, "no B over the inline text after the BL")); continue
        tgt = (off + 4 + 8 + (((b & 0xFFFFFF) ^ 0x800000) - 0x800000) * 4) & M
        # the ADR before the BL must point at the text between the B and its target
        adr = raw.read32(off - 4)
        if (adr & 0x0FEF0000) not in (0x028F0000, 0x024F0000) or ((adr >> 12) & 15) != 0:
            bad.append((off - 4, "the instruction before the BL is not ADR r0")); continue
        imm = adr & 0xFF; rot = ((adr >> 8) & 15) * 2; imm = ((imm >> rot) | (imm << (32 - rot))) & M if rot else imm
        textaddr = (off - 4 + 8 + imm if (adr & 0x00800000) else off - 4 + 8 - imm) & M
        if textaddr != off + 8: bad.append((off - 4, "the ADR points at %x, not at the text after the B (%x)" % (textaddr, off + 8)))
        t = bytearray(); a2 = off + 8
        while a2 < tgt and raw.d[a2] != 0: t.append(raw.d[a2]); a2 += 1
        if not (0 < len(t) <= 110) or not (tgt - (off + 8)) >= len(t) + 1 or (tgt - (off + 8)) % 4: bad.append((off + 8, "the inline text %r does not end at the B target %x" % (bytes(t), tgt)))
        l0, l1 = raw.read32(tgt), raw.read32(tgt + 4)
        if pcrel(tgt, l0) != syms["trace_r0"] or ((l0 >> 12) & 15) != 0 or not (l0 >> 20) & 1: bad.append((tgt, "the first load after the text is not LDR r0, trace_r0"))
        if pcrel(tgt + 4, l1) != syms["trace_lr"] or ((l1 >> 12) & 15) != 14 or not (l1 >> 20) & 1: bad.append((tgt + 4, "the second load after the text is not LDR lr, trace_lr"))
        # flags: the instruction before the macro must not be a flag-setting one that the instruction after the macro depends on; and the macro itself changes the flags
        before = raw.read32(first - 4); after = raw.read32(tgt + 8)
        if (after >> 28) != 0xE: bad.append((first, "the instruction after the macro is conditional (%08x): it would test the flags of sul_trace" % after))
        elif False: pass
        if (after >> 28) == 0xE and sets_flags(before) and False: pass
    # a flag-setting instruction before the macro matters only if a conditional instruction later depends on it before the flags are set again: look at the next eight instructions after each macro
    for off in sites:
        b = raw.read32(off + 4); tgt = (off + 4 + 8 + (((b & 0xFFFFFF) ^ 0x800000) - 0x800000) * 4) & M
        a2 = tgt + 8
        for k in range(8):
            w = raw.read32(a2 + 4 * k)
            if (w >> 28) not in (0xE, 0xF): bad.append((a2 + 4 * k, "a conditional instruction (%08x) follows the macro at %x before the flags are set again: it would test the flags of sul_trace" % (w, off - 16))); break
            if sets_flags(w): break
            top = (w >> 25) & 7
            if top == 7 and (w >> 24) & 1: break                                     # a SWI sets V itself
            if top == 5: break                                                       # a branch ends the straight line
            if top == 4 and (w & (1 << 20)) and (w & (1 << 15)): break                # LDM with pc: a return
            if top in (0, 1) and ((w >> 12) & 15) == 15: break                        # MOV pc, ...: a return
            if (w & 0x0FFFFFF0) == 0x012FFF10: break                                  # BX
            if top in (2, 3) and (w & (1 << 20)) and ((w >> 12) & 15) == 15: break    # LDR pc, ...
    rec(not bad and n >= 12, "%d TRACE sites (BL sul_trace): shape right (B over the text, LDR r0 / LDR lr after it), no flags live across any%s" % (n, "" if not bad else ": " + "; ".join("%x: %s" % (o, m) for o, m in bad)))
    print("sul trace: %s" % ("all right" if ok_all else "FAILED"))
    return 0 if ok_all else 1
if __name__ == "__main__":
    try:
        sys.exit(main())
    except Fault as e:
        print("  FAIL simulation fault: %s" % e); sys.exit(1)
