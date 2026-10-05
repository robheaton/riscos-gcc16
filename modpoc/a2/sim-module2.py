#!/usr/bin/env python3
"""sim-module2.py [HELLO2,FFA [HELLO2.ELF]] -- stage 2 of the module work on the A32 interpreter with the kernel model (../kernelmodel.py): HelloMod2 at three load addresses (SVC sp 8-byte aligned or not):
header (SWI chunk and table, command, service entry), init, *HelloMod2_Test through OS_CLI (17 checks that the module makes itself: data, bss, pointer tables, a switch, libc, its own SWIs through the
kernel, a service call), the second run, *HelloMod2_Dyn from outside (Service_UKCommand claimed), an unknown command (not claimed: 'Bad command', registers kept), too many parameters, the SWIs from outside
(Add, Op, Count, errors, a number the module does not have), a service call that is not ours (registers untouched), RMReInit (no second relocation, the state starts again), finalisation.
exit status 0 = everything right."""
import os, re, struct, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
sys.path.insert(0, os.path.expanduser("~/gccsdk-next/tools"))
from kernelmodel import Kernel, Module, u32, cstr, SERVICE_UKCOMMAND
import riscos_consts as rc
from a32 import Fault

img = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "hello2,ffa")
elf = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "hello2.elf")
NM = os.path.expanduser("~/gccsdk-next/env-f/bin/arm-riscos-gnueabihf-nm")
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

print("header")
m0 = Module(data, 0)
check(m0.title == "HelloMod2" and m0.help.startswith("HelloMod2\t0.01 "), "title %r, help %r" % (m0.title, m0.help))
check(m0.chunk == 0x5FC40 and m0.chunk % 64 == 0, "SWI chunk base %#x (a multiple of 64)" % m0.chunk)
check(m0.swi_prefix == "HelloMod2" and m0.swi_names == ["Add", "Op", "Count"], "SWI decoding table: prefix %r, names %s" % (m0.swi_prefix, m0.swi_names))
check([c[0] for c in m0.commands] == ["HelloMod2_Test"] and m0.commands[0][2] == 0 and m0.commands[0][3] == 0, "command table: HelloMod2_Test with no parameters")
check(m0.service == syms["service"] and m0.swi_handler == syms["swi_entry"] and m0.init == syms["init"] and m0.final == syms["final"], "init, final, service and SWI handler offsets point at the veneers")
check(m0.flags & 1 == 1, "module flags: 32-bit compatible")
tab_off, n = u32(data, syms["reloc_info"]), u32(data, syms["reloc_info"] + 4)
table = [u32(data, tab_off + 4 * i) for i in range(n)]
check(tab_off + 4 * n == len(data) and n > 0 and all(0 <= t < tab_off and t % 4 == 0 for t in table), "relocation table: %d words at the end of the image" % n)
print()

TARGETS = {"hello2_init": 0, "hello2_final": 0, "hello2_cmd": 0, "hello2_swi": 0, "hello2_ukcommand": 0}
def run(base, sp0):
    k = Kernel(sp=sp0)
    mod = k.load(data, base)
    k.cpu.r[13] = sp0
    tg = {base + syms[n]: n for n in TARGETS}
    orig = k.cpu.step
    def step():
        if k.cpu.r[15] in tg and k.cpu.r[13] % 8: raise Fault("sp is %#x (not 8-byte aligned) on entry to %s" % (k.cpu.r[13], tg[k.cpu.r[15]]))
        orig()
    k.cpu.step = step
    r = {}
    r0, v = k.init(mod); r["init"] = (r0, v)
    cpu = k.cpu
    r["patched"] = [t for t in table if cpu.rd32(base + t) != (u32(data, t) + base) & 0xFFFFFFFF]
    r["ws"] = cpu.rd32(mod.pw)
    err, out = k.command(mod, "HelloMod2_Test"); r["test1"] = (err, out)
    err, out = k.command(mod, "*helloMOD2_test"); r["test2"] = (err, out)
    err, out = k.command(mod, "HelloMod2_Dyn  one two"); r["dyn"] = (err, out)
    k.log.clear()
    err, out = k.command(mod, "NoSuchCommand foo"); r["unk"] = (err, out, list(k.log))
    err, out = k.command(mod, "HelloMod2_Test extra"); r["toomany"] = (err, out)
    # SWIs from outside, through the kernel's dispatcher
    marks = {2: 0x2222, 3: 0x3333, 4: 0x4444, 5: 0x5555, 6: 0x6666, 7: 0x7777, 8: 0x8888, 9: 0x9999}
    regs, v = k.swi(0x20000 | mod.chunk, {**marks, 0: 1000, 1: 234}); r["add"] = (regs, v)
    regs, v = k.swi(0x20000 | (mod.chunk + 1), {**marks, 0: 0, 1: 12}); r["op"] = (regs, v)
    regs, v = k.swi(0x20000 | (mod.chunk + 2), dict(marks)); r["count"] = (regs, v)
    regs, v = k.swi(0x20000 | mod.chunk, {**marks, 0: 0x7FFFFFFF, 1: 1}); r["overflow"] = (regs, v, cpu.rd32(regs[0]) if v else None)
    regs, v = k.swi(0x20000 | (mod.chunk + 63), dict(marks)); r["badswi"] = (regs, v, cpu.rd32(regs[0]) if v else None)
    # a service call that is not ours: every register the same on return
    before = {0: 0xA0, 2: 0xA2, 3: 0xA3, 4: 0xA4, 5: 0xA5, 6: 0xA6, 7: 0xA7, 8: 0xA8, 9: 0xA9, 10: 0xAA}
    after = k.service(mod, 0x10, before); r["svc_other"] = [(i, before[i], after[i]) for i in before if after[i] != before[i]] + ([("r1", 0x10, after[1])] if after[1] != 0x10 else [])
    # a UKCommand that is not ours: not claimed, registers kept
    cp = k.put_string("Something_else arg")
    after = k.service(mod, SERVICE_UKCOMMAND, {0: cp, 2: 0xB2, 3: 0xB3, 4: 0xB4}); r["svc_uk_not_ours"] = (after[0] == cp and after[1] == SERVICE_UKCOMMAND and after[2] == 0xB2 and after[3] == 0xB3 and after[4] == 0xB4)
    after = k.service(mod, rc.get("Service_International"), {0: 3, 2: 0xC2, 3: 0xC3}); r["svc_intl"] = (after[0] == 3 and after[1] == rc.get("Service_International") and after[2] == 0xC2 and after[3] == 0xC3)
    # RMReInit: finalise, initialise the same image again
    r["final"] = k.final(mod); r["freed"] = list(k.freed); r["pw_after_final"] = cpu.rd32(mod.pw)
    nsync = len(k.sync)
    r["reinit"] = k.init(mod); r["nsync_after_reinit"] = (nsync, len(k.sync))
    err, out = k.command(mod, "HelloMod2_Test"); r["test3"] = (err, out)
    r["kernel"] = k; r["mod"] = mod
    return r

OK_LINES = 17
outs = []
for base, sp0 in ((0x01D40000, 0x00200000), (0x01E7C010, 0x00200000 - 4), (0x02001230, 0x00200000 - 12)):
    print("loaded at %#x, sp on entry %#x (sp %% 8 = %d)" % (base, sp0, sp0 % 8))
    try:
        r = run(base, sp0)
    except Fault as e:
        check(False, "the run faulted: %s" % e); continue
    k = r["kernel"]; mod = r["mod"]
    check(r["init"] == (0, 0), "init: r0 = 0, V clear")
    check(not r["patched"], "relocation: all %d table words are the link-time value + the load address" % len(table) + ("" if not r["patched"] else " (wrong: %s)" % r["patched"][:5]))
    check(len(k.sync) >= 1 and k.sync[0] == (1, base, base + tab_off - 1), "OS_SynchroniseCodeAreas for the whole image")
    t1 = r["test1"][1]
    lines = t1.split("\n")
    oks = [l for l in lines if l.startswith("  ok    ")]; bad = [l for l in lines if l.startswith("  FAIL  ")]
    check(r["test1"][0] is None and len(oks) == OK_LINES and not bad and lines[0].startswith("HelloMod2_Test, run 1, image at &%X" % base) and "%d checks, 0 failed: all ok" % OK_LINES in t1,
          "*HelloMod2_Test: %d checks ok, none failed%s" % (len(oks), "" if not bad else " FAILED: " + "; ".join(b.strip() for b in bad)))
    # the dynamic command was claimed twice: once in the test (OS_CLI inside the module) and once from outside
    check("HelloMod2: *HelloMod2_Dyn was claimed from Service_UKCommand; the rest of the line is 'with some words'" in t1, "inside the test: OS_CLI -> Service_UKCommand -> the module claims *HelloMod2_Dyn")
    check(r["test2"][0] is None and r["test2"][1].startswith("HelloMod2_Test, run 2,") and "%d checks, 0 failed: all ok" % OK_LINES in r["test2"][1], "the second run (command name in another case, with a *): run 2, all ok")
    check(r["dyn"][0] is None and r["dyn"][1] == "HelloMod2: *HelloMod2_Dyn was claimed from Service_UKCommand; the rest of the line is 'one two'\n", "*HelloMod2_Dyn one two from outside: claimed, output %r" % r["dyn"][1])
    check(r["unk"][0] == 0xFE and r["unk"][1] == "" and not r["unk"][2], "an unknown command: Service_UKCommand (&%02X) is not claimed ('Bad command'), r0, r1 and r2-r10 unchanged %s" % (SERVICE_UKCOMMAND, r["unk"][2] or ""))
    check(r["toomany"][0] == 0x1E0 and r["toomany"][1] == "", "*HelloMod2_Test extra: the kernel refuses (too many parameters), the module is not entered")
    regs, v = r["add"]; check(v == 0 and regs[0] == 1234 and all(regs[i] == 0x1111 * i for i in range(2, 10)) and regs[1] == 234, "SWI HelloMod2_Add (1000, 234) = 1234, no error; r1 - r9 as they were")
    regs, v = r["op"]; check(v == 0 and regs[0] == 144 and regs[1] == 12, "SWI HelloMod2_Op (0, 12) = 144 (square)")
    regs, v = r["count"]; check(v == 0 and regs[0] == 2, "SWI HelloMod2_Count = 2 (two runs of the test so far)")
    regs, v, num = r["overflow"]; check(v == 1 and num == 0x1B3C1 and all(regs[i] == 0x1111 * i for i in range(2, 10)), "SWI HelloMod2_Add overflow: V set, r0 = the error block (number %s), r2 - r9 as they were" % (hex(num) if num is not None else None))
    regs, v, num = r["badswi"]; check(v == 1 and num == 0x1E6, "SWI chunk + 63 (not in the module's table): V set, error &1E6")
    check(r["svc_other"] == [], "a service call that is not Service_UKCommand: every register as it was%s" % ("" if not r["svc_other"] else " (changed: %s)" % r["svc_other"]))
    check(r["svc_uk_not_ours"], "Service_UKCommand for another command: not claimed (r1 = &43), r0, r2 - r4 as they were")
    check(r["svc_intl"], "Service_International (&43, the number the first version wrongly used for UKCommand) with r0 = 3: passed on, registers as they were")
    check(r["final"] == (0, 0) and r["freed"] == [r["ws"]] and r["pw_after_final"] == 0, "final: workspace freed, private word cleared, V clear")
    check(r["nsync_after_reinit"][0] == r["nsync_after_reinit"][1] and r["reinit"] == (0, 0), "RMReInit of the relocated image: no second relocation (OS_SynchroniseCodeAreas not called again)")
    check(r["test3"][0] is None and r["test3"][1].startswith("HelloMod2_Test, run 1,") and "%d checks, 0 failed: all ok" % OK_LINES in r["test3"][1], "after RMReInit: all checks ok again, and the run count starts at 1 (the module resets its own static state)")
    outs.append(r["test1"][1])
print()
check(len(outs) == 3 and len(set(outs)) == 3 and all(("%d checks, 0 failed: all ok" % OK_LINES) in o for o in outs), "all three load addresses: every check ok (the texts differ only in the image address)")
print("RESULT: %d check(s) failed" % fails)
sys.exit(1 if fails else 0)
