#!/usr/bin/env python3
"""sim-mk32.py -- MK32 (tests/hwpack/mk32.*, the module of pack/module32) on the A32 interpreter with the kernel model: built with the cross compiler (cmunge, gcc -mmodule, modreloc), then

  - the header: SWI chunk and decoding table (prefix MKTest, not the title), the international help file name, the command table flags (international, add-syntax), the start entry (module-is-runnable)
  - *MK32_SelfTest in SVC mode (the SWIs through the model's dispatcher, the generic veneer called with 8 flag states, getenv, malloc, snprintf), the commands and their argument checks
  - the same self test in USER mode: the module run as a program (*RMRun MK32 test), and the argument list (*RMRun MK32 a "b c" d), exit codes

The model does not know OS_CallEvery, so *MK32_After is only run on the machine (the veneer itself is tested in IRQ mode by tests/sim-veneers.py).  TC = the tool chain (default: the work area's tc-dev, else env-f).
Exit status 0 = everything right."""
import os, struct, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE); sys.path.insert(0, os.path.join(HERE, "..", "..", "modpoc")); sys.path.insert(0, os.path.expanduser("~/gccsdk-next/tools"))
from riscosmodel import RiscosModel
from kernelmodel import u32, cstr
from a32 import Fault

TC = os.environ.get("TC") or next(p for p in (os.path.expanduser("~/gccsdk-next/tc-dev/riscos-gcc16-cross-16.2.0-14-x86_64-linux"), os.path.expanduser("~/gccsdk-next/env-f")) if os.path.exists(p))
BIN = os.path.join(TC, "bin")
CC = os.path.join(BIN, "arm-riscos-gnueabihf-gcc"); CMUNGE = os.path.join(BIN, "cmunge"); MODRELOC = os.path.join(BIN, "arm-riscos-gnueabihf-modreloc")
W = tempfile.mkdtemp(prefix="mk32-")
SRC = os.path.join(HERE, "hwpack")
fails = 0
def check(ok, what):
    global fails
    print("  %s  %s" % ("ok  " if ok else "FAIL", what))
    if not ok: fails += 1

def sh(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode: sys.exit("%s\n%s" % (" ".join(cmd), r.stderr))
    return r
def build(out):
    sh([CMUNGE, "-tgcc", "-32bit", "-s", W + "/h.s", "-d", W + "/header.h", SRC + "/mk32.cmhg"])
    sh([CC, "-mmodule", "-c", W + "/h.s", "-o", W + "/h.o"])
    sh([CC, "-mmodule", "-O2", "-std=gnu99", "-Wall", "-Wextra", "-I" + W, "-c", SRC + "/mk32.c", "-o", W + "/mk32.o"])
    sh([CC, "-mmodule", "-c", SRC + "/veneercall.S", "-o", W + "/vc.o"])
    sh([CC, "-mmodule", "-o", W + "/MK32.elf", W + "/mk32.o", W + "/h.o", W + "/vc.o"])
    sh([MODRELOC, W + "/MK32.elf", out])
build(W + "/MK32,ffa")
data = open(W + "/MK32,ffa", "rb").read()
print("built MK32,ffa: %d bytes" % len(data))

BASE = 0x01C21000
class Done(Exception):
    def __init__(self, kind, **kw): self.kind = kind; self.kw = kw

def fresh():
    k = RiscosModel(max_steps=6_000_000)
    mod = k.load(data, BASE)
    k.cpu.r[13] = k.sp0
    k.vars["mk32$path"] = "ADFS::Pi.$.Pack."
    orig = RiscosModel.swi_hook.__get__(k)                                              # (the model calls self.swi_hook again for OS_CallASWI, which is what _swix uses: the hook must be on the object)
    state = {}
    def hook(cpu, swi):
        n = swi & ~0x20000
        if n == 0x10:                                                                   # OS_GetEnv
            cpu.r[0] = k.put_string(state.get("tail", "RMRun MK32")); cpu.r[1] = 0x00800000; cpu.r[2] = k.put_string("\0\0\0\0\0"); cpu.v = 0
        elif n == 0x11: raise Done("exit", code=cpu.r[2], r1=cpu.r[1], mode=cpu.mode)                                 # OS_Exit
        elif n == 0x2B: raise Done("error", text=k.read_cstr(cpu.r[0] + 4))                                           # OS_GenerateError
        elif n == 0x38:                                                                 # OS_SWINumberToString: r0 = number, r1 = buffer, r2 = size -> r2 = bytes used
            num = cpu.r[0]; text = None
            for m in k.modules:
                if m.chunk and m.chunk <= num < m.chunk + 64 and num - m.chunk < len(m.swi_names): text = "%s_%s" % (m.swi_prefix, m.swi_names[num - m.chunk])
            if text is None: cpu.r[0] = k.error_block(0x1E6, "SWI name not known"); cpu.v = 1
            else:
                for i, c in enumerate(text.encode() + b"\0"): cpu.wr8(cpu.r[1] + i, c)
                cpu.r[2] = len(text) + 1; cpu.v = 0
        else: orig(cpu, swi)
    k.swi_hook = hook; k.cpu.swi_hook = hook
    r0, v = k.init(mod)
    assert r0 == 0 and v == 0, (r0, v)
    return k, mod, state

def program(tail):
    """*RMRun MK32 <tail>: the start entry in USER mode"""
    k, mod, state = fresh()
    cpu = k.cpu
    state["tail"] = "RMRun MK32 " + tail
    for i in range(16): cpu.r[i] = 0x55000000 + i
    cpu.r[0] = k.put_string(tail); cpu.r[12] = mod.pw; cpu.r[14] = 0xFFFF0000
    cpu.set_mode(0x10)
    cpu.bank[0x13] = [k.sp0, 0x0B0B0B0B]                                                 # the SVC stack is idle (the veneer of a generic handler may switch to SVC mode)
    mark = len(k.out)
    try:
        cpu.run(mod.base + mod.hdr[0], 0xFFFF0000); res = Done("returned")
    except Done as d: res = d
    except Fault as f: res = Done("fault", text=str(f))
    return res, "".join(k.out[mark:]).replace("\n\r", "\n")

print("the header")
k, mod, st = fresh()
check(mod.title == "MK32" and mod.help.startswith("MK32\t") and mod.help.endswith("0.32 (07 Oct 2026)"), "title %r, help %r" % (mod.title, mod.help))
check(mod.chunk == 0xC0A00 and mod.swi_table and mod.swi_prefix == "MKTest" and mod.swi_names == ["Add", "Sub", "Name", "Fail"], "SWI chunk &%X, prefix %r (not the title), names %s" % (mod.chunk, getattr(mod, "swi_prefix", None), mod.swi_names))
check(mod.hdr[0] != 0, "word 0 is the start offset of the program (%#x)" % mod.hdr[0])
check(mod.hdr[11] != 0 and cstr(data, mod.hdr[11]) == "MK32:Messages", "word 11 is the international help file: %r" % (cstr(data, mod.hdr[11]) if mod.hdr[11] else None))
cmds = {c[0]: c for c in mod.commands}
raw = {}                                                                                     # the info words of the command table
p = mod.hdr[6]
while data[p] != 0:
    nm = cstr(data, p); p += (len(nm) + 1 + 3) & ~3
    raw[nm] = u32(data, p + 4); p += 16
check(sorted(raw) == sorted(["MK32_Plain", "MK32_Tokens", "MK32_Joined", "MK32_Try", "MK32_SelfTest", "MK32_After"]), "the six commands are in the table")
check(raw["MK32_Tokens"] >> 28 & 1 == 1 and raw["MK32_Plain"] >> 28 & 1 == 0 and raw["MK32_Joined"] >> 28 & 1 == 0, "only MK32_Tokens has the international bit (bit 28): %#x %#x %#x" % (raw["MK32_Tokens"], raw["MK32_Plain"], raw["MK32_Joined"]))
c = cmds["MK32_Tokens"]
check(cstr(data, c[5]) == "HELPTOK" and cstr(data, c[4]) == "SYNTOK", "MK32_Tokens: help is the token %r, syntax the token %r" % (cstr(data, c[5]), cstr(data, c[4])))
c = cmds["MK32_Joined"]
check(cstr(data, c[5]).startswith("*MK32_Joined takes up to three words.") and cstr(data, c[5]).endswith("Syntax: *MK32_Joined [<a> [<b> [<c>]]]"), "MK32_Joined (add-syntax): the syntax follows the help text in one string: %r" % cstr(data, c[5]))
print()

print("the commands")
err, out = k.command(mod, "MK32_Plain a b")
check(err is None and out.strip() == "MK32_Plain with 2 arguments", "MK32_Plain a b: %r" % out)
err, out = k.command(mod, "MK32_Plain a b c")
check(err == 0x1E0, "MK32_Plain a b c: the kernel's error (too many arguments): %s" % err)
err, out = k.command(mod, "MK32_Tokens x")
check(err is None and out.strip() == "MK32_Tokens with 1 arguments", "MK32_Tokens x")
err, out = k.command(mod, "MK32_Try MK32_Plain a b c")
check(err is None and out.strip() == "MK32_Try: error &1E0: Syntax: *MK32_Plain [a] [b]", "MK32_Try shows the error of the command: %r" % out)
err, out = k.command(mod, "MK32_Try MK32_Plain a")
check(err is None and out.replace("\n\r", "\n").strip() == "MK32_Plain with 1 arguments\nMK32_Try: no error", "MK32_Try of a good command: %r" % out)
print()

print("*MK32_SelfTest (SVC mode)")
err, out = k.command(mod, "MK32_SelfTest")
for l in out.replace("\n\r", "\n").split("\n"):
    if l.strip(): print("      " + l)
check(err is None and "0 FAILED" in out and out.count("  ok ") >= 14 and "FAIL " not in out.replace("0 FAILED", ""), "no check failed (%d ok)" % out.count("  ok "))
check(not k.problems and not k.log, "no complaints of the model %s" % ((k.problems + k.log)[:3],))
print()

print("*RMRun MK32 test (USER mode)")
res, out = program("test")
for l in out.split("\n"):
    if l.strip(): print("      " + l)
if res.kind != "exit": print("      ended with %s %s" % (res.kind, res.kw))
check(res.kind == "exit" and res.kw["code"] == 0 and res.kw["mode"] == 0x10 and "0 FAILED" in out and out.count("  ok ") >= 14 and "atexit: bye" in out, "OS_Exit 0 in USER mode, no check failed (%d ok), atexit ran" % out.count("  ok "))
print()

print('*RMRun MK32 a "b c" d, exit codes')
res, out = program('a "b c" d')
check(res.kind == "exit" and res.kw["code"] == 0 and out == "MK32 as a program (user mode): argc=4\nargv[0]=<MK32>\nargv[1]=<a>\nargv[2]=<b c>\nargv[3]=<d>\natexit: bye\n", "argv and exit code 0: %r" % out[:90])
res, out = program("exit 3")
check(res.kind == "exit" and res.kw["code"] == 3, "exit 3: OS_Exit with the code 3")
res, out = program("")
check(res.kind == "exit" and res.kw["code"] == 0 and out.startswith("MK32 as a program (user mode): argc=1\n"), "no arguments: argc 1, code 0")

if fails:
    print("%d FAILED" % fails); sys.exit(1)
print("ALL OK")
