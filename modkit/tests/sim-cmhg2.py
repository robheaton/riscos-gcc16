#!/usr/bin/env python3
"""sim-cmhg2.py -- the CMHG directives and options that sim-veneers.py does not use (tests/veneers/vt2.* and vt3.*), on the A32 interpreter with the kernel model.  The modules are built with the cross compiler
(cmunge, gcc -mmodule, modreloc), loaded and initialised, then

  Vt2
  - swi-decoding-table with  Name/function : the SWI's own function gets the offset (the others go to swi-handler-code, an offset past the table gives error_BAD_SWI)
  - swi-decoding-code FN (one function with the registers in a block): name to number and number to name through the header's decoding code word
  - generic-veneers with  private-word: rN  (the handler gets rN, r12 and everything else come back as they were), carry-capable: (VENEER_SETCARRY = 2 returns with C set and V clear; other answers as before)
    and both options together;  a veneer without carry-capable: takes 2 as an error pointer
  - vector-handlers with  error-capable:  (0 claims, 1 passes on, anything else claims with V set and r0 = the error) and without it (every non-zero value passes on)
  - command handlers: handler: FN (a function of its own), no-handler: (the code word is 0), the table's handler for the others;  module-is-not-reentrant: is accepted
  Vt3
  - swi-decoding-code NAME/NUMBER (two functions; a SWI chunk without names), the command table  -  (no handler of the table), handler: for one command, none for the other (no handler)

TC = the tool chain (default: the work area's tc-rel17 unpacked tarball, else env-f); CMUNGE = the cmunge to use (default: the tool chain's).  Exit status 0 = everything right."""
import os, struct, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE); sys.path.insert(0, os.path.join(HERE, "..", "..", "modpoc")); sys.path.insert(0, os.path.expanduser("~/gccsdk-next/tools"))
from riscosmodel import RiscosModel, USR_SP, IRQ_STACK_TOP
from a32 import Fault

def find_tc():
    r = os.path.expanduser("~/gccsdk-next/tc-rel17")
    if os.path.isdir(r):
        for d in sorted(os.listdir(r)):
            if os.path.exists(os.path.join(r, d, "bin", "arm-riscos-gnueabihf-gcc")): return os.path.join(r, d)
    return os.path.expanduser("~/gccsdk-next/env-f")
TC = os.environ.get("TC") or find_tc()
BIN = os.path.join(TC, "bin")
CC = os.path.join(BIN, "arm-riscos-gnueabihf-gcc"); NM = os.path.join(BIN, "arm-riscos-gnueabihf-nm")
CMUNGE = os.environ.get("CMUNGE") or os.path.join(BIN, "cmunge"); MODRELOC = os.path.join(BIN, "arm-riscos-gnueabihf-modreloc")
W = tempfile.mkdtemp(prefix="cmhg2-")
SRC = os.path.join(HERE, "veneers")
fails = 0
def check(ok, what):
    global fails
    print("  %s  %s" % ("ok  " if ok else "FAIL", what))
    if not ok: fails += 1

def sh(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode: sys.exit("%s\n%s%s" % (" ".join(cmd), r.stdout, r.stderr))
    return r

def build(name, cmhg, header, cflags):
    sh([CMUNGE, "-tgcc", "-32bit", "-s", W + "/%s.s" % name, "-d", W + "/" + header, SRC + "/" + cmhg])
    sh([CC, "-mmodule", "-c", W + "/%s.s" % name, "-o", W + "/%s-h.o" % name])
    sh([CC, "-mmodule", "-O2", "-Wall", "-Wextra", "-I" + W, "-c", SRC + "/vt2.c", "-o", W + "/%s-c.o" % name] + cflags)
    sh([CC, "-mmodule", "-o", W + "/%s.elf" % name, W + "/%s-c.o" % name, W + "/%s-h.o" % name])
    sh([MODRELOC, W + "/%s.elf" % name, W + "/%s,ffa" % name])
    syms = {}
    for ln in sh([NM, W + "/%s.elf" % name]).stdout.split("\n"):
        p = ln.split()
        if len(p) == 3: syms[p[2]] = int(p[0], 16)
    return open(W + "/%s,ffa" % name, "rb").read(), syms

BASE = 0x01C21000
RET = 0xFFFF0900
CLAIM = 0xFFFF0A00
PW = None

def start(data):
    k = RiscosModel(max_steps=2_000_000)
    mod = k.load(data, BASE); k.cpu.r[13] = k.sp0
    r0, v = k.init(mod)
    assert r0 == 0 and v == 0
    return k, mod

def errblock(k, p):
    """(number, text) of the error block at P, or None"""
    if not (0x1000 < p < 0x10000000): return None
    return k.cpu.rd32(p), k.read_cstr(p + 4)

def run_command(k, mod, cmds, name):
    regs, v = k.call(mod.base + cmds[name][1], {0: k.put_string(""), 1: 0, 12: mod.pw, 13: k.sp0})
    return regs[0], v

def call_veneer(k, addr, regs, flags=(1, 0, 1, 0), mode=0x13, claim=None):
    """the veneer at ADDR called in MODE with the registers REGS (r12 = 0xC1C1C1C1 unless given) and the FLAGS; for a vector the kernel's claim address is stacked; returns the registers, the flags and the pc afterwards"""
    cpu = k.cpu
    s = k._snapshot()
    cpu.steps = 0
    cpu.set_mode(0x13); cpu.r[13], cpu.r[14] = k.sp0 - 0x200, 0x0B0B0B0B
    ctl = {0x13: 0x13, 0x12: 0x92, 0x10: 0x10}[mode]
    if mode != 0x13: cpu.set_mode(mode)
    cpu.cpsr_ctl = ctl
    sp = {0x13: k.sp0 - 0x300, 0x12: IRQ_STACK_TOP, 0x10: USR_SP}[mode]
    if claim is not None:
        cpu.wr32(sp - 4, claim); sp -= 4
    cpu.r[13] = sp; cpu.r[14] = RET
    for i in range(13): cpu.r[i] = regs.get(i, 0x70000000 + i)
    cpu.n, cpu.z, cpu.c, cpu.v = flags
    before = list(cpu.r)
    fault = None
    try: cpu.run(addr, {RET, CLAIM})
    except Fault as f: fault = str(f)
    after = list(cpu.r); fl = (cpu.n, cpu.z, cpu.c, cpu.v); ctl_after = (cpu.mode, cpu.cpsr_ctl)
    k._restore(s)
    return before, after, fl, fault, ctl_after

def judge(label, problems):
    check(not problems, label if not problems else "%s: %s" % (label, "; ".join(problems)))

def same(b, a, regs, problems, skip=()):
    for i in regs:
        if i not in skip and a[i] != b[i]: problems.append("r%d changed (%#x -> %#x)" % (i, b[i], a[i]))

# ================================================================ Vt2
data, syms = build("vt2", "vt2.cmhg", "Vt2.h", [])
print("built Vt2,ffa: %d bytes" % len(data))
k, mod = start(data); cpu = k.cpu; PW = mod.pw
print("the header")
check(mod.title == "Vt2" and mod.chunk == 0x58D00 and mod.swi_names == ["One", "Two", "Three"] and mod.swi_prefix == "V2", "title %r, chunk %#x, prefix %r, names %s" % (mod.title, mod.chunk, getattr(mod, "swi_prefix", None), mod.swi_names))
check(mod.hdr[10] != 0, "the header has a SWI decoding code word (%#x)" % mod.hdr[10])
cmds = {c[0]: c for c in mod.commands}
check(set(cmds) == {"V2_Table", "V2_Own", "V2_Help", "V2_Own2"}, "the four commands are in the table: %s" % sorted(cmds))
check(cmds["V2_Help"][1] == 0 and all(cmds[n][1] != 0 for n in ("V2_Table", "V2_Own", "V2_Own2")), "no-handler: gives the command the code word 0 (V2_Help), the others have code")
print()

print("SWIs: Name/function")
for off, want, what in ((0, 1000, "V2_One: v2_one (offset 0)"), (1, 2001, "V2_Two: v2_swi (no function of its own)"), (2, 3002, "V2_Three: v2_three")):
    cpu.r[13] = k.sp0
    regs, v = k.swi(0x58D00 + off, {0: 5})
    check(v == 0 and regs[0] == want, "%s -> r0 = %d" % (what, regs[0]))
for off in (3, 17, 63):
    cpu.r[13] = k.sp0
    regs, v = k.swi(0x58D00 + off, {0: 5})
    e = errblock(k, regs[0]) if v else None
    check(v == 1 and e is not None and e[0] == 0x1E6 and e[1] == "SWI value out of range for module Vt2", "offset %d (past the table): v2_swi answers error_BAD_SWI: %r" % (off, e))
regs, v = k.swi(0x39, {1: k.put_string("V2_Three")})
check(v == 0 and regs[0] == 0x58D02, "OS_SWINumberFromString V2_Three = %#x (the /function does not belong to the name)" % regs[0])
print()

print("swi-decoding-code FN")
DEC = BASE + mod.hdr[10]
def decode(regs):
    cpu.r[13] = k.sp0
    r, v = k.call(DEC, {**regs, 12: mod.pw, 13: k.sp0})
    return r
r = decode({0: 0xFFFFFFFF, 1: k.put_string("Zed")})
check(r[0] == 7, "name Zed -> %d" % r[0])
r = decode({0: 0xFFFFFFFF, 1: k.put_string("Zee")})
check(r[0] == 0xFFFFFFFF, "name Zee -> %#x (not known)" % r[0])
buf = k.alloc(32)
for i in range(32): cpu.wr8(buf + i, 0x2E)
r = decode({0: 7, 1: buf, 2: 2, 3: 32})
text = bytes(cpu.rd8(buf + i) for i in range(8))
check(text == b"..Zed..." and r[2] == 5 and r[3] == mod.pw and r[0] == 7 and r[1] == buf, "number 7 -> %r written at offset 2, r2 = %d, the handler got the private word (r3 = %#x), r0 and r1 kept" % (text, r[2], r[3]))
print()

print("commands: the table's handler, handler: FN, no-handler:")
for name, want in (("V2_Table", 0x2001), ("V2_Own", 0x2002), ("V2_Own2", 0x2003)):
    r0, v = run_command(k, mod, cmds, name)
    e = errblock(k, r0) if v else None
    check(e is not None and e[0] == want, "*%s -> error &%X (%s)" % (name, e[0] if e else 0, {"V2_Table": "v2_table, number 0", "V2_Own": "v2_own, number 1", "V2_Own2": "v2_own2, number 3"}[name]))
print()

print("generic veneers: private-word: and carry-capable:")
V = lambda n: BASE + syms[n]
FLAGS = [(0, 0, 0, 0), (1, 0, 1, 0), (0, 1, 0, 1), (1, 1, 1, 1)]
for mode, mname in ((0x13, "SVC"), (0x12, "IRQ")):
    for fl in FLAGS:
        tag = "%s mode, NZCV=%d%d%d%d" % ((mname,) + fl)
        # pw_a: the private word is in r4, r12 is somebody else's and must come back
        regs = {i: 0x10000 + 0x111 * i for i in range(13)}; regs[4] = mod.pw; regs[12] = 0xC1C1C1C1; regs[1] = 0x55
        b, a, f, flt, _ = call_veneer(k, V("pw_a"), regs, fl, mode)
        p = []
        if flt: p.append("fault %s" % flt)
        else:
            if a[8] != mod.pw: p.append("the handler got %#x, not the private word of r4" % a[8])
            if a[9] != 0x55: p.append("r9 %#x (the handler copies r1 of the block)" % a[9])
            same(b, a, range(13), p, skip=(8, 9))
            if a[12] != 0xC1C1C1C1: p.append("r12 was not kept (%#x)" % a[12])
            if f != fl or a[15] != RET or a[13] != b[13]: p.append("flags %s, pc %#x or sp changed" % (f, a[15]))
        judge("pw_a (%s): the handler got r4 as its private word, r12 and the others are as they were" % tag, p)
        # cc_a: 0 nothing, 2 carry, 1 an error
        for arg, what in ((0, "0: nothing"), (2, "2 = VENEER_SETCARRY"), (1, "an error")):
            regs = {i: 0x20000 + 0x113 * i for i in range(13)}; regs[0] = arg
            b, a, f, flt, _ = call_veneer(k, V("cc_a"), regs, fl, mode)
            p = []
            if flt: p.append("fault %s" % flt)
            else:
                if a[7] != 77: p.append("the handler did not run")
                if arg == 0: want = fl; skip = (7,)
                elif arg == 2: want = (fl[0], fl[1], 1, 0); skip = (7,)
                else: want = (fl[0], fl[1], fl[2], 1); skip = (0, 7)
                same(b, a, range(13), p, skip=skip)
                if f != want: p.append("flags %s, expected %s" % (f, want))
                if arg == 1:
                    e = errblock(k, a[0])
                    if e is None or e[0] != 0x2001: p.append("r0 = %#x is not the error" % a[0])
                if a[15] != RET or a[13] != b[13]: p.append("pc or sp wrong")
            judge("cc_a (%s), the handler answers %s" % (tag, what), p)
        # both_a: the private word is in r0, r1 = what to answer
        for arg, what in ((0, "0: nothing"), (2, "2 = VENEER_SETCARRY"), (1, "an error")):
            regs = {i: 0x30000 + 0x117 * i for i in range(13)}; regs[0] = mod.pw; regs[1] = arg; regs[12] = 0xC1C1C1C1
            b, a, f, flt, _ = call_veneer(k, V("both_a"), regs, fl, mode)
            p = []
            if flt: p.append("fault %s" % flt)
            else:
                if a[8] != mod.pw: p.append("the handler got %#x, not the private word of r0" % a[8])
                if arg == 0: want = fl
                elif arg == 2: want = (fl[0], fl[1], 1, 0)
                else: want = (fl[0], fl[1], fl[2], 1)
                same(b, a, range(13), p, skip=(8,) if arg != 1 else (0, 8))
                if f != want: p.append("flags %s, expected %s" % (f, want))
                if arg == 1:
                    e = errblock(k, a[0])
                    if e is None or e[0] != 0x2002: p.append("r0 = %#x is not the error" % a[0])
                if a[12] != 0xC1C1C1C1 or a[15] != RET or a[13] != b[13]: p.append("r12, pc or sp wrong")
            judge("both_a (%s), private word in r0, the handler answers %s" % (tag, what), p)
        # plain_a: no carry-capable: so 2 is an error pointer
        regs = {i: 0x40000 + 0x119 * i for i in range(13)}; regs[0] = 2
        b, a, f, flt, _ = call_veneer(k, V("plain_a"), regs, fl, mode)
        p = []
        if flt: p.append("fault %s" % flt)
        elif a[0] != 2 or f != (fl[0], fl[1], fl[2], 1): p.append("r0 %#x flags %s: without carry-capable: the answer 2 is an error pointer (V set, r0 = 2)" % (a[0], f))
        judge("plain_a (%s): without carry-capable: the answer 2 is returned as an error (V set, r0 = 2)" % tag, p)
print()

print("vector handlers: error-capable:")
for fl in FLAGS:
    tag = "NZCV=%d%d%d%d" % fl
    for arg, what in ((0, "claim"), (1, "pass on"), (2, "VECTOR_ERROR (&error)")):
        regs = {i: 0x50000 + 0x11B * i for i in range(13)}; regs[1] = arg; regs[12] = mod.pw
        b, a, f, flt, _ = call_veneer(k, V("vec_e"), regs, fl, 0x13, claim=CLAIM)
        p = []
        if flt: p.append("fault %s" % flt)
        else:
            if a[8] != mod.pw: p.append("the handler did not get the private word")
            want_pc = RET if arg == 1 else CLAIM
            if a[15] != want_pc: p.append("ended at %#x, expected %#x" % (a[15], want_pc))
            want = (fl[0], fl[1], fl[2], 1) if arg == 2 else fl
            if f != want: p.append("flags %s, expected %s" % (f, want))
            if a[13] != b[13] + (0 if arg == 1 else 4): p.append("sp %#x (entered with %#x)" % (a[13], b[13]))
            same(b, a, range(12), p, skip=(8,) if arg != 2 else (0, 8))
            if arg == 2:
                e = errblock(k, a[0])
                if e is None or e[0] != 0x2001: p.append("r0 = %#x is not the error" % a[0])
        judge("vec_e (%s), the handler says %s" % (tag, what), p)
    for arg, what, claimed in ((0, "claim", True), (1, "pass on", False), (2, "2 (not an error here)", False)):
        regs = {i: 0x60000 + 0x11D * i for i in range(13)}; regs[1] = arg; regs[12] = mod.pw
        b, a, f, flt, _ = call_veneer(k, V("vec_n"), regs, fl, 0x13, claim=CLAIM)
        p = []
        if flt: p.append("fault %s" % flt)
        else:
            if a[15] != (CLAIM if claimed else RET): p.append("ended at %#x" % a[15])
            if f != fl: p.append("flags %s, expected %s" % (f, fl))
            same(b, a, range(12), p, skip=(8,))
        judge("vec_n (%s), the handler says %s: %s, flags kept" % (tag, what, "claimed" if claimed else "passed on"), p)
print()

# ================================================================ Vt3
data3, syms3 = build("vt3", "vt3.cmhg", "Vt3.h", ["-DVT3"])
print("built Vt3,ffa: %d bytes" % len(data3))
k, mod = start(data3); cpu = k.cpu
print("the header")
check(mod.title == "Vt3" and mod.chunk == 0x58D40 and mod.swi_names == [] and mod.hdr[10] != 0, "SWI chunk %#x without names, a decoding code word (%#x)" % (mod.chunk, mod.hdr[10]))
cmds = {c[0]: c for c in mod.commands}
check(cmds["V3_Own"][1] != 0 and cmds["V3_Nothing"][1] == 0, "the command table  -  has no handler: V3_Own has code (handler:), V3_Nothing has the code word 0")
r0, v = run_command(k, mod, cmds, "V3_Own")
e = errblock(k, r0) if v else None
check(e is not None and e[0] == 0x2003, "*V3_Own -> error &2003 (its own function, number 0)")
print()
print("swi-decoding-code NAME/NUMBER")
DEC = BASE + mod.hdr[10]
def decode(regs, sp=None):
    cpu.r[13] = k.sp0
    r, v = k.call(DEC, {**regs, 12: mod.pw, 13: sp if sp is not None else k.sp0})
    return r
for name, want in (("Zed", 7), ("Nine", 9), ("Foo", 0xFFFFFFFF), ("Zedd", 0xFFFFFFFF)):
    r = decode({0: 0xFFFFFFFF, 1: k.put_string(name)})
    check(r[0] == want, "name %s -> %#x" % (name, r[0]))
for sp_adj in (0, 4):
    for num, text in ((7, b"Zed"), (9, b"Nine")):
        buf = k.alloc(64)
        for i in range(64): cpu.wr8(buf + i, 0x2E)
        r = decode({0: num, 1: buf, 2: 4, 3: 64, 5: 0x5A5A5A5A}, sp=k.sp0 - sp_adj)
        got = bytes(cpu.rd8(buf + 4 + i) for i in range(len(text) + 2))
        p = []
        if got[:len(text)] != text: p.append("written %r" % got)
        if got[len(text)] != 0: p.append("the handler's sp & 7 was %d (the stack must be 8 byte aligned)" % got[len(text)])
        if got[len(text) + 1] != mod.pw & 0xFF: p.append("the fifth argument (private word) is wrong: %#x" % got[len(text) + 1])
        if r[2] != 4 + len(text) or r[0] != num or r[1] != buf or r[3] != 64 or r[5] != 0x5A5A5A5A: p.append("registers %s" % [hex(x) for x in r[:6]])
        judge("number %d -> %s at offset 4 (sp %s), r2 = %d" % (num, text.decode(), "8 byte aligned" if sp_adj == 0 else "only 4 byte aligned", r[2]), p)
buf = k.alloc(16)
r = decode({0: 5, 1: buf, 2: 3, 3: 16})
check(r[2] == 3, "number 5 is not known: r2 = %d (unchanged)" % r[2])
print()

if fails:
    print("%d FAILED" % fails); sys.exit(1)
print("ALL OK")
