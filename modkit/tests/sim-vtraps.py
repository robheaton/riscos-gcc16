#!/usr/bin/env python3
"""sim-vtraps.py -- vector-traps: and pdriver-handler: of cmunge (tests/veneers/vt4.*) on the A32 interpreter with the kernel model: the module is built with the cross compiler (cmunge, gcc -mmodule, modreloc), loaded and
initialised, then

  - a vector trap veneer, entered as the kernel enters a vector (r12 = the private word, lr = the pass-on address, the claim address on the stack) with a stand-in for the rest of the vector chain at the pass-on address:
    the handler gets the registers as a block, the private word, trap and trappw and an 8 byte aligned stack; NULL claims the vector (registers as the handler left them, flags as they were); VECTORTRAP_PASSON passes it on
    (the chain gets the registers, r10 and r11 as they were at the entry); an error block claims with V set and r0 = the block; trap () runs the rest of the chain with the registers of the block it is given and returns NULL or
    the error that the chain claimed with, and the registers come back into the block.
  - the printer driver entry, called as the PDriver module calls it (r11 = the reason code): the function of the reason code gets the code, the registers and the private word; NULL returns success with the registers
    as the function left them, V clear; an error block returns with V set and r0 = it; a code out of range, or error_BAD_PDRIVER_SWI, returns the system's BadSWI error.

TC = the tool chain (default: the work area's tc-dev, else env-f).  Exit status 0 = everything right."""
import os, struct, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE); sys.path.insert(0, os.path.join(HERE, "..", "..", "modpoc")); sys.path.insert(0, os.path.expanduser("~/gccsdk-next/tools"))
from riscosmodel import RiscosModel
from a32 import Fault

TC = os.environ.get("TC") or next(p for p in (os.path.expanduser("~/gccsdk-next/tc-dev/riscos-gcc16-cross-16.2.0-14-x86_64-linux"), os.path.expanduser("~/gccsdk-next/env-f")) if os.path.exists(p))
BIN = os.path.join(TC, "bin")
CC = os.path.join(BIN, "arm-riscos-gnueabihf-gcc"); NM = os.path.join(BIN, "arm-riscos-gnueabihf-nm")
CMUNGE = os.path.join(BIN, "cmunge"); MODRELOC = os.path.join(BIN, "arm-riscos-gnueabihf-modreloc")
W = tempfile.mkdtemp(prefix="vtraps-")
SRC = os.path.join(HERE, "veneers")
fails = 0
def check(ok, what):
    global fails
    print("  %s  %s" % ("ok  " if ok else "FAIL", what))
    if not ok: fails += 1
def sh(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode: sys.exit("%s\n%s" % (" ".join(cmd), r.stderr))
    return r
sh([CMUNGE, "-tgcc", "-32bit", "-s", W + "/h.s", "-d", W + "/VecTrapTest.h", SRC + "/vt4.cmhg"])
MUT = os.environ.get("MUTATE")                                       # "old=>new": a change in the generated assembler source, to see that the test notices it
if MUT:
    src = open(W + "/h.s").read(); o, n = MUT.split("=>")
    if o not in src: sys.exit("MUTATE: %r is not in the generated source" % o)
    open(W + "/h.s", "w").write(src.replace(o, n, 1))
sh([CC, "-mmodule", "-c", W + "/h.s", "-o", W + "/h.o"])
sh([CC, "-mmodule", "-O2", "-Wall", "-I" + W, "-c", SRC + "/vt4.c", "-o", W + "/vt4.o"])
sh([CC, "-mmodule", "-o", W + "/VT4.elf", W + "/vt4.o", W + "/h.o"])
sh([MODRELOC, W + "/VT4.elf", W + "/VT4,ffa"])
data = open(W + "/VT4,ffa", "rb").read()
syms = {}
for ln in sh([NM, W + "/VT4.elf"]).stdout.split("\n"):
    p = ln.split()
    if len(p) == 3: syms[p[2]] = int(p[0], 16)
print("built VT4,ffa: %d bytes" % len(data))

BASE = 0x01C21000
k = RiscosModel(max_steps=2_000_000)
mod = k.load(data, BASE); cpu = k.cpu; cpu.r[13] = k.sp0
r0, v = k.init(mod)
assert r0 == 0 and v == 0
M = 0xFFFFFFFF

CLAIM = 0xFFFF0A00                                                       # what the kernel stacks on a vector: the address that a claim returns to
S1 = "chain_ok"                                                          # the rest of the vector chain (in vt4.c: the interpreter runs the code of the module image only): the chain claims
S2 = "chain_err"                                                         # the chain claims with an error (r9 = the error block)
chain_err = k.error_block(0x4567, "VecTrapTest chain error")
vt_err = None

def call_trap(entry, regs, flags=(0, 0, 0, 0), adj=0, chain=S1):
    """the veneer ENTRY called as a vector: the stack top is the claim address, lr the pass-on address (the stand-in for the chain); returns the registers and flags where the claim address is reached"""
    s = k._snapshot()
    cpu.steps = 0
    cpu.set_mode(0x13); cpu.cpsr_ctl = 0x13
    sp = k.sp0 - 0x300 - adj
    cpu.wr32(sp - 4, CLAIM)
    cpu.r[13] = sp - 4; cpu.r[14] = BASE + syms[chain]
    for i in range(12): cpu.r[i] = regs.get(i, 0x30000000 + 0x101 * i)
    cpu.r[12] = mod.pw
    cpu.n, cpu.z, cpu.c, cpu.v = flags
    before = list(cpu.r)
    fault = None
    try: cpu.run(BASE + syms[entry], {CLAIM})
    except Fault as f: fault = str(f)
    after = list(cpu.r); fl = (cpu.n, cpu.z, cpu.c, cpu.v)
    k._restore(s)
    return before, after, fl, fault, sp

def err_text(ptr):
    return (cpu.rd32(ptr), k.read_cstr(ptr + 4)) if 0x1000 < ptr < 0x10000000 else None

FLAGS = [(0, 0, 0, 0), (1, 0, 1, 0), (0, 1, 0, 0), (1, 1, 1, 1)]
print("vector trap veneers")
for adj in (0, 4):
    for fl in FLAGS:
        tag = "NZCV=%d%d%d%d, stack %s" % (fl + ("8 byte aligned" if adj == 0 else "4 byte aligned",))
        regs = {0: 0, 1: 10, 2: 3}
        b, a, f, fault, sp = call_trap("vta", regs, fl, adj)
        pr = []
        if fault: pr.append("fault %s" % fault)
        else:
            if a[1] != 23: pr.append("r1 %d, expected 23" % a[1])
            if a[5] != mod.pw: pr.append("the handler did not get the private word")
            if a[6] != 0: pr.append("the handler's sp & 7 was %d" % a[6])
            if a[7] != sp - 4 - 52: pr.append("the block is not the 13 stacked words")
            if not (BASE <= a[8] < BASE + len(data)): pr.append("trap %#x is not in the module" % a[8])
            if a[9] != a[7]: pr.append("trappw is not the block")
            for i in (0, 2, 3, 4, 10, 11):
                if a[i] != b[i]: pr.append("r%d changed" % i)
            if f != fl: pr.append("flags %s, expected %s" % (f, fl))
            if a[13] != sp: pr.append("sp %#x, expected %#x" % (a[13], sp))
        check(not pr, "NULL claims the vector: the registers as the handler left them, the flags and sp as they were (%s)%s" % (tag, "" if not pr else ": " + "; ".join(pr)))
        b, a, f, fault, sp = call_trap("vta", {0: 1, 1: 10, 2: 3}, fl, adj)
        pr = []
        if fault: pr.append("fault %s" % fault)
        else:
            if (a[1], a[2]) != (11, 0x77): pr.append("the chain did not run with the registers: r1 %d r2 %#x" % (a[1], a[2]))
            if a[6] != b[5] or a[7] != b[10] or a[8] != b[11]: pr.append("the chain got other r5, r10 or r11")
            if f != fl or a[13] != sp: pr.append("flags or sp changed")
        check(not pr, "VECTORTRAP_PASSON: the chain runs, r10 and r11 as they were, the registers as at the entry (the handler's r2 = 0x1234 goes on too) (%s)%s" % (tag, "" if not pr else ": " + "; ".join(pr)))
        b, a, f, fault, sp = call_trap("vta", {0: 2, 1: 10, 2: 3}, fl, adj)
        pr = []
        if fault: pr.append("fault %s" % fault)
        else:
            if err_text(a[0]) != (0x2345, "VecTrapTest vector error"): pr.append("r0 = %#x is not the error" % a[0])
            if f != fl[:3] + (1,): pr.append("flags %s, expected V set and NZC as they were" % (f,))
            for i in (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11):
                if a[i] != b[i]: pr.append("r%d changed" % i)
            if a[13] != sp: pr.append("sp changed")
        check(not pr, "an error block: the vector is claimed, V set, r0 = the block, the chain did not run (%s)%s" % (tag, "" if not pr else ": " + "; ".join(pr)))
        b, a, f, fault, sp = call_trap("vta", {0: 3, 1: 10, 2: 3, 5: 0x66}, fl, adj)
        pr = []
        if fault: pr.append("fault %s" % fault)
        else:
            if (a[1], a[2], a[3], a[4]) != (11, 0x77, 0x33, 2): pr.append("r1-r4 = %d %#x %#x %d, expected 11 0x77 0x33 2" % (a[1], a[2], a[3], a[4]))
            if a[6] != 0x66 or a[7] != b[10] or a[8] != b[11]: pr.append("the chain got other r5, r10 or r11 (r6 %#x)" % a[6])
            if a[0] != 3 or f != fl or a[13] != sp: pr.append("r0, flags or sp changed (r0 %d)" % a[0])
            if a[9] != b[9] or a[10] != b[10] or a[11] != b[11]: pr.append("r9 - r11 changed")
        check(not pr, "trap () runs the rest of the chain with the block's registers, they come back into the block, it returns NULL (%s)%s" % (tag, "" if not pr else ": " + "; ".join(pr)))
        b, a, f, fault, sp = call_trap("vta", {0: 4, 1: 10, 2: 3, 5: 0x66}, fl, adj)
        pr = []
        if fault: pr.append("fault %s" % fault)
        else:
            if a[6] != 0x55: pr.append("the chain got r5 = %#x, not what the handler put in the block (0x55)" % a[6])
            if (a[1], a[2], a[4]) != (11, 0x77, 2): pr.append("r1 r2 r4 = %d %#x %d" % (a[1], a[2], a[4]))
        check(not pr, "the handler changes the block before trap (): the chain gets the changed registers (%s)%s" % (tag, "" if not pr else ": " + "; ".join(pr)))
        b, a, f, fault, sp = call_trap("vta", {0: 3, 1: 10, 2: 3, 9: chain_err}, fl, adj, chain=S2)
        pr = []
        if fault: pr.append("fault %s" % fault)
        else:
            if a[0] != chain_err or f != fl[:3] + (1,): pr.append("r0 %#x, flags %s: expected the chain's error and V set" % (a[0], f))
            if a[4] != 1: pr.append("trap () did not return the error (r4 = %d)" % a[4])
            if a[13] != sp: pr.append("sp changed")
        check(not pr, "the chain claims with an error: trap () returns it, the handler returns it, the vector is claimed with V set (%s)%s" % (tag, "" if not pr else ": " + "; ".join(pr)))
        b, a, f, fault, sp = call_trap("vta", {0: 5, 1: 10, 2: 3}, fl, adj)
        pr = []
        if fault: pr.append("fault %s" % fault)
        else:
            if (a[1], a[2]) != (11, 0x77) or a[3] != b[3] or a[4] != b[4]: pr.append("the chain did not get the entry's registers")
        check(not pr, "passing on without trap (): the registers are the entry's (%s)%s" % (tag, "" if not pr else ": " + "; ".join(pr)))
    b, a, f, fault, sp = call_trap("vtb", {}, FLAGS[1], adj)
    check(fault is None and a[9] == mod.pw + 1 and f == FLAGS[1] and a[13] == sp, "the second veneer vtb calls vtb_handler (the default name): r9 = pw + 1, the flags kept")
print()

RET = 0xFFFF0900
def call_pd(reason, regs, flags=(0, 0, 0, 0), adj=0):
    """the printer driver entry called as the PDriver module calls it: r11 = the reason code, r12 = the private word, SVC mode, lr = RET"""
    s = k._snapshot()
    cpu.steps = 0
    cpu.set_mode(0x13); cpu.cpsr_ctl = 0x13
    sp = k.sp0 - 0x300 - adj
    cpu.r[13] = sp; cpu.r[14] = RET
    for i in range(12): cpu.r[i] = regs.get(i, 0x40000000 + 0x103 * i)
    cpu.r[11] = reason; cpu.r[12] = mod.pw
    cpu.n, cpu.z, cpu.c, cpu.v = flags
    before = list(cpu.r)
    fault = None
    try: cpu.run(BASE + syms["pdentry"], {RET})
    except Fault as f: fault = str(f)
    after = list(cpu.r); fl = (cpu.n, cpu.z, cpu.c, cpu.v)
    k._restore(s)
    return before, after, fl, fault, sp

print("printer driver entry")
for adj in (0, 4):
    for fl in ((0, 0, 0, 0), (1, 0, 1, 0), (0, 1, 1, 1), (1, 1, 0, 1)):
        tag = "NZCV=%d%d%d%d, stack %s" % (fl + ("8 byte aligned" if adj == 0 else "4 byte aligned",))
        for reason in (0, 2):
            b, a, f, fault, sp = call_pd(reason, {0: 7, 1: 100, 2: 5}, fl, adj)
            pr = []
            if fault: pr.append("fault %s" % fault)
            else:
                if a[1] != 105 or a[6] != reason or a[7] != mod.pw or a[8] != 0: pr.append("r1 %d (105), r6 %d (%d), r7 %#x, sp & 7 = %d" % (a[1], a[6], reason, a[7], a[8]))
                for i in (0, 2, 3, 4, 5, 9, 10, 11):
                    if a[i] != b[i]: pr.append("r%d changed" % i)
                if f != (fl[0], fl[1], fl[2], 0): pr.append("flags %s: expected V clear and NZC as they were" % (f,))
                if a[13] != sp or a[15] != RET: pr.append("sp or the return address")
            check(not pr, "reason %d (pd_info): the function gets the code, the block and the private word; success with V clear (%s)%s" % (reason, tag, "" if not pr else ": " + "; ".join(pr)))
        b, a, f, fault, sp = call_pd(1, {0: 0, 3: 9}, fl, adj)
        check(fault is None and a[3] == 0x77 and a[6] == 1 and f == (fl[0], fl[1], fl[2], 0) and a[0] == 0, "reason 1 (pd_set) with r0 = 0: success, r3 = 0x77 (%s)" % tag)
        b, a, f, fault, sp = call_pd(1, {0: 1}, fl, adj)
        pr = []
        if fault: pr.append("fault %s" % fault)
        else:
            if err_text(a[0]) != (0x2346, "VecTrapTest printer error"): pr.append("r0 = %#x is not the error" % a[0])
            if f != (fl[0], fl[1], fl[2], 1): pr.append("flags %s: expected V set and NZC as they were" % (f,))
            for i in (1, 2, 3, 4, 5, 9, 10, 11):
                if a[i] != b[i]: pr.append("r%d changed" % i)
            if a[6] != 1 or a[13] != sp: pr.append("r6 or sp")
        check(not pr, "an error block: V set, r0 = the block, the other registers as the function left them (%s)%s" % (tag, "" if not pr else ": " + "; ".join(pr)))
        for reason, r0 in ((1, 2), (3, 0), (4, 0), (63, 0), (0x7FFFFFFF, 0)):
            b, a, f, fault, sp = call_pd(reason, {0: r0}, fl, adj)
            pr = []
            if fault: pr.append("fault %s" % fault)
            else:
                if err_text(a[0]) != (0x1E6, "SWI value out of range for module VecTrapTest"): pr.append("r0 = %#x %r is not the BadSWI error" % (a[0], err_text(a[0])))
                if f != (fl[0], fl[1], fl[2], 1) or a[13] != sp: pr.append("flags %s or sp" % (f,))
                for i in (1, 2, 3, 4, 5, 9, 10, 11):
                    if a[i] != b[i] and not (reason == 1 and i == 6): pr.append("r%d changed" % i)
            check(not pr, "%s: the system's BadSWI error with the module title (%s)%s" % ("reason %d, a function that says error_BAD_PDRIVER_SWI" % reason if reason == 1 else "reason %d is out of range (3 handlers)" % reason, tag, "" if not pr else ": " + "; ".join(pr)))
print()
print("ALL OK" if not fails else "%d FAILED" % fails)
sys.exit(1 if fails else 0)
