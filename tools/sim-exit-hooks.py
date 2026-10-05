#!/usr/bin/env python3
"""Run the machine code of the exit-time additions of a BUILT libunixlib.so (16.2.0-7 and later) on the small ARM (A32) interpreter of sim-startup-loops.py and check what it does:

  __signalhandler_stack_free   (called by _exit and execve after the environment handlers are off) gives the one page signal stack back to ARMEABISupport - only when this process is
                               the last user of the program image (__dynamic_area_refcount is 0), a signal handler is not running, and sp is not on the signal stack itself.
                               StackOp GET_STACK (top - 4) -> handle, StackOp FREE (handle); __ul_global.signalhandler_sp is cleared only when FREE worked.  Never a fault, never a changed register.
  mmap_too_big                 (called by mmap () and by mremap () when the mapping grows) refuses a request of 2 GB or more, or more than the OS clamp on the size of one dynamic area
                               (OS_DynamicArea 8, read only, only asked for requests of 16 MB or more) before ARMEABISupport makes an "mmap#N" area for it that it would never give back.
  mmap, mremap                 refuse such a request with ENOMEM without calling ARMEABISupport_MMapOp; everything else reaches MMapOp with the arguments unchanged.

  __pthread_prog_fini          (called by _exit, 16.2.0-8 and later) frees the RMA block of the program image (__ul_global.pthread_callevery_rma, OS_Module 7) and stops the pthread ticker when
                               the pthread system runs - but ONLY for the last user of the image: a vfork child that ends without exec shares the image with its parent (__dynamic_area_refcount > 1,
                               the child has not yet taken itself off: __dynamic_area_exit comes after this function in _exit) and must leave the block and the ticker alone (its _exit used to free the
                               parent's block: a double free that corrupted the RMA heap in a loop of such children and froze the machine).

The machine code is read from the library itself; the SWIs, the shared-library GOT/PLT and the calls into other functions are modelled here.  Every conditional instruction of the new code is
then flipped once (a "mutant", EQ <-> NE and so on) and the scenarios must notice: a mutant that survives is a condition no scenario depends on.
usage: sim-exit-hooks.py LIBUNIXLIB.so      (exit status 0 = every scenario right and every mutant caught)"""
import importlib.util, os, struct, sys

here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("simstart", os.path.join(here, "sim-startup-loops.py"))
simstart = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(simstart)
Elf, Cpu, Fault = simstart.Elf, simstart.Cpu, simstart.Fault
M = 0xFFFFFFFF

# ---------------------------------------------------------------- the library: sections, PLT, GOT
class Lib(Elf):
    def __init__(self, path):
        super().__init__(path)
        d = self.d
        self.patches = {}                                           # va -> word: the mutants
        sh = [struct.unpack_from("<IIIIIIIIII", d, self.e_shoff + i * self.e_shentsize) for i in range(self.e_shnum)]
        so = sh[self.e_shstrndx][4]
        self.sec = {}
        for (name, typ, fl, addr, off, size, link, info, al, esz) in sh:
            self.sec[d[so + name:d.index(b"\0", so + name)].decode()] = dict(addr=addr, off=off, size=size, link=link)
        ds, dstr = self.sec[".dynsym"], self.sec[".dynstr"]
        self.dynsym = []
        for k in range(ds["size"] // 16):
            n, v, sz, info, oth, shn = struct.unpack_from("<IIIBBH", d, ds["off"] + k * 16)
            self.dynsym.append((d[dstr["off"] + n:d.index(b"\0", dstr["off"] + n)].decode(), v, shn))
        # function sizes from the full symbol table
        self.size = {}
        for (n, typ, fl, addr, off, size, link, info, al, esz) in sh:
            if typ == 2:
                so2 = sh[link][4]
                for k in range(size // 16):
                    sn, v, sz, inf, oth, shn = struct.unpack_from("<IIIBBH", d, off + k * 16)
                    nm = d[so2 + sn:d.index(b"\0", so2 + sn)].decode()
                    if nm and sz and nm not in self.size: self.size[nm] = sz
        # PLT: 5 words for PLT0, then 5 words per entry, in the order of .rel.plt
        plt0 = self.sec[".plt"]["addr"]; rp = self.sec[".rel.plt"]
        self.plt = {}
        for n in range(rp["size"] // 8):
            r_off, r_info = struct.unpack_from("<II", d, rp["off"] + n * 8)
            self.plt[plt0 + 20 + 20 * n] = (self.dynsym[r_info >> 8][0], r_off)
        # GLOB_DAT slots of the GOT
        rd = self.sec[".rel.dyn"]
        self.glob_dat = {}
        for n in range(rd["size"] // 8):
            r_off, r_info = struct.unpack_from("<II", d, rd["off"] + n * 8)
            if (r_info & 0xFF) == 21: self.glob_dat[r_off] = self.dynsym[r_info >> 8][0]
        self.got_base = self.syms["_GLOBAL_OFFSET_TABLE_"]
    def read32(self, va):
        if va in self.patches: return self.patches[va]
        return super().read32(va)
    def slot_of(self, sym):
        """the GOT slot that holds the address of SYM: a GLOB_DAT relocation names it; for a symbol of this library itself the slot holds its link-time address"""
        for a, n in self.glob_dat.items():
            if n == sym: return a
        got = self.sec[".got"]; hits = []
        for a in range(got["addr"], got["addr"] + got["size"], 4):
            if super().read32(a) == self.syms[sym]: hits.append(a)
        if len(hits) != 1: raise KeyError("%s: %d GOT slots hold its address" % (sym, len(hits)))
        return hits[0]
    def check_plt(self):
        """decode the three instructions that end every PLT entry and compare the GOT slot they use with the relocation: the order of .rel.plt is the order of the entries"""
        bad = 0
        for a, (nm, r_off) in list(self.plt.items())[:40] + list(self.plt.items())[-40:]:
            w3, w4 = self.read32(a + 12), self.read32(a + 16)
            def rot(w): imm = w & 0xFF; r = ((w >> 8) & 15) * 2; return ((imm >> r) | (imm << (32 - r))) & M if r else imm
            assert w3 & 0xFFFFF000 == 0xE28CC000 and w4 & 0xFFFFF000 == 0xE5BCF000, "unexpected PLT entry %08x %08x at %08x" % (w3, w4, a)
            slot = self.got_base + rot(w3) + (w4 & 0xFFF)
            if slot != r_off: bad += 1
        return bad

# ---------------------------------------------------------------- the CPU: the instruction forms the new code uses and the calls into other functions
class Sim(Cpu):
    def __init__(self, lib):
        super().__init__(lib)
        self.lib = lib
        self.externals = {}
        self.calls = []
    # byte memory
    def rd8(self, a):
        if a in self.mem: return self.mem[a]
        w = self.lib.read32(a & ~3)
        if w is None: raise Fault("read of unmapped byte %08x" % a)
        return (w >> (8 * (a & 3))) & 0xFF
    def wr8(self, a, v):
        if self.lib.read32(a & ~3) is not None: raise Fault("write into the library image %08x" % a)
        self.mem[a] = v & 0xFF
    def put32(self, a, v):                                          # set memory, also inside the image range (overrides what the file has: GOT slots, 0x8038)
        for i in range(4): self.mem[a + i] = (v >> (8 * i)) & 0xFF
    def step(self):
        pc = self.r[15]
        if pc in self.lib.plt:
            name, _ = self.lib.plt[pc]
            if name not in self.externals: raise Fault("call of %s, which the simulation does not model (pc=%08x)" % (name, pc))
            self.externals[name](self)
            self.r[15] = self.r[14]; self.steps += 1
            return
        w = self.lib.read32(pc)
        if w is None: raise Fault("fetch from %08x" % pc)
        cc = w >> 28
        top = (w >> 25) & 7
        if (w & 0x0FFFFFF0) == 0x012FFF10:                                          # BX Rm
            self.steps += 1
            self.r[15] = (self.r[w & 15] & ~1) & M if self.cond(cc) else pc + 4
            return
        if (w & 0x0FF00000) in (0x03000000, 0x03400000):                            # MOVW / MOVT
            self.steps += 1; self.r[15] = pc + 4
            if self.cond(cc):
                rd = (w >> 12) & 15; imm = ((w >> 4) & 0xF000) | (w & 0xFFF)
                self.set_reg(rd, imm if (w & 0x0FF00000) == 0x03000000 else (self.r[rd] & 0xFFFF) | (imm << 16))
            return
        if top == 4 and (w & (1 << 20)) and (w & (1 << 15)):                        # LDM with pc in the list (pop {..., pc})
            self.steps += 1; self.r[15] = pc + 4
            if not self.cond(cc): return
            p, u, wb, rn = (w >> 24) & 1, (w >> 23) & 1, (w >> 21) & 1, (w >> 16) & 15
            rl = [i for i in range(16) if (w >> i) & 1]; n = len(rl); base = self.reg(rn)
            start = (base + (4 if p else 0)) if u else (base - 4 * n + (0 if p else 4))
            target = None
            for k, i in enumerate(rl):
                a = (start + 4 * k) & M; self.check_sp_access(rn, a); val = self.rd32(a)
                if i == 15: target = val & ~3
                else: self.set_reg(i, val)
            if wb: self.set_reg(rn, (base + 4 * n) if u else (base - 4 * n))
            self.r[15] = target
            return
        if top in (2, 3) and (w & (1 << 22)):                                       # LDRB / STRB
            self.steps += 1; self.r[15] = pc + 4
            if not self.cond(cc): return
            p, u, wb, ld = (w >> 24) & 1, (w >> 23) & 1, (w >> 21) & 1, (w >> 20) & 1
            rn, rd = (w >> 16) & 15, (w >> 12) & 15
            if top == 2: off = w & 0xFFF
            else: off, _ = self.shift_imm(w & 15, (w >> 5) & 3, (w >> 7) & 31)
            base = self.reg(rn)
            addr = ((base + off if u else base - off) & M) if p else base
            self.check_sp_access(rn, addr)
            if ld: self.set_reg(rd, self.rd8(addr))
            else: self.wr8(addr, self.reg(rd))
            if not p: self.set_reg(rn, (base + off if u else base - off) & M)
            elif wb: self.set_reg(rn, addr)
            return
        super().step()
    # a function call from the point of view of a scenario: arguments in r0-r3 (+ stack), returns when pc reaches the sentinel
    SENT = 0xFFFF0000
    def call(self, addr, args, sp=0x7F000000, stack_args=()):
        for i, a in enumerate(args[:4]): self.r[i] = a & M
        sp -= 4 * len(stack_args) + 64
        for i, a in enumerate(stack_args): self.put32(sp + 4 * i, a)
        self.r[13] = sp; self.r[14] = self.SENT
        self.stack_ok = (0x7E000000, 0x7F000100)
        return self.run(addr, {self.SENT})

# ---------------------------------------------------------------- the model of _swix and of the little data the code reads
ERR = 0x71000000                                                    # "an error block": any non-zero pointer
GBL, REFC, ERRNO, THREAD, THREADPTR, PIC1, PIC2 = 0x50000000, 0x50001000, 0x50002000, 0x50003000, 0x50004000, 0x50005000, 0x50006000

def swix(cpu):
    """_swix (swi, flags, ...): _IN (n) = bit n, _OUT (n) = bit 31 - n; the varargs are the values of the input registers (from r2) and then the pointers for the outputs"""
    swi, flags = cpu.r[0], cpu.r[1]
    sp = cpu.r[13]; got = [0]
    def arg(i): return cpu.r[2 + i] if i < 2 else cpu.rd32(sp + 4 * (i - 2))
    ins = [n for n in range(10) if flags >> n & 1]
    outs = [n for n in range(10) if flags >> (31 - n) & 1]
    assert flags & ~(0x3FF | (0x3FF << 22)) == 0, "flags %08x not modelled" % flags
    regs = {n: arg(k) for k, n in enumerate(ins)}
    err, vals = cpu.swi_model(swi, regs)
    cpu.calls.append((swi, regs))
    if err: cpu.r[0] = ERR; return
    for k, n in enumerate(outs):
        ptr = arg(len(ins) + k)
        if ptr: cpu.put32(ptr, vals.get(n, 0))
    cpu.r[0] = 0

def setup(lib, refcount=1, top=0, handler=0, pagesize=4096, thread_system=0):
    cpu = Sim(lib)
    cpu.externals["_swix"] = swix
    for i in range(4, 12): cpu.r[i] = 0xC0DE0000 + i                 # callee-saved registers: must come back unchanged
    cpu.r[12] = 0x12121212
    # the shared-library GOT as the code reaches it: 0x8038 -> PIC1; *PIC1 = the GOT base; slots hold the addresses of the variables
    cpu.put32(0x8038, PIC1); cpu.put32(PIC1, lib.got_base)
    for sym, addr in (("__ul_global", GBL), ("__dynamic_area_refcount", REFC)): cpu.put32(lib.slot_of(sym), addr)
    cpu.put32(GBL + 92, top); cpu.put32(GBL + 84, handler); cpu.put32(GBL + 72, thread_system); cpu.put32(GBL + 48, pagesize)
    cpu.put32(REFC, refcount)
    cpu.errno_sym = None
    return cpu

HANDLE = 0x20376FD4                                                 # what StackOp GET_STACK answers
MB = 1 << 20

class World:
    """what the SWIs answer in one scenario; every call is logged"""
    def __init__(self, clamp="none", get="ok", free="ok", mmap="err", top=0):
        self.clamp, self.get, self.free, self.mmap, self.top = clamp, get, free, mmap, top
    def model(self, swi, regs):
        if swi == 0x66:                                             # OS_DynamicArea: only reason 8 with R1 = R2 = 0 (read the clamps) may be asked
            if (regs.get(0), regs.get(1), regs.get(2)) != (8, 0, 0): raise Fault("OS_DynamicArea with registers %s" % regs)
            if self.clamp == "swierr": return True, {}
            c = {"none": 0xFFFFFFFF, "zero": 0}.get(self.clamp, self.clamp)
            return False, {1: 0xFFFFFFFF, 2: c}
        if swi == 0x59D02:                                          # ARMEABISupport_StackOp
            r = regs.get(0)
            if r == 2:
                if self.get == "err": return True, {}
                if self.get == "null": return False, {1: 0}
                if regs.get(1) != self.top - 4: raise Fault("GET_STACK of %08x, expected top - 4 = %08x" % (regs.get(1), self.top - 4))
                return False, {1: HANDLE}
            if r == 1:
                if regs.get(1) != HANDLE: raise Fault("FREE of handle %08x, expected %08x" % (regs.get(1), HANDLE))
                return self.free == "err", {}
            raise Fault("StackOp reason %s" % r)
        if swi == 0x59D04:                                          # ARMEABISupport_MMapOp
            return (self.mmap == "err"), ({} if self.mmap == "err" else {0: 0x6EE67000})
        raise Fault("unexpected SWI %x" % swi)

def regs_ok(cpu, sp_entry):
    return all(cpu.r[i] == 0xC0DE0000 + i for i in range(4, 12)) and cpu.r[13] == sp_entry

SP_ENTRY = 0x7F000000 - 64                                         # Sim.call () without stack arguments

def run_free(lib, refcount=0, top_rel=-0x7000000, handler=0, top_null=False, get="ok", free="ok"):
    """__signalhandler_stack_free (): TOP_REL = where the signal stack's top is, relative to the sp the function is entered with (default: far from it)"""
    top = 0 if top_null else (SP_ENTRY + top_rel) & M
    cpu = setup(lib, refcount=refcount, top=top, handler=handler)
    w = World(get=get, free=free, top=top); cpu.swi_model = w.model
    try:
        cpu.call(lib.syms["__signalhandler_stack_free"], [0x11111111, 0x22222222, 0x33333333, 0x44444444])
    except Fault as e:
        return False, "fault: %s" % e
    calls = [(s, r.get(0)) for (s, r) in cpu.calls]
    sp_after = cpu.rd32(GBL + 92)
    return True, dict(calls=calls, sp_after=sp_after, top=top, regs=regs_ok(cpu, SP_ENTRY), r0=cpu.r[0])

def free_scenarios(lib):
    """(description, kwargs, expected calls [(swi, reason)], expect that signalhandler_sp is cleared)"""
    G, F = (0x59D02, 2), (0x59D02, 1)
    far, inside_top, inside_base, below_base, above_top = -0x7000000, 200, 4096 - 200, 4096 + 200, -200
    return [
        ("last user of the image, sp elsewhere: GET_STACK (top - 4), FREE, pointer cleared", dict(), [G, F], True),
        ("a vfork parent still shares the image (refcount 1): nothing is done",              dict(refcount=1), [], False),
        ("refcount 2: nothing is done",                                                      dict(refcount=2), [], False),
        ("refcount 0xFFFFFFFF (wrapped): nothing is done",                                   dict(refcount=0xFFFFFFFF), [], False),
        ("no signal stack (top = NULL): nothing is done",                                    dict(top_null=True), [], False),
        ("a signal handler is running: nothing is done",                                     dict(handler=1), [], False),
        ("nested signal handlers (3): nothing is done",                                      dict(handler=3), [], False),
        ("sp near the top of the signal stack (the exit handler runs there): left alone",    dict(top_rel=inside_top), [], False),
        ("sp near the base of the signal stack: left alone",                                 dict(top_rel=inside_base), [], False),
        ("sp just below the base of the signal stack: freed",                                dict(top_rel=below_base), [G, F], True),
        ("sp just above the top of the signal stack: freed",                                 dict(top_rel=above_top), [G, F], True),
        ("GET_STACK fails: no FREE, pointer kept",                                           dict(get="err"), [G], False),
        ("GET_STACK answers a NULL handle: no FREE, pointer kept",                           dict(get="null"), [G], False),
        ("FREE fails: pointer kept",                                                         dict(free="err"), [G, F], False),
    ]

RMA_BLOCK = 0x20F18420                                               # what __ul_global.pthread_callevery_rma holds in these scenarios

def run_pfini(lib, refcount, running):
    """__pthread_prog_fini (): returns (ok, dict(calls=[...], running_after, regs, sp))"""
    cpu = setup(lib, refcount=refcount, thread_system=running)
    cpu.put32(GBL + 112, RMA_BLOCK)
    log = []
    def os_swi(c):
        regs = c.r[1]; log.append(("OS_Module" if c.r[0] == 0x1E else "swi %x" % c.r[0], c.rd32(regs), c.rd32(regs + 8))); c.r[0] = 0
    def stop_ticker(c): log.append(("stop_ticker",))
    cpu.externals["__os_swi"] = os_swi; cpu.externals["__pthread_stop_ticker"] = stop_ticker
    try:
        cpu.call(lib.syms["__pthread_prog_fini"], [0x11111111, 0x22222222, 0x33333333, 0x44444444])
    except Fault as e:
        return False, "fault: %s" % e
    return True, dict(calls=log, running_after=cpu.rd32(GBL + 72), regs=regs_ok(cpu, SP_ENTRY))

def pfini_scenarios():
    """(description, refcount, pthread system running, expected calls, expected __ul_global.pthread_system_running afterwards)"""
    free = ("OS_Module", 7, RMA_BLOCK)
    stop = ("stop_ticker",)
    return [
        ("the last user of the image (refcount 1), pthread system not running: the RMA block is freed (OS_Module 7)", 1, 0, [free], 0),
        ("the last user, pthread system running: the ticker is stopped, the flag cleared, the block freed",          1, 1, [stop, free], 0),
        ("refcount 0 (a process whose count was already taken off): freed as before",                                  0, 0, [free], 0),
        ("a vfork child that shares the image (refcount 2), pthread system running: nothing is touched",             2, 1, [], 1),
        ("a vfork child (refcount 2), pthread system not running: nothing is touched",                               2, 0, [], 0),
        ("a vfork child of a vfork child (refcount 3): nothing is touched",                                          3, 1, [], 1),
        ("refcount 0xFFFFFFFF (wrapped): nothing is touched",                                                        0xFFFFFFFF, 1, [], 1),
    ]

def run_too_big(lib, length, clamp):
    cpu = setup(lib)
    w = World(clamp=clamp); cpu.swi_model = w.model
    try:
        cpu.call(lib.syms["mmap_too_big"], [length])
    except Fault as e:
        return False, "fault: %s" % e
    return True, dict(r0=cpu.r[0], swis=[s for (s, r) in cpu.calls], regs=regs_ok(cpu, SP_ENTRY))

def want_too_big(length, clamp):
    """what mmap_too_big is meant to do -> (answer, number of OS_DynamicArea calls)"""
    if length >= 1 << 31: return 1, 0
    if length < 16 * MB: return 0, 0
    c = {"swierr": None, "none": 0xFFFFFFFF, "zero": 0}.get(clamp, clamp)
    if c is None or c in (0, 0xFFFFFFFF): return 0, 1
    return int(length > c), 1

def run_mmap(lib, which, args, stack_args, clamp, mmap_result):
    cpu = setup(lib)
    cpu.put32(lib.slot_of("errno"), ERRNO); cpu.put32(lib.slot_of("__pthread_running_thread"), THREADPTR); cpu.put32(THREADPTR, THREAD)
    w = World(clamp=clamp, mmap=mmap_result); cpu.swi_model = w.model
    try:
        cpu.call(lib.syms[which], args, stack_args=stack_args)
    except Fault as e:
        return False, "fault: %s" % e
    return True, dict(r0=cpu.r[0], errno=cpu.rd32(ERRNO) if cpu.mem.get(ERRNO) is not None else 0, calls=cpu.calls, regs=regs_ok(cpu, 0x7F000000 - 4 * len(stack_args) - 64))

LENS = [0, 1, 4095, 4096, MB, 15 * MB, 16 * MB - 1, 16 * MB, 20 * MB, 100 * MB, 128 * MB - 1, 128 * MB, 128 * MB + 1, 129 * MB, 150 * MB, 200 * MB, 1 << 30, (1 << 31) - 4097, (1 << 31) - 1,
        1 << 31, (1 << 31) + 4096, 3 << 30, 0xFFFFFFFF - 4095, 0xFFFFFFFF]
CLAMPS = ["none", "zero", 128 * MB, 16 * MB, 0x7FFFFFFF, 0xFFFFFFFE, 1, "swierr"]

def evaluate(lib, verbose=False):
    """run every scenario; returns the list of (ok, description)"""
    res = []
    def rec(ok, msg):
        res.append((ok, msg))
        if verbose: print(("  ok   " if ok else "  FAIL ") + msg)
    # ---- __signalhandler_stack_free
    for desc, kw, exp_calls, exp_cleared in free_scenarios(lib):
        ok, r = run_free(lib, **kw)
        if not ok: rec(False, "%s: %s" % (desc, r)); continue
        good = r["calls"] == exp_calls and r["sp_after"] == (0 if exp_cleared else r["top"]) and r["regs"]
        rec(good, "free: %s" % desc + ("" if good else "   GOT calls %s, signalhandler_sp %08x, registers %s; EXPECTED calls %s, cleared %s" % (r["calls"], r["sp_after"], "kept" if r["regs"] else "DAMAGED", exp_calls, exp_cleared)))
    # ---- __pthread_prog_fini (16.2.0-8 and later)
    if "__pthread_prog_fini" in lib.size:
        for desc, rc, running, exp_calls, exp_running in pfini_scenarios():
            ok, r = run_pfini(lib, rc, running)
            if not ok: rec(False, "%s: %s" % (desc, r)); continue
            good = r["calls"] == exp_calls and r["running_after"] == exp_running and r["regs"]
            rec(good, "pthread_prog_fini: %s" % desc + ("" if good else "   GOT calls %s, running %d, registers %s; EXPECTED calls %s, running %d" % (r["calls"], r["running_after"], "kept" if r["regs"] else "DAMAGED", exp_calls, exp_running)))
    # ---- mmap_too_big
    for clamp in CLAMPS:
        bad = []
        for length in LENS:
            ok, r = run_too_big(lib, length, clamp)
            if not ok: bad.append("%d: %s" % (length, r)); continue
            ans, n = want_too_big(length, clamp)
            if r["r0"] != ans or len(r["swis"]) != n or not r["regs"]: bad.append("len %d: answer %d (expected %d), %d SWIs (expected %d)%s" % (length, r["r0"], ans, len(r["swis"]), n, "" if r["regs"] else ", REGISTERS DAMAGED"))
        rec(not bad, "mmap_too_big: %d lengths from 0 to 4 GB - 1 with the clamp %s%s" % (len(LENS), clamp if isinstance(clamp, str) else "%d MB" % (clamp >> 20) if (1 << 20) <= clamp < 1 << 30 else hex(clamp), "" if not bad else "   " + "; ".join(bad[:3])))
    # ---- mmap () and mremap ()
    for clamp in ("none", 128 * MB, "swierr"):
        bad = []
        for length in (MB, 16 * MB, 128 * MB, 128 * MB + 4096, 150 * MB, (1 << 31) - 1, 1 << 31, 0xFFFFFFFF):
            ok, r = run_mmap(lib, "mmap", [0, length, 3, 2], [0xFFFFFFFF, 0], clamp, "err")
            if not ok: bad.append("mmap %d: %s" % (length, r)); continue
            refuse = want_too_big(length, clamp)[0]
            reached = [c for c in r["calls"] if c[0] == 0x59D04]
            fine = r["r0"] == 0xFFFFFFFF and r["errno"] == 12 and r["regs"] and (len(reached) == 0) == bool(refuse)
            if fine and reached: fine = reached[0][1] == {0: 0, 1: 0, 2: length, 3: 3, 4: 2, 5: 0xFFFFFFFF, 6: 0}           # MAP, addr, len, prot, flags, fd, offset unchanged
            if not fine: bad.append("mmap %d: refused by UnixLib %s, expected %s; r0=%08x errno=%d" % (length, len(reached) == 0, bool(refuse), r["r0"], r["errno"]))
        rec(not bad, "mmap (): refused (ENOMEM, ARMEABISupport not asked) exactly when mmap_too_big says so, else MMapOp gets the arguments unchanged; clamp %s%s" % (clamp if isinstance(clamp, str) else "%d MB" % (clamp >> 20), "" if not bad else "   " + "; ".join(bad[:3])))
        bad = []
        for old, new in ((MB, 2 * MB), (MB, 128 * MB), (MB, 129 * MB), (MB, 3 << 30), (100 * MB, 150 * MB), (200 * MB, 150 * MB), (150 * MB, 150 * MB), (4 * MB, MB), (MB, 0xFFFFFFFF)):
            ok, r = run_mmap(lib, "mremap", [0x6EE67000, old, new, 1], [], clamp, "ok")
            if not ok: bad.append("mremap %d->%d: %s" % (old, new, r)); continue
            refuse = want_too_big(new, clamp)[0] if new > old else 0
            reached = [c for c in r["calls"] if c[0] == 0x59D04]
            if refuse: fine = r["r0"] == 0xFFFFFFFF and r["errno"] == 12 and not reached and r["regs"]
            else: fine = r["r0"] == 0x6EE67000 and len(reached) == 1 and r["regs"] and reached[0][1] == {0: 5, 1: 0x6EE67000, 2: old, 3: new, 4: 1}
            if not fine: bad.append("mremap %d -> %d: r0=%08x errno=%d reached MMapOp %s (expected refused %s)" % (old, new, r["r0"], r["errno"], bool(reached), bool(refuse)))
        rec(not bad, "mremap (): growth refused exactly when mmap_too_big says so (shrinking and same size never), else MMapOp gets the arguments unchanged; clamp %s%s" % (clamp if isinstance(clamp, str) else "%d MB" % (clamp >> 20), "" if not bad else "   " + "; ".join(bad[:3])))
    return res

# ---------------------------------------------------------------- mutants: every conditional instruction flipped once, every immediate of the new code changed by one step
OPP = {0: 1, 1: 0, 2: 3, 3: 2, 4: 5, 5: 4, 6: 7, 7: 6, 8: 9, 9: 8, 10: 11, 11: 10, 12: 13, 13: 12}

def where(lib, pc):
    """function+offset of an address, from the symbol table"""
    best = None
    for nm, a in lib.syms.items():
        if a <= pc < a + lib.size.get(nm, 0) and (best is None or a > best[1]): best = (nm, a)
    return "%s+0x%x" % (best[0], pc - best[1]) if best else "%08x" % pc

def read_allow(path):
    """tools/sim-exit-hooks.equivalent: lines  FUNCTION+0xOFFSET WORD  # why this mutant cannot change behaviour (reviewed by hand)"""
    ok = {}
    if os.path.exists(path):
        for ln in open(path, encoding="utf-8"):
            ln = ln.split("#")[0].split()
            if len(ln) == 2: ok[(ln[0], int(ln[1], 16))] = True
    return ok

def mutants(lib):
    """(description, {address: word}) for the three new or changed functions and the few instructions around the two calls of mmap_too_big in mmap and mremap"""
    spans = [(lib.syms["__signalhandler_stack_free"], lib.size["__signalhandler_stack_free"], 6), (lib.syms["mmap_too_big"], lib.size["mmap_too_big"], 0),
             (lib.syms["__pthread_prog_fini"], lib.size["__pthread_prog_fini"], 0)]
    for fn in ("mmap", "mremap"):                                   # the window around the bl mmap_too_big: from 4 instructions before to 8 after
        a, n = lib.syms[fn], lib.size[fn]
        for k in range(0, n, 4):
            w = lib.read32(a + k)
            if (w >> 24) == 0xEB and (a + k + 8 + (((w & 0xFFFFFF) ^ 0x800000) - 0x800000) * 4) == lib.syms["mmap_too_big"]:
                spans.append((a + k - 16, 48, 0))
    out = []
    for (start, size, skip) in spans:
        for k in range(skip * 4, size, 4):
            pc = start + k; w = lib.read32(pc)
            if w is None: continue
            cc = w >> 28; kind = (w >> 25) & 7
            if cc in OPP and (w & 0x0F000000) not in (0x0F000000,):
                out.append(("%08x: %08x -> condition %s flipped" % (pc, w, cc), {pc: (OPP[cc] << 28) | (w & 0x0FFFFFFF)}))
            if kind == 1 and (w & 0xFF) not in (0xFF,) and ((w >> 12) & 15) != 12 and ((w >> 16) & 15) != 13:   # data processing with an immediate (not the probe's ip, not sp arithmetic)
                out.append(("%08x: %08x -> immediate + 1" % (pc, w), {pc: (w & ~0xFF) | ((w & 0xFF) + 1)}))
    return out

def main():
    if len(sys.argv) != 2: sys.exit(__doc__)
    lib = Lib(sys.argv[1])
    bad = lib.check_plt()
    print("-- the library: %d PLT entries mapped to their relocations (%d mismatches), GOT base %08x" % (len(lib.plt), bad, lib.got_base))
    if bad: print("  FAIL the PLT/GOT model does not match this library"); return 1
    res = evaluate(lib, verbose=True)
    ok_all = all(ok for ok, _ in res)
    if not ok_all:                                                  # with a failing baseline every mutant would look caught: the mutation stage means nothing
        print("exit-hook simulation: FAILED (%d scenarios; the mutants were not run because the unmodified library already fails)" % len(res)); return 1
    print("-- mutants: every conditional instruction flipped, every immediate changed by one")
    survivors = []; reviewed = []; muts = mutants(lib)
    allow = read_allow(os.path.join(here, "sim-exit-hooks.equivalent"))
    for desc, patch in muts:
        lib.patches = patch
        r = evaluate(lib)
        if all(ok for ok, _ in r):
            pc = next(iter(patch)); orig = Elf.read32(lib, pc)                  # the original word (lib.read32 would give the mutated one)
            (reviewed if (where(lib, pc), orig) in allow else survivors).append("%s (%s)" % (desc, where(lib, pc)))
    lib.patches = {}
    for s in reviewed: print("  survived, reviewed as equivalent: " + s)
    for s in survivors: print("  SURVIVED " + s)
    print("  %d mutants, %d caught, %d survived but are reviewed as equivalent (tools/sim-exit-hooks.equivalent), %d NOT reviewed" % (len(muts), len(muts) - len(survivors) - len(reviewed), len(reviewed), len(survivors)))
    good = ok_all and not survivors
    print("exit-hook simulation: " + ("OK" if good else "FAILED") + " (%d scenarios)" % len(res))
    return 0 if good else 1

if __name__ == "__main__":
    try:
        sys.exit(main())
    except Fault as e:
        print("  FAIL simulation fault: %s" % e); sys.exit(1)
