#!/usr/bin/env python3
"""Run the machine code of the two start-up loops of a BUILT libunixlib.so (16.2.0-6 and later) on a small ARM (A32) interpreter and check what they do:

  stack_try  the main stack of an EABI program: __stack_size (bytes, weak) -> pages (at least 256 = 1 MB) -> StackOp ALLOC; no room -> half the size, down to 1 MB; not even
             1 MB -> exit with ERR_NO_STACK.  (Register-only: when it runs, r13 does not point at a usable stack yet; the interpreter makes ANY use of the stack pointer there
             a fault.)
  da_try     the heap dynamic area: OS_DynamicArea 0 with the maximum size from __dynamic_da_max_size / <prog>$HeapMax; no room -> half the size, down to 2 MB, then the plain SWI.
             The interpreter models a kernel that scrambles every register but sp after a FAILING OS_DynamicArea (so the loop may not trust anything to survive it).

The machine code is read from the library itself (symbols stack_try, stack_ok, da_try, da_last, da_done, __exit_with_error_num), the SWIs are modelled here.
usage: sim-startup-loops.py LIBUNIXLIB.so      (exit status 0 = every scenario right)"""
import struct, sys

# ---------------------------------------------------------------- ELF
class Elf:
    def __init__(self, path):
        d = self.d = open(path, "rb").read()
        assert d[:4] == b"\x7fELF" and d[4] == 1 and d[5] == 1, "not an ELF32 little-endian file"
        (self.e_phoff, self.e_shoff) = struct.unpack_from("<II", d, 28)
        (self.e_phentsize, self.e_phnum, self.e_shentsize, self.e_shnum, self.e_shstrndx) = struct.unpack_from("<HHHHH", d, 42)
        self.segs = []                                   # (vaddr, filesz, memsz, offset)
        for i in range(self.e_phnum):
            p_type, p_off, p_va, p_pa, p_fsz, p_msz, p_fl, p_al = struct.unpack_from("<IIIIIIII", d, self.e_phoff + i * self.e_phentsize)
            if p_type == 1:
                self.segs.append((p_va, p_fsz, p_msz, p_off))
        self.syms = {}
        secs = [struct.unpack_from("<IIIIIIIIII", d, self.e_shoff + i * self.e_shentsize) for i in range(self.e_shnum)]
        for (name, typ, fl, addr, off, size, link, info, al, entsize) in secs:
            if typ == 2:                                  # SHT_SYMTAB
                stroff = secs[link][4]
                for k in range(size // 16):
                    st_name, st_value, st_size, st_info, st_other, st_shndx = struct.unpack_from("<IIIBBH", d, off + k * 16)
                    e = d.index(b"\0", stroff + st_name)
                    nm = d[stroff + st_name:e].decode()
                    if nm and nm not in self.syms:
                        self.syms[nm] = st_value
    def read32(self, va):
        for (v, fsz, msz, off) in self.segs:
            if v <= va < v + msz:
                return struct.unpack_from("<I", self.d, off + (va - v))[0] if va - v + 4 <= fsz else 0
        return None

# ---------------------------------------------------------------- CPU
class Fault(Exception):
    pass

class Cpu:
    def __init__(self, elf):
        self.elf = elf
        self.r = [0] * 16
        self.n = self.z = self.c = self.v = 0
        self.mem = {}                 # byte-addressed data memory outside the image
        self.stack_ok = None          # (lo, hi): the only range sp-relative accesses may touch (None: any use of sp is a fault)
        self.swi_hook = None
        self.steps = 0
    # memory
    def rd32(self, a):
        if a & 3: raise Fault("unaligned read %08x" % a)
        w = self.elf.read32(a)
        if w is not None and a not in self.mem: return w
        return sum(self.mem.get(a + i, 0) << (8 * i) for i in range(4)) if any((a + i) in self.mem for i in range(4)) else self._unmapped(a)
    def _unmapped(self, a): raise Fault("read of unmapped memory %08x (pc=%08x)" % (a, self.r[15] - 8))
    def wr32(self, a, val):
        if a & 3: raise Fault("unaligned write %08x" % a)
        if self.elf.read32(a) is not None: raise Fault("write into the library image %08x" % a)
        for i in range(4): self.mem[a + i] = (val >> (8 * i)) & 0xFF
    def cond(self, cc):
        n, z, c, v = self.n, self.z, self.c, self.v
        return [z == 1, z == 0, c == 1, c == 0, n == 1, n == 0, v == 1, v == 0, c == 1 and z == 0, c == 0 or z == 1, n == v, n != v,
                z == 0 and n == v, z == 1 or n != v, True, False][cc]
    # operand 2
    def shift_imm(self, rm, typ, amt):
        val = self.reg(rm); carry = self.c
        if typ == 0:                                                  # LSL
            if amt: carry = (val >> (32 - amt)) & 1; val = (val << amt) & 0xFFFFFFFF
        elif typ == 1:                                                # LSR
            if amt == 0: carry = (val >> 31) & 1; val = 0
            else: carry = (val >> (amt - 1)) & 1; val >>= amt
        elif typ == 2:                                                # ASR
            if amt == 0: amt = 32
            s = val - (1 << 32) if val >> 31 else val
            carry = (s >> (amt - 1)) & 1; val = (s >> min(amt, 31)) & 0xFFFFFFFF
        else:
            raise Fault("ROR/RRX not modelled")
        return val, carry
    def reg(self, i):
        return (self.r[15] + 4) & 0xFFFFFFFF if i == 15 else self.r[i]      # r[15] holds the address of the instruction + 4 here: pc reads as +8
    def step(self):
        pc = self.r[15]
        w = self.elf.read32(pc)
        if w is None: raise Fault("fetch from %08x" % pc)
        self.r[15] = pc + 4                    # during execution r15 reads as pc+8: reg() adds 4 more
        self.steps += 1
        if self.steps > 200000: raise Fault("too many steps")
        cc = w >> 28
        if not self.cond(cc):
            return
        top = (w >> 25) & 7
        if top in (0, 1):                      # data processing
            if top == 0 and (w & 0x90) == 0x90: raise Fault("multiply/halfword transfer not modelled: %08x" % w)
            op = (w >> 21) & 15; s = (w >> 20) & 1; rn = (w >> 16) & 15; rd = (w >> 12) & 15
            if top == 1:
                imm = w & 0xFF; rot = ((w >> 8) & 15) * 2
                op2 = ((imm >> rot) | (imm << (32 - rot))) & 0xFFFFFFFF if rot else imm
                sc = (op2 >> 31) & 1 if rot else self.c
            else:
                if w & 0x10: raise Fault("register-specified shift not modelled: %08x" % w)
                op2, sc = self.shift_imm(w & 15, (w >> 5) & 3, (w >> 7) & 31)
            a = self.reg(rn); M = 0xFFFFFFFF
            def add(x, y, cin):
                t = x + y + cin; res = t & M
                cflag = 1 if t > M else 0
                vflag = 1 if (~(x ^ y) & (x ^ res)) >> 31 else 0
                return res, cflag, vflag
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
                self.n = res >> 31; self.z = 1 if res == 0 else 0; self.c = cflag; self.v = vflag if not logical else self.v
            if op not in (8, 9, 10, 11):
                if rd == 15: raise Fault("write to pc by data processing not modelled")
                self.set_reg(rd, res)
        elif top in (2, 3):                    # LDR / STR
            if top == 3 and (w & 0x10): raise Fault("media instruction")
            p = (w >> 24) & 1; u = (w >> 23) & 1; b = (w >> 22) & 1; wb = (w >> 21) & 1; ld = (w >> 20) & 1
            rn = (w >> 16) & 15; rd = (w >> 12) & 15
            if top == 2: off = w & 0xFFF
            else: off, _ = self.shift_imm(w & 15, (w >> 5) & 3, (w >> 7) & 31)
            if b: raise Fault("byte transfers not modelled")
            base = self.reg(rn)
            if rn == 15: base = (self.r[15] + 4) & 0xFFFFFFFF
            addr = (base + off if u else base - off) & 0xFFFFFFFF if p else base
            self.check_sp_access(rn, addr)
            if ld:
                val = self.rd32(addr)
                if rd == 15: raise Fault("load to pc not modelled")
                self.set_reg(rd, val)
            else:
                self.wr32(addr, self.reg(rd))
            if not p:
                self.set_reg(rn, (base + off if u else base - off) & 0xFFFFFFFF)
            elif wb:
                self.set_reg(rn, addr)
        elif top == 4:                         # LDM / STM
            p = (w >> 24) & 1; u = (w >> 23) & 1; sbit = (w >> 22) & 1; wb = (w >> 21) & 1; ld = (w >> 20) & 1
            rn = (w >> 16) & 15; rl = [i for i in range(16) if (w >> i) & 1]
            if sbit: raise Fault("LDM/STM with S bit")
            n = len(rl); base = self.reg(rn)
            if u: start = base + (4 if p else 0)
            else: start = base - 4 * n + (0 if p else 4)
            new = base + 4 * n if u else base - 4 * n
            for k, i in enumerate(rl):
                a = (start + 4 * k) & 0xFFFFFFFF
                self.check_sp_access(rn, a)
                if ld:
                    if i == 15: raise Fault("LDM to pc not modelled")
                    self.set_reg(i, self.rd32(a))
                else:
                    self.wr32(a, self.reg(i))
            if wb: self.set_reg(rn, new & 0xFFFFFFFF)
        elif top == 5:                         # B / BL
            off = w & 0xFFFFFF
            if off & 0x800000: off -= 1 << 24
            if (w >> 24) & 1: self.r[14] = self.r[15]
            self.r[15] = (pc + 8 + (off << 2)) & 0xFFFFFFFF
        elif top == 7 and (w >> 24) & 1:       # SWI
            self.swi_hook(self, w & 0xFFFFFF)
        else:
            raise Fault("instruction not modelled: %08x at %08x" % (w, pc))
    def set_reg(self, i, val): self.r[i] = val & 0xFFFFFFFF
    def check_sp_access(self, rn, addr):
        if rn == 13:
            if self.stack_ok is None: raise Fault("the stack pointer is used where it is not a usable stack (pc=%08x)" % (self.r[15] - 4))
            lo, hi = self.stack_ok
            if not (lo <= addr < hi): raise Fault("stack access outside the stack: %08x" % addr)
    def run(self, start, stops):
        self.r[15] = start
        while self.r[15] not in stops:
            self.step()
        return self.r[15]

# ---------------------------------------------------------------- scenarios
def find(elf, name):
    if name not in elf.syms: sys.exit("symbol %s not found in the library (it needs its local symbols: do not strip it)" % name)
    return elf.syms[name]

GOT = 0x40000000            # the fake GOT base (r7)
HANDLER = 0xFFFF0000        # stands for "the parent's error handler runs"
OK = True
def check(cond, msg):
    global OK
    print(("  ok   " if cond else "  FAIL ") + msg)
    if not cond: OK = False

def model_stack(size_bytes, free_pages):
    """What the loop is meant to do: pages = max(256, size/4096); try pages (+1 guard page); no room -> halve (not below 256); not even 256 -> error."""
    pages = max(256, (size_bytes & 0xFFFFFFFF) >> 12); tries = []
    while True:
        tries.append(pages)
        if pages + 1 <= free_pages: return pages, tries
        if pages <= 256: return None, tries
        pages = max(256, pages >> 1)

def run_stack(elf, size_bytes, free_pages, scramble=False, noname=False):
    """size_bytes None = __stack_size not defined (the GOT entry is 0)."""
    start = elf.syms["stack_try"]
    # the first instruction of the sequence is 4 instructions before the lsr #12; find it from the symbol table by walking back from stack_try
    a = start
    while elf.read32(a) != 0xE1A05625: a -= 4          # mov r5, r5, lsr #12
        # (the instructions before it: ldr r5,[pc,#x]; ldr r5,[r7,r5]; teq r5,#0; ldrne r5,[r5])
    first = a - 16
    cpu = Cpu(elf)
    cpu.r[7] = GOT; cpu.r[12] = 0x11110000; cpu.r[11] = 0x22220000; cpu.r[13] = 0xDEAD0000   # sp deliberately not a usable stack
    cpu.stack_ok = None
    # literal pool words -> GOT offsets; the GOT entries are what the dynamic linker would have put there
    ldr_ss = elf.read32(first)                           # ldr r5, [pc, #imm]
    imm = ldr_ss & 0xFFF
    assert ldr_ss & 0xFFFFF000 == 0xE59F5000, "unexpected first instruction %08x" % ldr_ss
    lit_ss = first + 8 + imm
    got_off_ss = elf.read32(lit_ss)
    sizevar = 0x50000000
    cpu.wr32(GOT + got_off_ss, 0 if size_bytes is None else sizevar)
    if size_bytes is not None: cpu.wr32(sizevar, size_bytes & 0xFFFFFFFF)
    # ___program_name: the loop reads it too (the name of the stack): give it a variable that holds a string pointer
    # find its literal: the ldr r3,[pc,#x] right after stack_try's three movs
    ldr_pn = elf.read32(start + 12)
    assert ldr_pn & 0xFFFFF000 == 0xE59F3000, "unexpected instruction %08x" % ldr_pn
    # the GOT slot of ___program_name is read from the literal's own symbol, NOT through the instruction: a loop whose ldr points at another literal then finds an empty slot
    got_off_pn = elf.read32(elf.syms["___program_name"]) if "___program_name" in elf.syms else elf.read32(start + 12 + 8 + (ldr_pn & 0xFFF))
    namevar, namestr = 0x50001000, 0x50002000
    cpu.wr32(GOT + got_off_pn, 0 if noname else namevar)           # noname: the weak symbol is not defined, the GOT entry is 0
    if not noname: cpu.wr32(namevar, namestr)
    calls = []; state = {"free": free_pages, "next_base": 0x60000000}
    def swi(c, n):
        c.r[15] -= 0                                      # r15 already points after the SWI
        if n != 0x79D02: raise Fault("unexpected SWI %x" % n)
        reason, pages, guard, name = c.r[0], c.r[1], c.r[2], c.r[3]
        if reason != 0: raise Fault("StackOp reason %d (expected 0, ALLOC)" % reason)
        if guard != 1: raise Fault("guard pages %d (expected 1)" % guard)
        if name != (0 if noname else namestr): raise Fault("name pointer %08x (expected the program name)" % name)
        calls.append(pages)
        need = pages + guard
        if need <= state["free"]:
            state["free"] -= need
            base = state["next_base"]; state["next_base"] += need << 12
            c.r[0] = 0x70000000; c.r[1] = base + (guard << 12); c.r[2] = base + (need << 12); c.v = 0
            state["top"] = c.r[2]
        else:                                             # as ARMEABISupport does: r0 = error pointer, r1 = r2 = 0, V set
            c.r[0] = 0x71000000; c.r[1] = 0; c.r[2] = 0; c.v = 1
            for i in (3, 4, 6, 8, 9, 10): c.r[i] = 0xBAD00000 + i      # registers the loop does not use: anything may be left in them
    cpu.swi_hook = swi
    stops = {find(elf, "stack_ok"), find(elf, "__exit_with_error_num")}
    end = cpu.run(first, stops)
    exp_pages, exp_tries = model_stack(0 if size_bytes is None else size_bytes, free_pages)
    what = "__stack_size %s, room for %d pages%s" % ("undefined" if size_bytes is None else "%d" % size_bytes, free_pages, ", __program_name not defined" if noname else "")
    if end == find(elf, "stack_ok"):
        ok = exp_pages is not None and calls == exp_tries and cpu.r[2] == state["top"] and calls[-1] == exp_pages
        check(ok, "%s: stack of %d pages after %d tr%s (%s), sp would be %08x%s" % (what, calls[-1], len(calls), "y" if len(calls) == 1 else "ies",
              " -> ".join(str(x) for x in calls), cpu.r[2], "" if ok else "   EXPECTED %s" % (exp_tries,)))
        check(cpu.r[7] == GOT and cpu.r[12] == 0x11110000 and cpu.r[11] == 0x22220000, "%s: PIC register, ip and fp intact" % what)
    else:
        ok = exp_pages is None and calls == exp_tries and cpu.r[0] == 8
        check(ok, "%s: no stack; exit with error %d after %s%s" % (what, cpu.r[0], " -> ".join(str(x) for x in calls), "" if ok else "   EXPECTED %s" % (exp_tries,)))

def model_da(max_bytes, capacity):
    """halve while > 2 MB and it does not fit; at <= 2 MB the plain SWI (an error there ends the program)."""
    s = max_bytes; tries = []
    while True:
        tries.append(s)
        if s <= capacity: return s, tries
        if s <= 2 * 1024 * 1024: return None, tries
        s >>= 1

def run_da(elf, max_bytes, capacity, paranoid=False):
    da_try, da_last, da_done = find(elf, "da_try"), find(elf, "da_last"), find(elf, "da_done")
    # the frame push is the instruction before da_try
    first = da_try - 4
    assert elf.read32(first) == 0xE92D1B20, "instruction before da_try is %08x (expected stmdb sp!,{r5,r8,r9,fp,ip})" % elf.read32(first)
    cpu = Cpu(elf)
    sp0 = 0x7F000000
    cpu.r[13] = sp0; cpu.stack_ok = (sp0 - 4096, sp0)
    cpu.r[5] = max_bytes; cpu.r[8] = 0x50003000; cpu.r[9] = 0xA9A9A9A9; cpu.r[11] = 0x22220000; cpu.r[12] = 0x11110000
    cpu.r[4] = 0x44444444; cpu.r[6] = 0x66666666; cpu.r[7] = 0x77777777          # whatever is there: the loop sets what OS_DynamicArea needs
    calls = []; handler = {"called_plain": False}
    def swi(c, n):
        if n not in (0x20066, 0x66): raise Fault("unexpected SWI %x" % n)
        r0, r1, r2, r3, r4, r5, r6, r7, r8 = c.r[0:9]
        if (r0, r1, r2, r3, r4, r6, r7) != (0, 0xFFFFFFFF, 0, 0xFFFFFFFF, 0x80, 0, 0): raise Fault("bad registers for OS_DynamicArea 0: %s" % [hex(x) for x in c.r[0:9]])
        if r8 != 0x50003000: raise Fault("bad name pointer %08x" % r8)
        calls.append((n, r5))
        if r5 <= capacity:
            c.r[1] = 7; c.r[3] = 0x30000000; c.v = 0                                   # area number, base address; the rest as it was
            if paranoid:                                                                # (paranoid model: a kernel that does not even keep r4-r12 after SUCCESS)
                for i in (4, 5, 6, 7, 8, 9, 10, 11, 12): c.r[i] = 0xBAD00000 + i
        else:
            if n == 0x66: handler["called_plain"] = True; c.v = 1; c.r[0] = 0x71000000; c.r[15] = HANDLER; return   # the parent's error handler would run: stop the simulation
            c.v = 1; c.r[0] = 0x71000001
            for i in (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12): c.r[i] = 0xBAD00000 + i  # a failing SWI may leave ANYTHING in the other registers
    cpu.swi_hook = swi
    end = cpu.run(first, {da_done + 16, HANDLER})                                       # da_done + 16 = the instruction after "add sp, sp, #20"
    what = "max size %d MB, room for %d MB%s" % (max_bytes >> 20, capacity >> 20, ", registers scrambled even after success" if paranoid else "")
    exp, exp_tries = model_da(max_bytes, capacity)
    got_tries = [s for (_, s) in calls]
    if handler["called_plain"]:
        check(exp is None and got_tries == exp_tries, "%s: even 2 MB did not fit: the plain SWI reports the error (tries %s)%s" % (what, " -> ".join("%d MB" % (s >> 20) if s >= 1 << 20 else "%d KB" % (s >> 10) for s in got_tries), "" if exp is None else "   EXPECTED a result"))
        return
    ok = exp is not None and got_tries == exp_tries and calls[-1][1] == exp
    kinds = "".join("X" if n == 0x20066 else "P" for (n, _) in calls)
    check(ok, "%s: area of %d MB after %d tr%s (%s; SWIs %s)%s" % (what, calls[-1][1] >> 20, len(calls), "y" if len(calls) == 1 else "ies",
          " -> ".join("%d" % (s >> 20) if s >= 1 << 20 else "%d KB" % (s >> 10) for s in got_tries), kinds, "" if ok else "   EXPECTED %s" % exp_tries))
    check(cpu.r[13] == sp0, "%s: stack pointer balanced" % what)
    check(cpu.r[9] == 0xA9A9A9A9 and cpu.r[11] == 0x22220000 and cpu.r[12] == 0x11110000, "%s: saved PIC register, fp and ip restored after the failed tries" % what)
    check(cpu.r[1] == 7 and cpu.r[3] == 0x30000000 and cpu.r[2] == 0, "%s: the SWI's results (area number, base) arrive in r1, r3" % what)

def run_himem_max(elf, himem, limit):
    """sys/_syslib.s, himem_max_start .. himem_max_end: __ul_memory.appspace_himem_max = appspace_himem when that is below the application space limit (the permitted RAM limit
    of a program that was started by vfork + exec), -1 otherwise (unsigned comparison).  Entry: fp = __ul_memory, r1 (a2) = the application space limit (OS_ChangeEnvironment 14),
    [fp, #4] = appspace_himem.  Nothing but r2 (a3) and the word at [fp, #52] may change."""
    first, last = find(elf, "himem_max_start"), find(elf, "himem_max_end")
    cpu = Cpu(elf)
    FP = 0x50010000
    cpu.r[11] = FP; cpu.r[1] = limit; cpu.r[0] = 0xA0A0A0A0; cpu.r[2] = 0x12345678; cpu.r[3] = 0xA3A3A3A3; cpu.r[13] = 0xDEAD0000; cpu.stack_ok = None
    for off in range(0, 64, 4): cpu.wr32(FP + off, 0x77000000 + off)
    cpu.wr32(FP + 4, himem)
    before = dict(cpu.mem)
    cpu.swi_hook = lambda c, n: (_ for _ in ()).throw(Fault("unexpected SWI %x" % n))
    cpu.run(first, {last})
    want = himem if himem < limit else 0xFFFFFFFF
    got = cpu.rd32(FP + 52)
    others = [a for a in range(FP, FP + 64) if cpu.mem.get(a) != before.get(a) and not (FP + 52 <= a < FP + 56)]
    what = "appspace_himem %08x, application space limit %08x" % (himem, limit)
    check(got == want and not others, "himem_max: %s -> appspace_himem_max %08x (expected %08x), no other word of __ul_memory touched" % (what, got, want))
    check(cpu.r[0] == 0xA0A0A0A0 and cpu.r[1] == limit and cpu.r[3] == 0xA3A3A3A3 and cpu.r[11] == FP and cpu.r[13] == 0xDEAD0000, "himem_max: %s: only r2 changed (r0, r1, r3, fp, sp intact)" % what)

def main():
    if len(sys.argv) != 2: sys.exit(__doc__)
    elf = Elf(sys.argv[1])
    MB = 1 << 20
    print("-- EABI main stack (stack_try)")
    for size, room in [(None, 100000), (None, 257), (None, 256), (0, 100000), (100, 100000), (1 << 20, 100000), (5 * MB + 100, 100000), (16 * MB, 100000), (64 * MB, 100000),
                       (64 * MB, 5000), (64 * MB, 300), (1228800, 200), (1536000, 300), (1536000, 1000), (1 << 30, 65536), (1 << 30, 65535), (1 << 30, 40000), (0x80000000, 65536), (0xF0000000, 1000), (64 * MB, 200)]:
        run_stack(elf, size, room)
    for size, room in [(3 * MB, 300), (3 * MB, 400), (2 * MB, 300), (4 * MB - 4096, 600)]:       # the halving lands between 256 and 511 pages
        run_stack(elf, size, room)
    run_stack(elf, 64 * MB, 100000, noname=True)
    print("-- heap dynamic area (da_try)")
    for mx, cap in [(32 * MB, 4096 * MB), (512 * MB, 4096 * MB), (512 * MB, 300 * MB), (512 * MB, 3 * MB), (512 * MB, 2 * MB), (512 * MB, 1 * MB), (32 * MB, 20 * MB),
                    (2 * MB, 2 * MB), (2 * MB, 1 * MB), (1 * MB, 4096 * MB), (3 * MB, 1 * MB), (1 << 30, 100 * MB), (100 * MB + 12345, 40 * MB)]:
        run_da(elf, mx, cap)
    for mx, cap in [(512 * MB, 4096 * MB), (512 * MB, 300 * MB), (2 * MB, 2 * MB)]:
        run_da(elf, mx, cap, paranoid=True)
    if "himem_max_start" in elf.syms:
        print("-- appspace_himem_max (himem_max_start .. himem_max_end)")
        for himem, limit in [(0x4008000, 0x4008000), (0x3d8aff8, 0x4008000), (0x3d8aff8, 0x3d8affc), (0x3d8affc, 0x3d8aff8), (0x4008000, 0x3d8aff8), (0, 0x4008000), (0x1000, 0xFFFFFFFF),
                             (0xFFFFFFFE, 0xFFFFFFFF), (0xFFFFFFFF, 0xFFFFFFFF), (0x80000000, 0x7FFFFFFF), (0x7FFFFFFF, 0x80000000), (0x6ee6b418, 0x4008000)]:
            run_himem_max(elf, himem, limit)
    else:
        print("-- (this library has no himem_max_start: the heap-limit patch is not in it)")
    print("start-up loop simulation: " + ("OK" if OK else "FAILED"))
    return 0 if OK else 1

if __name__ == "__main__":
    try:
        sys.exit(main())
    except Fault as e:
        print("  FAIL simulation fault: %s" % e); sys.exit(1)
