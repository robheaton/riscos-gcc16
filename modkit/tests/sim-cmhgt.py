#!/usr/bin/env python3
"""sim-cmhgt.py -- the hardware test module CmhgT (tests/hwpack/cmhgt.*, vecall.S, veneercall.S) on the A32 interpreter with the kernel model, before it goes to the machine: *CmhgT_Test must say
"0 FAILED" and every line must be ok (the model has no complaint).  The decoding code modules CmhgD and CmhgE are for the machine only: the model of the kernel does not call a module's SWI decoding code
(it reads the table), so they are only built here (and have to link).

TC = the tool chain (default: the work area's tc-cm, else tc-rel17); CMUNGE = the cmunge to use.  Exit status 0 = everything right."""
import os, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE); sys.path.insert(0, os.path.join(HERE, "..", "..", "modpoc")); sys.path.insert(0, os.path.expanduser("~/gccsdk-next/tools"))
from riscosmodel import RiscosModel

def find_tc():
    for r in (os.path.expanduser("~/gccsdk-next/tc-cm"), os.path.expanduser("~/gccsdk-next/tc-rel17")):
        if os.path.exists(os.path.join(r, "bin", "arm-riscos-gnueabihf-gcc")): return r
        if os.path.isdir(r):
            for d in sorted(os.listdir(r)):
                if os.path.exists(os.path.join(r, d, "bin", "arm-riscos-gnueabihf-gcc")): return os.path.join(r, d)
    return os.path.expanduser("~/gccsdk-next/env-f")
TC = os.environ.get("TC") or find_tc()
BIN = os.path.join(TC, "bin")
CC = os.path.join(BIN, "arm-riscos-gnueabihf-gcc"); MODRELOC = os.path.join(BIN, "arm-riscos-gnueabihf-modreloc")
CMUNGE = os.environ.get("CMUNGE") or os.path.join(BIN, "cmunge")
SRC = os.path.join(HERE, "hwpack")
W = tempfile.mkdtemp(prefix="cmhgt-")
fails = 0
def check(ok, what):
    global fails
    print("  %s  %s" % ("ok  " if ok else "FAIL", what))
    if not ok: fails += 1
def sh(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode: sys.exit("%s\n%s%s" % (" ".join(cmd), r.stdout[-2000:], r.stderr[-2000:]))
    return r

def build(name, cmhg, srcs, cflags=()):
    d = os.path.join(W, name); os.makedirs(d)
    sh([CMUNGE, "-tgcc", "-32bit", "-s", d + "/h.s", "-d", d + "/header.h", os.path.join(SRC, cmhg)])
    sh([CC, "-mmodule", "-c", d + "/h.s", "-o", d + "/h.o"])
    objs = [d + "/h.o"]
    for i, s in enumerate(srcs):
        o = d + "/o%d.o" % i
        sh([CC, "-mmodule", "-O2", "-std=gnu99", "-Wall", "-Wextra", "-I" + d] + list(cflags) + ["-c", os.path.join(SRC, s), "-o", o]); objs.append(o)
    sh([CC, "-mmodule", "-o", d + "/m.elf"] + objs)
    sh([MODRELOC, "-q", d + "/m.elf", d + "/m,ffa"])
    return open(d + "/m,ffa", "rb").read()

data = build("cmhgt", "cmhgt.cmhg", ["cmhgt.c", "veneercall.S", "vecall.S"])
print("built CmhgT,ffa: %d bytes" % len(data))
for name, cmhg, flags in (("cmhgd", "cmhgd.cmhg", []), ("cmhge", "cmhge.cmhg", ["-DPAIR"])):
    d = build(name, cmhg, ["cmhgd.c"], flags)
    print("built %s: %d bytes" % (name, len(d)))
for base in (0x01C21000, 0x02008040):
    k = RiscosModel(max_steps=300_000_000)
    mod = k.load(data, base); k.cpu.r[13] = k.sp0
    orig = k.swi_hook
    def hook(cpu, swi, orig=orig, k=k):
        if swi & 0xFFFF == 0x38 and swi & 0x20000 or swi == 0x38:                       # OS_SWINumberToString: r0 = number, r1 = buffer, r2 = size -> r2 = bytes used (the model of the kernel has no such SWI)
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
    check(r0 == 0 and v == 0, "CmhgT initialises at %#x" % base)
    m0 = len(k.out)
    try:
        err, out = k.command(mod, "CmhgT_Test")
    except Exception as ex:
        print("".join(k.out[m0:]).replace("\n\r", "\n")[-1500:]); raise
    out = out.replace("\n\r", "\n")
    bad = [l for l in out.split("\n") if l.startswith("  FAIL")]
    last = [l for l in out.split("\n") if l.startswith("CmhgT_Test:")]
    check(err is None and last and last[0].endswith(" 0 FAILED") and not bad, "*CmhgT_Test: %s" % (last[0] if last else repr(out[-200:])))
    for l in bad: print("      " + l)
    print("      %d lines, %d ok" % (len(out.split("\n")), out.count("  ok ")))
    check(not k.problems and not k.log, "no complaints of the model %s" % ((k.problems + k.log)[:3],))
if fails:
    print("%d FAILED" % fails); sys.exit(1)
print("ALL OK")
