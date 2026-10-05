#!/usr/bin/env python3
"""Run sul_fork () (vfork: no copy of the parent) of two BUILT SharedUnixLibrary modules on the small ARM (A32) interpreter and compare what it makes of the child's process structure:
  OLD module (1.16-vforkfix2):  the child's PROC_STATUS is a copy of the parent's, flag IS_EXECED (1 << 22) included: sul_exit takes a vfork child of an exec'd program for a NEW client and calls
                                SOM_DeregisterClient, which frees the PARENT's Shared Object Manager client (found 2026-10-04, RunTraceUL1);
  NEW module (1.16-vforkfix3):  the child's PROC_STATUS is the parent's with IS_EXECED cleared and nothing else changed; every other word of the child's structure, every other memory write, every SWI
                                and every register at the end are the same as with the old module.
Parent statuses: every combination of the flags IS_FORKED / IS_EXECED / IS_DYNAMIC / IS_ARMEABI, with and without a return code, the zombie bit and a signal number.
usage: sim-sul-fork.py OLD.bin OLD.o NEW.bin NEW.o        (exit status 0 = everything right)"""
import importlib.util, itertools, os, sys
here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("simdiff", os.path.join(here, "sim-sul-trace-diff.py"))
dif = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(dif)
st, DSim, Fault, M = dif.st, dif.DSim, dif.Fault, 0xFFFFFFFF

P, VAR, FDARR, HEAP = 0x50000000, 0x50002000, 0x50010000, 0x50400000
SENT = 0xFFFF0000
PROC_SIZE = 188 + 48 + 40                      # PROC_FORK_STORAGE + 4*12 + 10*4 (see sul.s)
F = dict(NEXT=0, PID=20, PPID=24, FDARRAY=64, MAXFD=68, FDSIZE=72, TTYCONSOLE=84, TTYRS423=88, ENVIRON=92, STATUS=96, CHILDREN=100, ENVIRONSIZE=112, STACK=124,
         CLI=136, OLDEXIT=140, INITIALAPPSPACE=160, INITIALHIMEM=164, PARENTADDR=172, PRIVATEWORD=184)
ST = dict(FORKED=1 << 21, EXECED=1 << 22, DYNAMIC=1 << 23, ARMEABI=1 << 24)

def run(raw, syms, status):
    cpu = DSim(raw)
    cpu.stack_ok = (0x7E000000, 0x7F000000); cpu.r[13] = 0x7F000000 - 64
    def put(a, v):
        for i in range(4): cpu.over[a + i] = (v >> (8 * i)) & 0xFF
    for k in range(0, PROC_SIZE + 16, 4): put(P + k, (0x11110000 + k * 0x101) & M)          # a recognisable parent structure
    for k, v in dict(PID=P >> 2, PPID=1, FDARRAY=FDARR, MAXFD=256, FDSIZE=12, TTYCONSOLE=0, TTYRS423=0, ENVIRON=0, CHILDREN=0, STACK=0x20376FD4, CLI=0x50004000, OLDEXIT=0x20001000,
                     INITIALAPPSPACE=0x01000000, INITIALHIMEM=0x00FF0000, PARENTADDR=0x50100000).items(): put(P + F[k], v)
    put(P + F["STATUS"], status)
    for k in range(0, 256 * 12, 4): put(FDARR + k, 0)                                          # no open file descriptors
    put(VAR, P)
    cpu.r[0] = P >> 2; cpu.r[1] = VAR; cpu.r[2] = 0; cpu.r[3] = 0                              # sul_fork (pid >> 2, &sulproc, stacklimit = 0, stack = 0): a vfork
    for i in range(4, 13): cpu.r[i] = 0xC0DE0000 + i
    cpu.r[14] = SENT
    calls = []; nxt = [HEAP]
    def swi(c, n):
        calls.append((n, c.r[0], c.r[3])); c.v = 0
        if n == 0x2001E and c.r[0] == 6:                                                       # XOS_Module: claim a block
            c.r[2] = nxt[0]; nxt[0] += (c.r[3] + 15) & ~15
    cpu.swi_hook = swi
    cpu.run(syms["sul_fork"], {SENT})
    child = {k: cpu.rd32(HEAP + k) for k in range(0, PROC_SIZE, 4)}
    return calls, child, cpu.writes, [cpu.r[i] for i in range(16)], cpu.rd32(VAR)

def main():
    if len(sys.argv) != 5: sys.exit(__doc__)
    oraw, osyms, nraw, nsyms = st.Raw(sys.argv[1]), st.symbols(sys.argv[2]), st.Raw(sys.argv[3]), st.symbols(sys.argv[4])
    ok_all = True; n = 0; bad = 0
    for combo in itertools.product((0, 1), repeat=4):
        for extra in (0x00000000, 0x00000045, 0x000F8000 | 0x1234):
            status = sum(v for (k, v), on in zip(ST.items(), combo) if on) | extra
            n += 1
            oc, och, ow, orr, ov = run(oraw, osyms, status)
            nc, nch, nw, nrr, nv = run(nraw, nsyms, status)
            st_o, st_n = och[F["STATUS"]], nch[F["STATUS"]]
            same_rest = {k: v for k, v in och.items() if k != F["STATUS"]} == {k: v for k, v in nch.items() if k != F["STATUS"]}
            # every write outside the module image except the child's status word (offset 96 of the block) must be the same, in the same order
            strip = lambda ws, size: [w for w in ws if w[0] >= size and not (HEAP <= w[0] < HEAP + 4 and False)]
            ow2 = [w for w in ow if w[0] >= len(oraw.d)]; nw2 = [w for w in nw if w[0] >= len(nraw.d)]
            same_writes = [w for w in ow2 if not (HEAP + F["STATUS"] <= w[0] < HEAP + F["STATUS"] + 4)] == [w for w in nw2 if not (HEAP + F["STATUS"] <= w[0] < HEAP + F["STATUS"] + 4)]
            same_regs = [r for i, r in enumerate(orr) if i != 14] == [r for i, r in enumerate(nrr) if i != 14]
            good = (st_o == status and st_n == (status & ~ST["EXECED"]) and same_rest and same_writes and same_regs and oc == nc and ov == nv == HEAP)
            if not good:
                ok_all = False; bad += 1
                if bad <= 5: print("  FAIL parent status %08x: old child %08x (expect %08x), new child %08x (expect %08x); rest same %s, writes same %s, registers same %s, SWIs same %s" % (
                        status, st_o, status, st_n, status & ~ST["EXECED"], same_rest, same_writes, same_regs, oc == nc))
    print("%s: %d parent statuses: the old module copies the status word as it is (IS_EXECED included), the new one clears IS_EXECED and changes nothing else (child structure, memory writes, SWIs, registers)" % ("ok  " if ok_all else "FAIL", n))
    return 0 if ok_all else 1
if __name__ == "__main__":
    try:
        sys.exit(main())
    except Fault as e:
        print("  FAIL simulation fault: %s" % e); sys.exit(1)
