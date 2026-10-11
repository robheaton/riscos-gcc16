#!/usr/bin/env python3
"""sim-errmod.py -- CMunge's error-identifiers: and error-chunk-base-number: (tests/hwpack/errmod.cmhg) on the A32 interpreter with the kernel model: the module is built with cmunge, gcc -mmodule and modreloc and
loaded at two addresses; *ErrMod_Fail n returns the error blocks of the module: the numbers are the base and the ones after it (a block with a number of its own does not use one of the sequence), the
messages are right, the blocks are word aligned and inside the image.  TC = the tool chain (default: the work area's tc-cm).  Exit status 0 = everything right."""
import os, shutil, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
from riscosmodel import RiscosModel
TC = os.environ.get("TC") or os.path.expanduser("~/gccsdk-next/tc-cm"); BIN = os.path.join(TC, "bin"); T = "arm-riscos-gnueabihf"
fails = 0
def check(ok, what):
    global fails
    print("  %s  %s" % ("ok  " if ok else "FAIL", what)); fails += not ok
W = tempfile.mkdtemp(prefix="errmod-")
def sh(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=W)
    if r.returncode: sys.exit("%s\n%s%s" % (" ".join(cmd), r.stdout[-1500:], r.stderr[-2500:]))
for f in ("errmod.cmhg", "errmod.c"): shutil.copy(os.path.join(HERE, "hwpack", f), W)
CC = os.path.join(BIN, T + "-gcc")
sh([os.path.join(BIN, "cmunge"), "-tgcc", "-32bit", "-p", "-d", "header.h", "-s", "header.s", "errmod.cmhg"])
sh([CC, "-mmodule", "-c", "header.s", "-o", "header.o"]); sh([CC, "-mmodule", "-O2", "-std=gnu99", "-Wall", "-Wextra", "-I.", "-c", "errmod.c", "-o", "errmod.o"])
sh([CC, "-mmodule", "-o", "ErrMod.elf", "header.o", "errmod.o"]); sh([os.path.join(BIN, T + "-modreloc"), "-q", "ErrMod.elf", "ErrMod,ffa"])
data = open(os.path.join(W, "ErrMod,ffa"), "rb").read()
want = [(0x838000, "The first error of ErrMod"), (0x838001, "The second error: a message with a number 2"), (0x838010, "The third error has a number of its own"), (0x838002, "The fourth error follows the second")]
for base in (0x01C21000, 0x02008040):
    k = RiscosModel(max_steps=50_000_000); mod = k.load(data, base); k.cpu.r[13] = k.sp0
    r0, v = k.init(mod); check(r0 == 0 and v == 0, "at %#x: initialisation returns no error" % base)
    for n, (num, msg) in enumerate(want):
        err, out = k.command(mod, "ErrMod_Fail %d" % n)
        p = k.cpu.r[0]
        got = k.read_cstr(p + 4) if err is not None else None
        check(err == num and got == msg and p % 4 == 0 and base <= p < base + len(data), "*ErrMod_Fail %d: error &%X \"%s\" (the block is at %#x, word aligned, in the image)" % (n, num, msg, p))
    k.final(mod)
    check(not k.problems and not k.log, "no complaints of the model %s" % ((k.problems + k.log)[:3],))
shutil.rmtree(W, ignore_errors=True)
print("ALL OK" if not fails else "%d FAILED" % fails); sys.exit(1 if fails else 0)
