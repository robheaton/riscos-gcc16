#!/usr/bin/env python3
"""sim-fpsvc.py -- FpSvc (tests/hwpack/fpsvc.*, the module of pack/module39) on the A32 interpreter with the kernel model: built with the cross compiler (cmunge, gcc -mmodule, modreloc), the answers
of pack/fpcases.py (glibc and Python) built in, then

  - the image: no VFP / NEON instruction and no ARMv7 only instruction (movw movt rbit ubfx sbfx bfi bfc udiv sdiv) in the code of the module, whatever the code is made of
  - *FpSvc_SelfTest, *FpSvc_Math and *FpSvc_Heavy in SVC mode: every check of the module must pass (printf of doubles, strtod, strtof, sscanf, the arithmetic, the conversions)
  - *FpSvc_Calc: the sum is right and the errors are given
  - *FpSvc_Tick: the generic veneer is called in IRQ mode by a fake OS_CallEvery that fires every second call of OS_ReadMonotonicTime; the arithmetic in it is right
  - the model has no complaint (no stack in the wrong place, no unknown SWI)

This takes minutes (about 300 million instructions on the interpreter).  TC = the tool chain (default: the work area's tc-os).  Exit status 0 = everything right."""
import os, re, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from riscosmodel import RiscosModel, IRQ_STACK_TOP
from a32 import Fault
import fpcases

TC = os.environ.get("TC") or next(p for p in (os.path.expanduser("~/gccsdk-next/tc-os"), os.path.expanduser("~/gccsdk-next/env-f")) if os.path.exists(p))
BIN = os.path.join(TC, "bin")
CC = os.path.join(BIN, "arm-riscos-gnueabihf-gcc"); CMUNGE = os.path.join(BIN, "cmunge"); MODRELOC = os.path.join(BIN, "arm-riscos-gnueabihf-modreloc"); OBJDUMP = os.path.join(BIN, "arm-riscos-gnueabihf-objdump")
W = tempfile.mkdtemp(prefix="fpsvc-")
SRC = os.path.join(HERE, "hwpack")
fails = 0
def check(ok, what):
    global fails
    print("  %s  %s" % ("ok  " if ok else "FAIL", what))
    if not ok: fails += 1
def sh(cmd, env=None):
    r = subprocess.run(cmd, capture_output=True, text=True, env=env)
    if r.returncode: sys.exit("%s\n%s%s" % (" ".join(cmd), r.stdout[-1500:], r.stderr[-1500:]))
    return r

open(W + "/fpcases.h", "w").write(fpcases.make_header())
sh([CMUNGE, "-tgcc", "-32bit", "-s", W + "/h.s", "-d", W + "/header.h", SRC + "/fpsvc.cmhg"], env=dict(os.environ, CMUNGE_CC=CC))
sh([CC, "-mmodule", "-c", W + "/h.s", "-o", W + "/h.o"])
sh([CC, "-mmodule", "-O2", "-std=gnu99", "-Wall", "-Wextra", "-I" + W, "-c", SRC + "/fpsvc.c", "-o", W + "/fpsvc.o"])
sh([CC, "-mmodule", "-o", W + "/FpSvc.elf", W + "/fpsvc.o", W + "/h.o", "-Wl,-Map=" + W + "/FpSvc.map"])
sh([MODRELOC, "-q", W + "/FpSvc.elf", W + "/FpSvc,ffa"])
data = open(W + "/FpSvc,ffa", "rb").read()
print("built FpSvc,ffa: %d bytes" % len(data))

print("the image has no VFP, NEON or ARMv7 only instruction (the members of the libraries that were linked, and the module's own objects, one by one)")
mapt = open(W + "/FpSvc.map").read()
pulled = sorted(set(re.findall(r"/([\w\-]+\.a)\(([^)]+)\)", mapt)))
os.makedirs(W + "/x", exist_ok=True)
for lib, m in pulled:
    subprocess.run([os.path.join(BIN, "arm-riscos-gnueabihf-ar"), "x", os.path.join(TC, "arm-riscos-gnueabihf", "lib", lib), m], cwd=W + "/x", check=True)
bad = []
for f in [W + "/x/" + m for _, m in pulled] + [W + "/fpsvc.o", W + "/h.o"]:
    dis = subprocess.run([OBJDUMP, "-d", "-m", "armv8-a", f], capture_output=True, text=True).stdout
    for line in dis.split("\n"):
        parts = line.split("\t")
        if len(parts) >= 3 and parts[2].split() and re.match(r"(v[a-z]|movw|movt|rbit|ubfx|sbfx|bfi|bfc|udiv|sdiv|dmb|dsb|isb)", parts[2].split()[0]): bad.append((os.path.basename(f), parts[2].strip()))
check(not bad and len(pulled) > 10, "%d library members and the module's own objects have none%s" % (len(pulled), "" if not bad else ": " + str(bad[:3])))
print()

BASE = 0x01C21000
k = RiscosModel(max_steps=6_000_000_000)
mod = k.load(data, BASE)
k.cpu.r[13] = k.sp0
state = {"ev": None, "irq_calls": 0, "irq_bad": []}
orig = k.swi_hook
def deliver(cpu, routine, handle):
    """the generic veneer called the way the kernel calls an OS_CallEvery routine: IRQ mode, interrupts off, r12 = the handle"""
    s = k._snapshot()
    saved_steps = cpu.steps
    RET = 0xFFFF0600
    live = (list(cpu.r), cpu.mode, cpu.cpsr_ctl, (cpu.n, cpu.z, cpu.c, cpu.v))
    cpu.set_mode(0x12); cpu.cpsr_ctl = 0x92
    cpu.r[13] = IRQ_STACK_TOP; cpu.r[14] = RET
    for i in range(12): cpu.r[i] = 0xB0000000 + i
    cpu.r[12] = handle
    cpu.run(routine, RET)
    if cpu.mode != 0x12: state["irq_bad"].append("returned in mode %#x" % cpu.mode)
    if cpu.r[13] != IRQ_STACK_TOP: state["irq_bad"].append("sp %#x on return" % cpu.r[13])
    state["irq_calls"] += 1
    k._restore(s)
    cpu.steps = saved_steps
def hook(cpu, swi):
    n = swi & ~0x20000
    if n == 0x3C: state["ev"] = (cpu.r[1], cpu.r[2]); cpu.v = 0; return                         # OS_CallEvery
    if n == 0x3D: state["ev"] = None; cpu.v = 0; return                                         # OS_RemoveTickerEvent
    if n == 0x42:                                                                               # OS_ReadMonotonicTime: one cs per call; an event every second call
        orig(cpu, swi)
        if state["ev"] and k.mono % 2 == 0: deliver(cpu, *state["ev"])
        return
    orig(cpu, swi)
k.swi_hook = hook; k.cpu.swi_hook = hook
r0, v = k.init(mod)
check(r0 == 0 and v == 0, "initialisation returns no error")
cmds = {c[0] for c in mod.commands}
check(cmds == {"FpSvc_SelfTest", "FpSvc_Heavy", "FpSvc_Tick", "FpSvc_Calc", "FpSvc_Math"}, "the five commands are in the table: %s" % sorted(cmds))
print()

def run(cmd):
    err, out = k.command(mod, cmd)
    out = out.replace("\n\r", "\n")
    return err, out

print("*FpSvc_Calc")
err, out = run("FpSvc_Calc 0.1 + 0.2")
check(err is None and out.startswith("0.30000000000000004 = 0x1.3333333333334p-2") and "bits 3FD3333333333334" in out, "0.1 + 0.2 = %r" % out.strip())
err, out = run("FpSvc_Calc 1 / 3 * 3 - 1")
check(err is None and out.startswith("0 = 0x0p+0"), "1 / 3 * 3 - 1 = %r" % out.strip()[:60])
err, out = run("FpSvc_Calc 1e308 * 10")
check(err is None and out.startswith("inf = inf"), "overflow gives inf: %r" % out.strip()[:50])
err, out = run("FpSvc_Calc 2 ^ 3")
check(err is not None, "a bad operator is an error (%s)" % (err,))
err, out = run("FpSvc_Calc x")
check(err is not None, "a bad number is an error (%s)" % (err,))
print()

print("*FpSvc_Tick (a fake OS_CallEvery: the generic veneer in IRQ mode)")
err, out = run("FpSvc_Tick")
print("      " + out.strip())
check(err is None and out.strip().endswith(": ok") and state["irq_calls"] >= 100 and not state["irq_bad"], "100 calls of the veneer (%d delivered), the arithmetic in them is right, %s" % (state["irq_calls"], state["irq_bad"][:2] or "IRQ mode and the stack are as they were"))
print()

print("*FpSvc_SelfTest (SVC mode)")
err, out = run("FpSvc_SelfTest")
for l in out.split("\n"):
    if l.strip(): print("      " + l[:200])
check(err is None and re.search(r"self test \(SVC mode\): \d+ checks, 0 FAILED", out) is not None, "no check failed")
print()

print("*FpSvc_Math")
err, out = run("FpSvc_Math")
for l in out.split("\n"):
    if l.strip(): print("      " + l[:200])
check(err is None and re.search(r"FpSvc_Math: \d+ checks, 0 FAILED", out) is not None, "no check failed")
print()

print("*FpSvc_Heavy")
err, out = run("FpSvc_Heavy")
for l in out.split("\n"):
    if l.strip(): print("      " + l[:200])
check(err is None and re.search(r"FpSvc_Heavy: \d+ checks, 0 FAILED", out) is not None, "no check failed")
print()
check(not k.problems and not k.log, "no complaints of the model %s" % ((k.problems + k.log)[:3],))
if fails:
    print("%d FAILED" % fails); sys.exit(1)
print("ALL OK")
