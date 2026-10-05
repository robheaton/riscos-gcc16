#!/usr/bin/env python3
"""Differential test: does the TRACE instrumentation change what sul_exit () of the SharedUnixLibrary module DOES?
Runs sul_exit () of the PLAIN module (SharedULib-116fix2,ffa) and of the TRACED one (SharedULib-116fix2t,ffa, built with -DSULTRACE) on the small ARM (A32) interpreter, from the entry to the point where the
process is left (the resume of the parent: pc = a sentinel taken from PROC_FORK_STORAGE; or the OS_Exit SWI of a process without a parent), for many process structures:
  every combination of the status flags IS_FORKED / IS_EXECED / IS_DYNAMIC / IS_ARMEABI  x  PROC_STACK 0 / a handle  x  PROC_PARENTADDR 0 (a vfork child that did not exec) / set (an exec'd child: copy_down_parent,
  with real memory to move)  x  PPID 1 (no parent: Cleanup, OS_Exit) / a parent (has_parent)  x  the old exit/error handlers recorded or not.
and compares, for the two modules, everything the module does that is NOT the trace: the sequence of SWIs (number and r0-r3, the trace routine's own SWIs left out: they are made with sp = the private stack),
every write to memory outside the module image, and the registers at the end (r0-r14; the flags are not compared: the TRACE macro changes them).  Addresses inside the module image (a string passed to OS_Exit) are
compared as  symbol+offset  because the two images are laid out differently.
usage: sim-sul-trace-diff.py PLAIN.bin PLAIN.o TRACED.bin TRACED.o        (exit status 0 = every scenario identical)"""
import importlib.util, itertools, os, struct, sys
here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("simtrace", os.path.join(here, "sim-sul-trace.py"))
st = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(st)
Fault, M = st.Fault, 0xFFFFFFFF

class Done(Exception): pass

class DSim(st.TSim):
    """TSim plus what the whole of sul_exit needs: FPA LFM / SFM (the address arithmetic and the 48 bytes; the register values do not matter), LDM with pc, and a log of the writes"""
    def __init__(self, raw):
        super().__init__(raw); self.writes = []
    def wr8(self, a, v): self.writes.append((a, 1, v & 0xFF)); super().wr8(a, v)
    def wr32(self, a, val): self.writes.append((a, 4, val & M)); super().wr32(a, val)
    def step(self):
        pc = self.r[15]; w = self.elf.read32(pc)
        if w is not None:
            top = (w >> 25) & 7
            if top == 4 and (w & (1 << 20)) and (w & (1 << 15)):                                      # LDM with pc in the list
                self.steps += 1; self.r[15] = pc + 4
                if not self.cond(w >> 28): return
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
            if (w & 0x0FC000F0) == 0x00000090:                                                          # MUL / MLA (32-bit result)
                self.steps += 1; self.r[15] = pc + 4
                if not self.cond(w >> 28): return
                rd, rn, rs, rm, acc, sflag = (w >> 16) & 15, (w >> 12) & 15, (w >> 8) & 15, w & 15, (w >> 21) & 1, (w >> 20) & 1
                res = (self.r[rm] * self.r[rs] + (self.r[rn] if acc else 0)) & M
                self.set_reg(rd, res)
                if sflag: self.n, self.z = res >> 31, 1 if res == 0 else 0
                return
            if (w & 0x0E000000) == 0x0C000000 and ((w >> 8) & 15) in (1, 2):                          # FPA LFM / SFM (all uses in the module: 4 registers = 48 bytes)
                self.steps += 1; self.r[15] = pc + 4
                if not self.cond(w >> 28): return
                p, u, wb, ld, rn, off = (w >> 24) & 1, (w >> 23) & 1, (w >> 21) & 1, (w >> 20) & 1, (w >> 16) & 15, (w & 0xFF) * 4
                base = self.reg(rn); addr = ((base + off) if u else (base - off)) & M if p else base
                if off != 48: raise Fault("FPA transfer of %d bytes: not modelled" % off)
                if not ld:
                    for i in range(48): self.wr8(addr + i, 0)
                if wb or not p: self.set_reg(rn, ((base + off) if u else (base - off)) & M)
                return
        super().step()

P, CELL, PARENT_CELL, CLI, ENV = 0x50000000, 0x50001000, 0x50003000, 0x50004000, 0x50005000
SENT, PARENT_SP, H = 0xFFFF0000, 0x6706AD9C, 0x20376FD4
UP, STACK_TOP = 0x50100000, 0x50200000
ST = dict(FORKED=1 << 21, EXECED=1 << 22, DYNAMIC=1 << 23, ARMEABI=1 << 24)
F = dict(NEXT=0, PID=20, PPID=24, ENVIRON=92, STATUS=96, STACK=124, PARENTSULPROC=128, PARENTSULPROCADDR=132, CLI=136, OLDEXIT=140, OLDEXITR12=144, OLDERROR=148, OLDERRORR12=152, OLDERRORBUF=156,
         INITIALAPPSPACE=160, INITIALHIMEM=164, PARENTSTACK=168, PARENTADDR=172, PARENTSIZE1=176, PARENTSIZE2=180, PRIVATEWORD=184, FORK=188)
SWI_EXIT = 0x11

def namer(syms, size):
    items = sorted((v, k) for k, v in syms.items() if v < size)
    def name(v):
        if 0 <= v < size:
            best = None
            for a, k in items:
                if a <= v: best = (a, k)
                else: break
            return "<%s+%d>" % (best[1], v - best[0]) if best else "<image+%d>" % v
        return v
    return name

def run(raw, syms, entry, status, ppid, stack, parentaddr, olds):
    cpu = DSim(raw); size = len(raw.d)
    cpu.stack_ok = (0x7E000000, 0x7F000000); cpu.r[13] = 0x7F000000 - 64
    def put(a, v): cpu.over.update({a + i: (v >> (8 * i)) & 0xFF for i in range(4)})
    for a in range(0x8000, 0x8100, 4): put(a, (0x11110000 + a) & M)              # the first page of the application space (the trace reads [0x8038], [0x8040] and sums 0x8000-0x80ff)
    put(0x8038, 0x50006000); put(0x50006000, 0x6706CAFE); put(0x8040, 0x20F16000)
    for a in range(0, 0x200, 4): put(P + a, 0)
    for a in range(0, 0x100, 4): put(CELL + a, 0)
    put(CELL, P)                                                                       # the head of the SUL list is this process (delink finds it at once)
    f = lambda k, v: put(P + F[k], v)
    f("PID", P >> 2); f("PPID", ppid); f("STATUS", status | 0x03); f("STACK", stack)
    f("PARENTSULPROC", 0x50002000); f("PARENTSULPROCADDR", PARENT_CELL); f("PRIVATEWORD", CELL)
    f("CLI", CLI if olds else 0); f("ENVIRON", ENV if olds else 0)
    if olds:
        f("OLDEXIT", 0x20001000); f("OLDEXITR12", 0x2000); f("OLDERROR", 0x20002000); f("OLDERRORR12", 0x2001); f("OLDERRORBUF", 0x8000)
    if parentaddr:
        f("PARENTADDR", UP); f("PARENTSIZE1", 0x40); f("PARENTSIZE2", 0x20); f("PARENTSTACK", STACK_TOP); f("INITIALAPPSPACE", 0x01000000); f("INITIALHIMEM", 0x00FF0000)
        for k in range(0x40 + 0x20): cpu.over[UP + k] = (k * 7 + 3) & 0xFF
    for k in range(48): cpu.over[P + F["FORK"] + k] = 0
    regs = [0x6700 + i for i in range(10)]                                              # the saved v1-v6, sl, fp, sp, pc of the parent
    regs[8] = PARENT_SP; regs[9] = SENT
    for i, v in enumerate(regs): put(P + F["FORK"] + 48 + 4 * i, v)
    cpu.r[0] = P >> 2; cpu.r[1] = 0x55                                                  # sul_exit (pid >> 2, status)
    for i in range(2, 13): cpu.r[i] = 0xC0DE0000 + i
    cpu.r[14] = 0xAAAA0000
    calls = []
    def swi(c, n):
        if "trace_stack_top" in syms and c.r[13] == syms["trace_stack_top"]:            # the trace routine's own SWIs: answered, not logged
            c.v = 0
            if n == 0x2003A: c.c = 0 if (0x50000000 <= c.r[0] < 0x50300000 and c.r[0] % 4 == 0) else 1   # OS_ValidateAddress: only the memory this test has set up is valid (a restored word that is garbage is not)
            elif n == 0x2000D and c.r[0] == 0xC0: c.r[0] = 77
            elif n == 0x20009 and c.r[0] == 2: c.r[2] = 100
            elif n == 0x20042: c.r[0] = 0x1234
            return
        calls.append((n, c.r[0], c.r[1], c.r[2], c.r[3]))
        c.v = 0
        if n == SWI_EXIT: raise Done()
    cpu.swi_hook = swi
    end = "sentinel"
    try:
        cpu.run(entry, {SENT})
    except Done:
        end = "OS_Exit"
    nm = namer(syms, size)
    norm = lambda v: nm(v & M)
    calls = [(n, norm(a), norm(b), norm(c), norm(d)) for (n, a, b, c, d) in calls]
    writes = [w for w in cpu.writes if w[0] >= size]
    if end == "OS_Exit": final = None
    else:
        final = [norm(cpu.r[i]) for i in range(16)]
        # r14 is a return address into code that the trace code has moved: the symbol only (the offset in the symbol differs by the size of the inserted TRACE code); it is dead at this point anyway
        # (the resumed parent loads its own lr from __saved_lr)
        final[14] = nm(cpu.r[14]).split("+")[0] if isinstance(nm(cpu.r[14]), str) else nm(cpu.r[14])
        final = tuple(final)
    return end, calls, writes, final, any(w[0] < size for w in cpu.writes)

def symbols(o):
    s = st.symbols(o)
    return s

def main():
    if len(sys.argv) != 5: sys.exit(__doc__)
    praw, ptr, traw, ttr = st.Raw(sys.argv[1]), sys.argv[2], st.Raw(sys.argv[3]), sys.argv[4]
    ps, ts = symbols(ptr), symbols(ttr)
    pe, te = ps["sul_exit"], ts["sul_exit"]
    n = bad = 0; kinds = {}
    names = list(ST)
    for combo in itertools.product((0, 1), repeat=len(names)):
        status = sum(ST[nm] for nm, on in zip(names, combo) if on)
        for ppid, stack, parentaddr, olds in itertools.product((1, 0x1234), (0, H), (0, 1), (0, 1)):
            n += 1
            a = run(praw, ps, pe, status, ppid, stack, parentaddr, olds)
            b = run(traw, ts, te, status, ppid, stack, parentaddr, olds)
            ok = a[:4] == b[:4] and not a[4]
            kinds[a[0]] = kinds.get(a[0], 0) + 1
            if not ok:
                bad += 1
                if bad <= 5:
                    print("  FAIL scenario status=%08x ppid=%x stack=%x parentaddr=%d olds=%d" % (status, ppid, stack, parentaddr, olds))
                    for k, nmk in enumerate(("end", "SWIs", "writes", "final registers")):
                        if a[k] != b[k]: print("     %s differ:\n        plain : %s\n        traced: %s" % (nmk, a[k], b[k]))
                    if a[4]: print("     the PLAIN module wrote into its own image")
    print("%s: %d scenarios of sul_exit (%s): the traced module does exactly what the plain one does" % ("ok  " if not bad else "FAIL", n, ", ".join("%d end in %s" % (v, k) for k, v in kinds.items())) if not bad
          else "FAIL: %d of %d scenarios differ" % (bad, n))
    return 1 if bad else 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except Fault as e:
        print("  FAIL simulation fault: %s" % e); sys.exit(1)
