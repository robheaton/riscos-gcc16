#!/usr/bin/env python3
"""sim-regvar.py REGVAR1.C CC [CC ...] -- report 19: run repro/regvar/regvar1.c's f () on the A32 interpreter, compiled by each compiler (-O2), once with the wrapper as it is in
UnixLib's os.h (a result in a local register variable, read after the asm) and once with -DFIXED (the rewritten wrapper: the result is copied out inside the asm).
f ("hello") must return strlen ("hello") + 7 = 12.  The libc functions that f () calls (strlen, malloc, memcpy, memset, free, abort) are SWI trampolines that the script models.
Prints one line per compiler and variant; exit status 0 when every FIXED variant returns 12 (the original variant is reported, not judged: it is the bug).
Each CC is a path to an arm-riscos-gnueabihf-gcc (hard-float EABI, the static image is linked with -nostdlib).  Needs: a32.py next to this script."""
import os, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from a32 import Elf, Fault, Cpu

STUBS = """\t.text
\t.arm
%s"""
NAMES = ["strlen", "malloc", "memcpy", "memset", "free", "abort"]

def build(cc, src, fixed, work):
    stubs = os.path.join(work, "stubs.s")
    open(stubs, "w").write(STUBS % "".join("\t.global %s\n%s:\tswi\t0x%x\n\tbx\tlr\n" % (n, n, 0x101 + i) for i, n in enumerate(NAMES)))
    exe = os.path.join(work, "t.elf")
    cmd = [cc, "-O2", "-DNO_MAIN", "-nostdlib", "-static", "-Wl,-Ttext=0x8000", "-Wl,--entry=0x8000", "-Wl,--unresolved-symbols=ignore-all", "-o", exe, src, stubs]
    if fixed: cmd.insert(2, "-DFIXED")
    subprocess.check_call(cmd, stderr=subprocess.DEVNULL)
    return Elf(exe)

def run(elf, text):
    cpu = Cpu(elf, max_steps=20000)
    STR, STACK = 0x300000, 0x200000
    for i, b in enumerate(text.encode() + b"\0"): cpu.wr8(STR + i, b)
    heap = [0x400000]
    def hook(c, swi):
        n = swi - 0x101
        if n == 0:                                   # strlen
            a = c.r[0]; k = 0
            while c.rd8(a + k): k += 1
            c.r[0] = k
        elif n == 1:                                 # malloc
            p = heap[0]; heap[0] += (c.r[0] + 15) & ~7; c.r[0] = p
        elif n == 2:                                 # memcpy (dst, src, n)
            d, s, k = c.r[0], c.r[1], c.r[2]
            for i in range(k): c.wr8(d + i, c.rd8(s + i))
        elif n == 3:                                 # memset (dst, c, n)
            d, v, k = c.r[0], c.r[1], c.r[2]
            for i in range(k): c.wr8(d + i, v & 255)
        elif n == 4: pass                            # free
        else: raise Fault("abort ()")
    cpu.swi_hook = hook
    cpu.r[0] = STR; cpu.r[13] = STACK; cpu.r[14] = 0xFFFF0000
    cpu.run(elf.syms["f"], 0xFFFF0000)
    return cpu.r[0]

def main():
    src, ccs = sys.argv[1], sys.argv[2:]
    bad = 0
    for cc in ccs:
        ver = subprocess.run([cc, "--version"], capture_output=True, text=True).stdout.split("\n")[0]
        for fixed in (False, True):
            work = tempfile.mkdtemp(prefix="regvar.")
            try:
                got = run(build(cc, src, fixed, work), "hello")
            except Exception as e:
                got = "error: %s" % e
            want = 12
            ok = got == want
            print("%-62s %-22s f (\"hello\") = %-6s %s" % (ver[:62], "FIXED wrapper" if fixed else "wrapper as in os.h", got, "ok" if ok else "WRONG (expected %d: arg_size was taken from r0 after the call of strlen)" % want))
            if fixed and not ok: bad += 1
    sys.exit(1 if bad else 0)
main()
