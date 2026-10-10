#!/usr/bin/env python3
"""sim-stackleft.py -- __modlib_stack_left () (lib/stackleft.c) on the A32 interpreter with the kernel model: the kernel's SVC stack is 32 KB below the top that OS_ReadSysInfo 6 gives; at a depth of N levels
of 1 KB frames about N KB less are left.
TC = the tool chain (default: the work area's tc-cm).  Exit status 0 = everything right."""
import os, re, shutil, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
from riscosmodel import RiscosModel
TC = os.environ.get("TC") or os.path.expanduser("~/gccsdk-next/tc-cm"); BIN = os.path.join(TC, "bin"); T = "arm-riscos-gnueabihf"
fails = 0
def check(ok, what):
    global fails
    print("  %s  %s" % ("ok  " if ok else "FAIL", what)); fails += not ok
W = tempfile.mkdtemp(prefix="stackleft-")
def sh(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=W)
    if r.returncode: sys.exit("%s\n%s%s" % (" ".join(cmd), r.stdout[-1500:], r.stderr[-2500:]))
for f in ("stackleft.cmhg", "stackleft.c"): shutil.copy(os.path.join(HERE, "stack", f), W)
CC = os.path.join(BIN, T + "-gcc")
sh([os.path.join(BIN, "cmunge"), "-tgcc", "-32bit", "-p", "-d", "header.h", "-s", "header.s", "stackleft.cmhg"])
sh([CC, "-mmodule", "-c", "header.s", "-o", "header.o"]); sh([CC, "-mmodule", "-O2", "-std=gnu99", "-Wall", "-Wextra", "-I.", "-c", "stackleft.c", "-o", "stackleft.o"])
sh([CC, "-mmodule", "-o", "StkLeft.elf", "header.o", "stackleft.o"]); sh([os.path.join(BIN, T + "-modreloc"), "-q", "StkLeft.elf", "StkLeft,ffa"])
data = open(os.path.join(W, "StkLeft,ffa"), "rb").read()
for base in (0x01C21000, 0x02008040):
    k = RiscosModel(max_steps=50_000_000); mod = k.load(data, base); k.cpu.r[13] = k.sp0; k.init(mod)
    err, out = k.command(mod, "StkLeft_Show 8"); m = re.search(r"top (-?\d+)\n.*deep (-?\d+)", out.replace("\n\r", "\n"), re.S)
    top, deep = (int(m.group(1)), int(m.group(2))) if m else (None, None)
    check(m is not None and 32000 < top <= 32768 - 0, "at %#x: %s bytes are left at the top of the command (of 32768)" % (base, top))
    check(m is not None and 8 * 1024 <= top - deep <= 12 * 1024, "8 levels of 1 KB frames use about 8 KB: %s left" % deep)
    k.final(mod)
shutil.rmtree(W, ignore_errors=True)
print("ALL OK" if not fails else "%d FAILED" % fails); sys.exit(1 if fails else 0)
