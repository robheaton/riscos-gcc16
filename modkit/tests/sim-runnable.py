#!/usr/bin/env python3
"""sim-runnable.py -- a module-is-runnable module (tests/runnable/runme.*) on the A32 interpreter with the kernel model: built with the cross compiler (gcc -mmodule, cmunge), loaded, initialised, and then
STARTED the way *RMRun does it: the start entry of the header is called in USER mode with r0 = the command tail and r12 = the private word, and OS_GetEnv gives the top of the memory.  What the program does
(argc / argv, atexit, exit, return, abort) is read from its output and from the OS_Exit it ends with; the module commands are run in SVC mode, where exit () must be an error (OS_GenerateError).
TC = the tool chain (default: the work area's tc-dev, else env-f).  Exit status 0 = everything right."""
import os, struct, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE); sys.path.insert(0, os.path.join(HERE, "..", "..", "modpoc")); sys.path.insert(0, os.path.expanduser("~/gccsdk-next/tools"))
from riscosmodel import RiscosModel
from a32 import Fault

TC = os.environ.get("TC") or next(p for p in (os.path.expanduser("~/gccsdk-next/tc-dev/riscos-gcc16-cross-16.2.0-14-x86_64-linux"), os.path.expanduser("~/gccsdk-next/env-f")) if os.path.exists(p))
CC = os.path.join(TC, "bin", "arm-riscos-gnueabihf-gcc"); CMUNGE = os.path.join(TC, "bin", "cmunge")
W = tempfile.mkdtemp(prefix="runnable-")
SRC = os.path.join(HERE, "runnable")
fails = 0
def check(ok, what):
    global fails
    print("  %s  %s" % ("ok  " if ok else "FAIL", what))
    if not ok: fails += 1

def sh(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode: sys.exit("%s\n%s" % (" ".join(cmd), r.stderr))
    return r
sh([CMUNGE, "-tgcc", "-32bit", "-s", W + "/h.s", "-d", W + "/header.h", SRC + "/runme.cmhg"])
sh([CC, "-mmodule", "-c", W + "/h.s", "-o", W + "/h.o"])
sh([CC, "-mmodule", "-O2", "-Wall", "-I" + W, "-c", SRC + "/runme.c", "-o", W + "/runme.o"])
sh([CC, "-mmodule", "-o", W + "/RunMe,ffa", W + "/runme.o", W + "/h.o"])
data = open(W + "/RunMe,ffa", "rb").read()
print("built RunMe,ffa: %d bytes" % len(data))

class Done(Exception):
    def __init__(self, kind, **kw): self.kind = kind; self.kw = kw

def fresh():
    """a new machine with the module loaded and initialised"""
    k = RiscosModel(max_steps=3_000_000)
    mod = k.load(data, 0x01C21000)
    k.cpu.r[13] = k.sp0
    r0, v = k.init(mod)
    assert r0 == 0 and v == 0
    orig = k.swi_hook
    state = {"getenv_mode": None, "getenv_sp": None}
    def hook(cpu, swi):
        n = swi & ~0x20000
        if n == 0x10:                                                                   # OS_GetEnv
            state["getenv_mode"] = cpu.mode; state["getenv_regs"] = (cpu.r[12], cpu.r[13])
            cpu.r[0] = k.put_string("RMRun RunMe"); cpu.r[1] = 0x00800000; cpu.r[2] = k.put_string("\0\0\0\0\0"); cpu.v = 0
        elif n == 0x11: raise Done("exit", code=cpu.r[2], r0=cpu.r[0], r1=cpu.r[1], mode=cpu.mode)                  # OS_Exit
        elif n == 0x2B: raise Done("error", text=k.read_cstr(cpu.r[0] + 4), num=cpu.rd32(cpu.r[0]))              # OS_GenerateError
        else: orig(cpu, swi)
    k.cpu.swi_hook = hook
    return k, mod, state

def run_program(tail):
    """*RMRun RunMe <tail>: the start entry in USER mode"""
    k, mod, st = fresh()
    cpu = k.cpu
    start = mod.hdr[0]
    for i in range(16): cpu.r[i] = 0x55000000 + i                                      # junk everywhere, as a user mode program finds it
    cpu.r[0] = k.put_string(tail); cpu.r[12] = mod.pw; cpu.r[14] = 0xFFFF0000
    cpu.set_mode(0x10)
    mark = len(k.out)
    try:
        cpu.run(mod.base + start, 0xFFFF0000)
        res = Done("returned")
    except Done as d:
        res = d
    except Fault as f:
        res = Done("fault", text=str(f))
    return res, "".join(k.out[mark:]).replace("\n\r", "\n"), st, k, mod

print("the header")
k, mod, st = fresh()
check(mod.hdr[0] != 0 and mod.title == "RunMe", "word 0 is the start offset (%#x) and the title is %r" % (mod.hdr[0], mod.title))
check(sorted(c[0] for c in mod.commands) == ["RunMeExit", "RunMeHello"], "the commands are there too")
print()

print("*RMRun RunMe  (no arguments)")
res, out, st, k, mod = run_program("")
check(res.kind == "exit" and res.kw["code"] == 10 and res.kw["r1"] == 0x58454241 and res.kw["r0"] == 0, "OS_Exit with the code 10 and the ABEX word (%s)" % ({x: hex(y) for x, y in res.kw.items()} if res.kind == "exit" else res.kind + " " + str(res.kw)))
check(st["getenv_mode"] == 0x10, "OS_GetEnv was called in USER mode")
check(out == "argc=1\nargv[0]=<RunMe> (5)\nargv[argc]=null\natexit: bye\n", "the output: %r" % out)
print()

print('*RMRun RunMe a "b c" d')
res, out, st, k, mod = run_program('a "b c" d')
check(res.kind == "exit" and res.kw["code"] == 40, "exit code 40 (argc 4)")
check(out == 'argc=4\nargv[0]=<RunMe> (5)\nargv[1]=<a> (1)\nargv[2]=<b c> (3)\nargv[3]=<d> (1)\nargv[argc]=null\natexit: bye\n', "argv: %r" % out)
print()

print("the tail ends at a carriage return, extra blanks are skipped")
res, out, st, k, mod = run_program('  x   y \r garbage')
check(res.kind == "exit" and res.kw["code"] == 30 and "argv[2]=<y>" in out and "garbage" not in out, "argc 3, the text after the CR is not an argument: %r" % out[:70])
print()

print("exit (n), return n, abort ()")
res, out, st, k, mod = run_program("exit 9")
check(res.kind == "exit" and res.kw["code"] == 9 and out.endswith("atexit: bye\n"), "exit 9: code 9, the atexit function ran")
res, out, st, k, mod = run_program("exit")
check(res.kind == "exit" and res.kw["code"] == 7, "exit with no number: 7")
res, out, st, k, mod = run_program("return 12")
check(res.kind == "exit" and res.kw["code"] == 12 and out.endswith("atexit: bye\n"), "return 12 from main: code 12, the atexit function ran")
res, out, st, k, mod = run_program("abort")
check(res.kind == "exit" and res.kw["code"] == 134 and "atexit" not in out, "abort (): code 134, no atexit function")
print()

print("many arguments (more than the 40 that fit): the rest is dropped, nothing breaks")
res, out, st, k, mod = run_program(" ".join(str(i) for i in range(60)))
check(res.kind == "exit" and res.kw["code"] == 400 and "argv[39]=<38>" in out, "argc 40, exit code 400")
print()

print("the module as a module: *RunMeHello and *RunMeExit in SVC mode")
k, mod, st = fresh()
err, out = k.command(mod, "RunMeHello")
check(err is None and out.replace("\n\r", "\n") == "hello from RunMe (1)\n", "*RunMeHello: %r" % out)
try:
    k.command(mod, "RunMeExit 3")
    res = Done("returned")
except Done as d:
    res = d
check(res.kind == "error" and "exit (3) was called in a module" in res.kw["text"], "exit (3) in SVC mode is the error %r" % (res.kw.get("text") if res.kind == "error" else res.kind))
print()

print("after a program has run: the module is a module again (*RunMeExit is an error, the exit functions of the program are gone)")
res, out, st, k, mod = run_program("abort")                                                  # abort () does not run its atexit function: it stays registered unless the end of the program clears it
check(res.kind == "exit" and res.kw["code"] == 134, "the program ended with abort ()")
mark = len(k.out)
try:
    k.cpu.set_mode(0x13); k.cpu.r[13] = k.sp0
    err, out = k.command(mod, "RunMeExit 3")
    res = Done("returned")
except Done as d:
    res = d
check(res.kind == "error" and "exit (3) was called in a module" in res.kw["text"], "*RunMeExit 3 after the program: %s" % (res.kw.get("text") if res.kind == "error" else res.kind))
check("".join(k.out[mark:]).count("atexit: bye") == 1, "the command's own exit function ran once, the one of the aborted program is gone (%r)" % "".join(k.out[mark:]))
print()
print("%s" % ("ALL OK" if not fails else "%d FAILED" % fails))
sys.exit(1 if fails else 0)
