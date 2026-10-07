#!/usr/bin/env python3
"""sim-veneers.py -- what cmunge makes of a CMHG file with generic veneers, a SWI decoding table whose prefix is not the title and service numbers that are not ARM immediates (tests/veneers/vt.*),
on the A32 interpreter with the kernel model: the module is built with the cross compiler (cmunge, gcc -mmodule, modreloc), loaded and initialised, then

  - the header: the SWI chunk, the prefix and the names of the decoding table (OS_SWINumberFromString finds VT_Beta and not VeneerTest_Beta)
  - the SWI handler: VT_Alpha changes registers, VT_Beta gives an error (V set, r0 = the block, the registers the handler changed come back)
  - the service handler: every number of the list reaches the handler (the chain of compares that builds the numbers that no immediate can hold), no other number does
  - a generic veneer called in SVC, IRQ and USR mode with the flags in every state: handler returns 0 -> the registers the handler left in the block, the flags, the mode and the stacks as they were;
    handler returns an error -> V set and r0 = the error, the other registers as the handler left them; the handler gets the registers as a block, the private word and an 8-byte aligned stack.

TC = the tool chain (default: the work area's tc-dev, else env-f).  Exit status 0 = everything right."""
import os, struct, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE); sys.path.insert(0, os.path.join(HERE, "..", "..", "modpoc")); sys.path.insert(0, os.path.expanduser("~/gccsdk-next/tools"))
from riscosmodel import RiscosModel, USR_SP, IRQ_STACK_TOP
from a32 import Fault

TC = os.environ.get("TC") or next(p for p in (os.path.expanduser("~/gccsdk-next/tc-dev/riscos-gcc16-cross-16.2.0-14-x86_64-linux"), os.path.expanduser("~/gccsdk-next/env-f")) if os.path.exists(p))
BIN = os.path.join(TC, "bin")
CC = os.path.join(BIN, "arm-riscos-gnueabihf-gcc"); NM = os.path.join(BIN, "arm-riscos-gnueabihf-nm")
CMUNGE = os.path.join(BIN, "cmunge"); MODRELOC = os.path.join(BIN, "arm-riscos-gnueabihf-modreloc")
W = tempfile.mkdtemp(prefix="veneers-")
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
sh([CMUNGE, "-tgcc", "-32bit", "-s", W + "/h.s", "-d", W + "/VeneerTest.h", SRC + "/vt.cmhg"])
MUT = os.environ.get("MUTATE")                                       # "old=>new": a change in the generated assembler source, to see that the test notices it (mutate-veneers.sh)
if MUT:
    src = open(W + "/h.s").read(); o, n = MUT.split("=>")
    if o not in src: sys.exit("MUTATE: %r is not in the generated source" % o)
    open(W + "/h.s", "w").write(src.replace(o, n, 1))
sh([CC, "-mmodule", "-c", W + "/h.s", "-o", W + "/h.o"])
sh([CC, "-mmodule", "-O2", "-Wall", "-I" + W, "-c", SRC + "/vt.c", "-o", W + "/vt.o"])
sh([CC, "-mmodule", "-o", W + "/VT.elf", W + "/vt.o", W + "/h.o"])
sh([MODRELOC, W + "/VT.elf", W + "/VT,ffa"])
data = open(W + "/VT,ffa", "rb").read()
syms = {}
for ln in sh([NM, W + "/VT.elf"]).stdout.split("\n"):
    p = ln.split()
    if len(p) == 3: syms[p[2]] = int(p[0], 16)
print("built VT,ffa: %d bytes" % len(data))

BASE = 0x01C21000
RET = 0xFFFF0900
k = RiscosModel(max_steps=2_000_000)
mod = k.load(data, BASE); cpu = k.cpu; cpu.r[13] = k.sp0
r0, v = k.init(mod)
assert r0 == 0 and v == 0

print("the header")
check(mod.title == "VeneerTest" and mod.chunk == 0x58C80, "title %r, SWI chunk %#x" % (mod.title, mod.chunk))
check(mod.swi_table and mod.swi_prefix == "VT" and mod.swi_names == ["Alpha", "Beta", "Gamma"], "SWI prefix %r (not the title), names %s" % (getattr(mod, "swi_prefix", None), mod.swi_names))
cpu.r[13] = k.sp0
regs, v = k.swi(0x39, {1: k.put_string("VT_Beta")})
check(v == 0 and regs[0] == 0x58C81, "OS_SWINumberFromString VT_Beta = %#x" % regs[0])
regs, v = k.swi(0x39, {1: k.put_string("VeneerTest_Beta")})
check(v == 1, "OS_SWINumberFromString VeneerTest_Beta is not known (the prefix is VT)")
print()

print("the SWI handler")
regs, v = k.swi(0x58C80, {0: 7, 1: 3, 4: 0x44})
check(v == 0 and regs[0] == 10 and regs[1] == 4 and regs[2] == 0 and regs[4] == 0x44, "VT_Alpha 7 3 -> r0 %d r1 %d r2 %d (r4 kept %#x)" % (regs[0], regs[1], regs[2], regs[4]))
regs, v = k.swi(0x58C81, {0: 1, 1: 2})
err = struct.unpack("<I", bytes(cpu.rd8(regs[0] + i) for i in range(4)))[0]
check(v == 1 and err == 0x1235 and k.read_cstr(regs[0] + 4) == "VeneerTest SWI error" and regs[3] == 33, "VT_Beta: V set, r0 -> error &%X %r, r3 = %d (the handler's change comes back)" % (err, k.read_cstr(regs[0] + 4), regs[3]))
regs, v = k.swi(0x58C82, {0: 5, 1: 6, 2: 7})
check(v == 0 and regs[:3] == [5, 6, 7], "VT_Gamma changes nothing")
print()

print("the service handler: the numbers of the list, one by one, and the ones next to them")
NUMS = [0x44ec1, 0x81040, 0x400c3, 0x1000001, 0x12345678, 0xFFFFFFFF, 0x27]
for n in NUMS + [0x44ec2, 0x44ec0, 0x81041, 0x400c2, 0x1000002, 0x1000000, 0x12345679, 0x12345677, 0xFFFFFFFE, 0x26, 0x28, 0, 1, 0x100, 0x80000000]:
    res = k.service(mod, n, {})
    ran = res[0] == (0xA0000000 + n) & 0xFFFFFFFF
    if n in NUMS:
        check(ran and res[1] == (0 if n == 0x27 else n), "service %#x reaches the handler (%s)%s" % (n, "r0 %#x" % res[0], ", claimed (r1 = 0)" if n == 0x27 else ", r1 kept"))
    else:
        check(not ran and res[0] == 0 and res[1] == n, "service %#x is not for the module: the handler did not run (r0 %#x r1 %#x)" % (n, res[0], res[1]))
print()

def call_veneer(addr, mode, flags, regs, sp=None):
    """the generic veneer called in MODE with the FLAGS (n, z, c, v) and the registers REGS as the caller had them; returns what is there when it comes back, and what the caller must find unchanged"""
    s = k._snapshot()
    cpu.steps = 0
    cpu.set_mode(0x13); cpu.r[13], cpu.r[14] = k.sp0 - 0x200, 0x0B0B0B0B                       # the SVC registers: live (SVC) or banked
    ctl = {0x13: 0x13, 0x12: 0x92, 0x10: 0x10}[mode]
    if mode != 0x13:
        cpu.set_mode(mode)
    cpu.cpsr_ctl = ctl
    cpu.r[13] = sp if sp is not None else {0x13: k.sp0 - 0x300, 0x12: IRQ_STACK_TOP, 0x10: USR_SP}[mode]
    cpu.r[14] = RET
    for i in range(12): cpu.r[i] = regs.get(i, 0x70000000 + i)
    cpu.r[12] = mod.pw
    cpu.n, cpu.z, cpu.c, cpu.v = flags
    before = dict(r=list(cpu.r), flags=flags, mode=cpu.mode, ctl=cpu.cpsr_ctl, bank={m: list(x) for m, x in cpu.bank.items()})
    fault = None
    try:
        cpu.run(addr, RET)
    except Fault as f:
        fault = str(f)
    after = dict(r=list(cpu.r), flags=(cpu.n, cpu.z, cpu.c, cpu.v), mode=cpu.mode, ctl=cpu.cpsr_ctl, bank={m: list(x) for m, x in cpu.bank.items()})
    k._restore(s)
    return before, after, fault

A = BASE + syms["gv_a"]; B = BASE + syms["gv_b"]
NAMES = {0x13: "SVC", 0x12: "IRQ", 0x10: "USR"}
FLAGS = [(0, 0, 0, 0), (1, 0, 1, 0), (0, 1, 0, 0), (1, 1, 1, 0), (0, 0, 1, 1), (1, 0, 0, 1)]
STACK = {0x13: 0x1ffd00 - 0, 0x12: IRQ_STACK_TOP, 0x10: USR_SP}
def judge(label, problems):
    check(not problems, label if not problems else "%s: %s" % (label, "; ".join(problems)))

for mode in (0x13, 0x12, 0x10):
    print("generic veneer called in %s mode: 6 flag states x the stack 8 byte aligned or only 4 byte aligned (the kernel does not promise more)" % NAMES[mode])
    for adj in (0, 4):
        sp = STACK[mode] - adj
        ok0 = ok1 = 0
        for fl in FLAGS:
            tag = "NZCV=%d%d%d%d, sp %#x" % (fl + (sp,))
            problems = []
            regs = {i: 0x10000 + 0x111 * i for i in range(12)}; regs[0] = 0                       # handler returns 0: r1 = r1*2+r2, r5 = pw, r6 = sp & 7, r7 = the block
            b, a, f = call_veneer(A, mode, fl, regs, sp)
            if f: problems.append("fault %s" % f)
            else:
                exp1 = (b["r"][1] * 2 + b["r"][2]) & 0xFFFFFFFF
                for i in (0, 2, 3, 4, 8, 9, 10, 11):
                    if a["r"][i] != b["r"][i]: problems.append("r%d changed" % i)
                if a["r"][1] != exp1: problems.append("r1 is %#x, not %#x" % (a["r"][1], exp1))
                if a["r"][5] != mod.pw: problems.append("the handler did not get the private word")
                if a["r"][6] != 0: problems.append("the handler's sp & 7 was %d" % a["r"][6])
                if a["r"][7] != b["r"][13] - 52: problems.append("the block is not the 13 stacked words (%#x, sp %#x)" % (a["r"][7], b["r"][13]))
                for name, same in (("flags", a["flags"] == fl), ("mode", a["mode"] == b["mode"]), ("control bits", a["ctl"] == b["ctl"]), ("sp", a["r"][13] == b["r"][13]), ("lr", a["r"][14] == b["r"][14]), ("return address", a["r"][15] == RET),
                                   ("banked registers", all(a["bank"][m] == b["bank"][m] for m in b["bank"] if m != b["mode"]))):
                    if not same: problems.append("%s changed" % name)
            judge("returns 0 (%s): the block's registers as the handler left them, the rest as it was" % tag, problems)
            problems = []
            regs = {i: 0x20000 + 0x113 * i for i in range(12)}; regs[0] = 1                       # handler returns an error
            b, a, f = call_veneer(A, mode, fl, regs, sp)
            if f: problems.append("fault %s" % f)
            else:
                errnum = struct.unpack("<I", bytes(cpu.rd8(a["r"][0] + i) for i in range(4)))[0] if 0 < a["r"][0] < 0x10000000 else None
                if errnum != 0x1234 or k.read_cstr(a["r"][0] + 4) != "VeneerTest generic error": problems.append("r0 = %#x is not the error" % a["r"][0])
                if a["flags"] != fl[:3] + (1,): problems.append("flags %s, expected V set and NZC as they were" % (a["flags"],))
                if a["r"][1] != (b["r"][1] * 2 + b["r"][2]) & 0xFFFFFFFF or a["r"][5] != mod.pw: problems.append("the handler's changes did not come back")
                for i in (2, 3, 4, 8, 9, 10, 11):
                    if a["r"][i] != b["r"][i]: problems.append("r%d changed" % i)
                if a["mode"] != b["mode"] or a["ctl"] != b["ctl"] or a["r"][13] != b["r"][13] or a["r"][15] != RET: problems.append("mode, control bits, sp or return address changed")
            judge("returns an error (%s): V set, NZC kept, r0 = the error, the handler's changes come back" % tag, problems)
        b, a, f = call_veneer(A, mode, (1, 0, 1, 0), {0: 2, 1: 0x55, 2: 0x66}, sp)
        judge("handler changes nothing (sp %#x): every register and the flags as they were" % sp, [] if f is None and all(a["r"][i] == b["r"][i] for i in range(15)) and a["r"][15] == RET and a["flags"] == (1, 0, 1, 0) else ["something changed"])
    b, a, f = call_veneer(B, mode, (0, 1, 0, 0), {})
    judge("the second veneer gv_b calls gv_b_handler (the default name): r9 = pw + 1, flags kept", [] if f is None and a["r"][9] == mod.pw + 1 and a["flags"] == (0, 1, 0, 0) and a["r"][15] == RET else ["r9 %#x flags %s" % (a["r"][9], a["flags"])])
    print()

if fails:
    print("%d FAILED" % fails); sys.exit(1)
print("ALL OK")
