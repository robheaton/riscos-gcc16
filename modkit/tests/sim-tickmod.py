#!/usr/bin/env python3
"""sim-tickmod.py [TICKMOD,FFA] -- TickMod (stage B1: the vector and callback veneers of modkit without sockets) on the A32 interpreter with the kernel model (riscosmodel.py), and TickWait, the
absolute program that the hardware run uses to wait in user mode.  Besides the plain ticks (from SVC and from USR mode) the model lets interrupts arrive in the middle of the module's own SVC mode
code (the busy wait of *TickMod_Delay: a tick right after any SWI) and in the middle of a callback (IRQs are enabled there).
exit status 0 = everything right."""
import os, re, struct, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from riscosmodel import RiscosModel, SWIS, oslibv_swis, TICKERV
from kernelmodel import Module, u32
from a32 import Fault, Cpu, FlatImage

img = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "examples", "tickmod", "TickMod,ffa")
EX = os.path.dirname(os.path.abspath(img))
NM = os.path.expanduser("~/gccsdk-next/env-f/bin/arm-riscos-gnueabihf-nm")
elf = os.path.join(EX, "build", "TickMod.elf")
data = open(img, "rb").read()
syms = {}
for ln in subprocess.run([NM, elf], capture_output=True, text=True, check=True).stdout.split("\n"):
    p = ln.split()
    if len(p) == 3: syms[p[2]] = int(p[0], 16)
fails = 0
def check(ok, what):
    global fails
    print("  %s  %s" % ("ok  " if ok else "FAIL", what))
    if not ok: fails += 1

def fields(out):
    """the numbers of the status output: {name: int or tuple}"""
    d = {}
    m = re.search(r"TickerV claimed: (\w+), (\d+) cs", out); d["claimed"] = m.group(1) if m else None; d["elapsed"] = int(m.group(2)) if m else None
    m = re.search(r"ticks (\d+), callbacks requested (\d+) \(errors (\d+)\), callbacks run (\d+), pending (\d+)", out)
    d["ticks"], d["requested"], d["req_err"], d["callbacks"], d["pending"] = [int(x) for x in m.groups()] if m else [None] * 5
    m = re.search(r"tick handler:\s+(\d+) in SVC mode, (\d+) in other modes, IRQs enabled in (\d+) calls, wrong r12 in (\d+) calls", out)
    d["t_svc"], d["t_other"], d["t_irq_on"], d["t_pw_bad"] = [int(x) for x in m.groups()] if m else [None] * 4
    m = re.search(r"callback handler: (\d+) in SVC mode, (\d+) in other modes, IRQs disabled in (\d+) calls, wrong r12 in (\d+) calls", out)
    d["c_svc"], d["c_other"], d["c_irq_off"], d["c_pw_bad"] = [int(x) for x in m.groups()] if m else [None] * 4
    m = re.search(r"remove test: (\d+) run, callbacks that ran although removed: (\d+)", out)
    d["rt_run"], d["rt_stale"] = [int(x) for x in m.groups()] if m else [None] * 2
    sp = re.findall(r"last CPSR &([0-9A-F]{8}), sp &([0-9A-F]{8}) \.\. &([0-9A-F]{8})", out)
    d["psr_tick"], d["psr_cb"] = (int(sp[0][0], 16), int(sp[1][0], 16)) if len(sp) == 2 else (None, None)
    d["sp"] = [(int(a, 16), int(b, 16)) for _, a, b in sp]
    return d

BASE = 0x01C21000
k = RiscosModel(max_steps=3_000_000)
mod = k.load(data, BASE); cpu = k.cpu; cpu.r[13] = k.sp0
targets = {BASE + syms[n]: n for n in ("module_init", "module_finalise", "command_handler", "tickerv_handler", "callback_handler")}
orig = cpu.step
def step():
    if cpu.r[15] in targets and cpu.r[13] % 8: raise Fault("sp is %#x (not 8-byte aligned) on entry to %s" % (cpu.r[13], targets[cpu.r[15]]))
    orig()
cpu.step = step

print("generator and model agree on the SWIs")
gen = oslibv_swis(os.path.join(EX, "build", "oslibv.c"))
check(all(gen[f] == (SWIS[f] if f in SWIS else 0x42) for f in gen), "SWI numbers of the generated veneers: %s" % {f: hex(n) for f, n in gen.items()})
print()

print("header, init")
check(mod.title == "TickMod" and [c[0] for c in mod.commands] == ["TickMod_Start", "TickMod_Stop", "TickMod_Status", "TickMod_Delay", "TickMod_RemoveTest"], "title %r, commands %s" % (mod.title, [c[0] for c in mod.commands]))
r0, v = k.init(mod)
check(r0 == 0 and v == 0, "init")
print()

print("status before the start")
err, out = k.command(mod, "TickMod_Status")
f = fields(out)
check(err is None and f["claimed"] == "no" and f["ticks"] == 0 and f["callbacks"] == 0, "nothing counted: %r" % out[:90])
print(out.replace("\n", "\n    "))

print("*TickMod_Start, ticks from SVC and USR mode, callbacks")
err, out = k.command(mod, "TickMod_Start")
check(err is None and "claimed" in out and len(k.vectors.get(TICKERV, [])) == 1, "claimed: %r" % out)
err, out = k.command(mod, "TickMod_Start")
check(err is not None and "already started" in k.read_cstr(cpu.r[0] + 4) and len(k.vectors[TICKERV]) == 1, "a second start is refused, still one claim")
k.pump(20)                                                      # 20 ticks, each followed by its callback
err, out = k.command(mod, "TickMod_Status"); f = fields(out)
check(f["ticks"] == 20 and f["requested"] == 20 and f["callbacks"] == 20 and f["pending"] == 0 and f["req_err"] == 0, "20 ticks, 20 requests, 20 callbacks, none pending: %s" % {x: f[x] for x in ("ticks", "requested", "callbacks", "pending")})
check(f["t_svc"] == 20 and f["t_other"] == 0 and f["t_irq_on"] == 0 and f["t_pw_bad"] == 0, "tick handler: SVC mode every time (the veneer switched from IRQ), IRQs off, r12 right: %s" % {x: f[x] for x in ("t_svc", "t_other", "t_irq_on", "t_pw_bad")})
check(f["c_svc"] == 20 and f["c_other"] == 0 and f["c_irq_off"] == 0 and f["c_pw_bad"] == 0, "callback: SVC mode, IRQs on, r12 right: %s" % {x: f[x] for x in ("c_svc", "c_other", "c_irq_off", "c_pw_bad")})
check((f["psr_tick"] & 0x9F) == 0x93 and (f["psr_cb"] & 0x9F) == 0x13, "CPSR in the tick handler &%08X (SVC, I set), in the callback &%08X (SVC, I clear)" % (f["psr_tick"], f["psr_cb"]))
check(f["claimed"] == "yes", "status: claimed")
print()

print("ticks while a callback is pending: one request only")
k.reset = None
k.tick(0x13); k.tick(0x10); k.tick(0x13)
check(len(k.callbacks) == 1, "3 ticks, one callback queued")
k.run_callbacks()
err, out = k.command(mod, "TickMod_Status"); f2 = fields(out)
check(f2["ticks"] == 23 and f2["requested"] == 21 and f2["callbacks"] == 21 and f2["pending"] == 0, "23 ticks, 21 requests, 21 callbacks: %s" % {x: f2[x] for x in ("ticks", "requested", "callbacks", "pending")})
check(f2["c_svc"] == 21, "21 callbacks in SVC mode")
print()

print("*TickMod_Delay: interrupts in the middle of the module's own SVC mode code")
t0 = len(k.tick_log); cb0 = f2["callbacks"]; tk0 = f2["ticks"]
def irq(swi):
    if swi == 0x42 and k.mono % 4 == 0: k.tick(live=True)
k.irq_hook = irq
err, out = k.command(mod, "TickMod_Delay 200")
k.irq_hook = None
nt = len(k.tick_log) - t0
check(err is None and nt >= 40, "the command waited 200 'cs'; %d ticks arrived in the middle of its code (every 4th time read)" % nt)
err, out = k.command(mod, "TickMod_Status"); f3 = fields(out)
check(f3["ticks"] == tk0 + nt and f3["callbacks"] == cb0 and f3["pending"] == 1 and f3["requested"] == f2["requested"] + 1, "the ticks were counted, ONE callback is waiting and has not run (callbacks run when the OS returns to user mode): ticks %d (+%d), callbacks %d, pending %d" % (f3["ticks"], f3["ticks"] - tk0, f3["callbacks"], f3["pending"]))
check(f3["t_svc"] == f2["t_svc"] + nt and f3["t_other"] == 0 and f3["t_irq_on"] == 0 and f3["t_pw_bad"] == 0, "every tick seen in SVC mode with IRQs off")
nb = k.run_callbacks()
err, out = k.command(mod, "TickMod_Status"); f4 = fields(out)
check(nb == 1 and f4["callbacks"] == cb0 + 1 and f4["pending"] == 0, "the callback ran after the command had returned: callbacks %d, pending %d, longest request-to-callback %s ticks" % (f4["callbacks"], f4["pending"], re.search(r"longest (\d+)", out).group(1)))
check(not k.problems, "no complaints of the model about the veneers: %s" % k.problems[:4])
print()

print("interrupts inside a callback (IRQs are on there): the module asks for the next callback while one runs")
def irq2(swi):
    pass
before = fields(k.command(mod, "TickMod_Status")[1])
# a tick in the middle of the callback, after it has cleared its "pending" flag: use the Cpu step hook to deliver it once (a tick before that is right to ask for nothing: one callback is still pending)
state = {"done": False, "n": 0}
cb_entry = BASE + syms["callback_handler"]
orig2 = cpu.step
def step2():
    if k._ctx == "callback" and cb_entry <= cpu.r[15] < cb_entry + 0x200 and not state["done"]:
        state["n"] += 1
        if state["n"] == 30:
            state["done"] = True
            k.tick(live=True)
    orig2()
cpu.step = step2
k.tick(0x13)                                                    # queue a callback
n = k.run_callbacks()
cpu.step = orig2
after = fields(k.command(mod, "TickMod_Status")[1])
check(state["done"] and n == 2, "a tick arrived 30 instructions into callback_handler: that callback and the one it caused both ran (%d ran)" % n)
check(after["ticks"] == before["ticks"] + 2 and after["callbacks"] == before["callbacks"] + 2 and after["pending"] == 0, "counters: ticks %+d, callbacks %+d, pending %d" % (after["ticks"] - before["ticks"], after["callbacks"] - before["callbacks"], after["pending"]))
check(not k.problems, "no complaints: %s" % k.problems[:4])
print()

print("*TickMod_RemoveTest: a callback that is queued and removed must never run")
before = fields(k.command(mod, "TickMod_Status")[1])
err, out = k.command(mod, "TickMod_RemoveTest")
check(err is None and "removed again" in out and not k.callbacks, "queued and removed: %r, nothing left in the kernel's list (%d)" % (out.strip(), len(k.callbacks)))
n = k.run_callbacks()
after = fields(k.command(mod, "TickMod_Status")[1])
check(n == 0 and after["rt_run"] == 1 and after["rt_stale"] == 0 and after["callbacks"] == before["callbacks"], "no callback ran: remove test run %d, stale %d" % (after["rt_run"], after["rt_stale"]))
k.pump(10)                                                      # the ticks go on: the callbacks that they ask for are not 'stale' (0.01 counted them: a flaw of the test that the first run on the machine showed)
after = fields(k.command(mod, "TickMod_Status")[1])
check(after["rt_stale"] == 0 and after["callbacks"] == before["callbacks"] + 10 and after["requested"] == before["requested"] + 10, "10 more ticks and callbacks: stale still %d, callbacks %+d" % (after["rt_stale"], after["callbacks"] - before["callbacks"]))
# the test must be able to FAIL: a callback with the test handle that is NOT taken out does get counted
cb_addr = BASE + syms["callback"]
k.callbacks.append((cb_addr, 0x5E57A1E0)); k.run_callbacks()
after = fields(k.command(mod, "TickMod_Status")[1])
check(after["rt_stale"] == 1 and after["callbacks"] == before["callbacks"] + 10, "a callback with the test handle that was not removed IS counted as stale (the test can fail): stale %d" % after["rt_stale"])
print()

print("the stack")
for ctx in ("tick", "callback"):
    low = k.stack_low.get(ctx); print("      %-9s lowest SVC sp %#x" % (ctx, low or 0))
check(k.sp0 - k.stack_low["callback"] < 4096, "the callback needs less than 4 KB (%d bytes)" % (k.sp0 - k.stack_low["callback"]))
print()

print("*TickMod_Stop and finalisation with a callback pending")
err, out = k.command(mod, "TickMod_Stop")
check(err is None and not k.vectors.get(TICKERV) and "released" in out, "stop: vector released, %r" % out.strip())
k.callbacks.clear(); k.tick(0x13)
check(not k.callbacks, "no tick after the stop")
err, out = k.command(mod, "TickMod_Start")
k.tick(0x10)
check(len(k.callbacks) == 1, "started again, a tick queued a callback")
r0, v = k.final(mod)
check(r0 == 0 and v == 0 and not k.vectors.get(TICKERV) and not k.callbacks, "finalisation: vector released, the pending callback taken out (%d left)" % len(k.callbacks))
check(not k.problems, "no complaints of the model: %s" % k.problems[:4])
print()

print("TickWait: the absolute program that waits in user mode")
TW = os.path.join(HERE, "..", "examples", "tickmod", "TickWait,ff8")
if os.path.exists(TW):
    code = open(TW, "rb").read()
    tel = {"n": 0}
    for arg, expect in (("200", 200), ("37", 37), ("0", 0), ("5", 5)):
        img2 = FlatImage(code, 0x8000)
        c = Cpu(img2, max_steps=500000)
        calls = []
        def hook(cpu, swi):
            n = swi & ~0x20000
            calls.append(n)
            if n == 0x10:                                           # OS_GetEnv
                s = ("<Obey$Dir>.TickWait " + arg).encode() + b"\0"
                for i, b in enumerate(s): cpu.wr8(0x00100000 + i, b)
                cpu.r[0] = 0x00100000; cpu.r[1] = 0x00200000; cpu.r[2] = 0
            elif n == 0x42:                                         # OS_ReadMonotonicTime
                tel["n"] += 1; cpu.r[0] = tel["n"] * 1 + 1000
            elif n == 0x11: raise StopIteration
            else: raise Fault("SWI %#x" % swi)
        c.swi_hook = hook; c.mode = 0x10
        try: c.run(0x8000, 0xFFFF0000)
        except StopIteration: pass
        reads = calls.count(0x42)
        check(calls[-1] == 0x11 and c.r[0] == 0 and c.r[1] == 0x58454241 and c.r[2] == 0 and reads == max(expect, 1) + 1, "TickWait %s: %d time reads (one for the start, then one per centisecond until the time is up: at least one), then OS_Exit with ABEX and return code 0" % (arg, reads))
else:
    print("  (TickWait,ff8 is not built: skipped)")
print()

print("%s" % ("ALL PASS" if not fails else "%d FAILED" % fails))
sys.exit(1 if fails else 0)
